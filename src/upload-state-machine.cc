#include "upload-state-machine.h"

#include <iostream>

#include "boost/filesystem.hpp"

#include "bundle.h"
#include "glacier-connection.h"
#include "metadata-db.h"
#include "proto/bundle-manifest.pb.h"
#include "proto/glacier.pb.h"

namespace polar_express {
namespace {
// TEMPORARY
const int kTestServerId = 1;
}  // namesapce

void UploadStateMachine::Start(const string& aws_region_name,
                               const string& aws_access_key,
                               const CryptoPP::SecByteBlock& aws_secret_key,
                               const string& glacier_vault_name) {
  InternalStart(aws_region_name, aws_access_key, aws_secret_key,
                glacier_vault_name);
}

UploadStateMachineImpl::BackEnd* UploadStateMachine::GetBackEnd() {
  return this;
}

UploadStateMachineImpl::UploadStateMachineImpl()
    : exit_requested_(false),
      metadata_db_(new MetadataDb),
      glacier_connection_(new GlacierConnection),
      attempted_vault_creation_(false),
      vault_created_(false),
      vault_description_(new GlacierVaultDescription) {
}

UploadStateMachineImpl::~UploadStateMachineImpl() {
}

void UploadStateMachineImpl::SetBundleUploadedCallback(Callback callback) {
  bundle_uploaded_callback_ = callback;
}

void UploadStateMachineImpl::UploadBundle(
    boost::shared_ptr<AnnotatedBundleData> bundle) {
  assert(next_bundle_data_ == nullptr);
  next_bundle_data_ = bundle;
  PostEvent<NewBundleReady>();
}

void UploadStateMachineImpl::FinishAndExit() {
  exit_requested_ = true;
  PostEvent<NewBundleReady>();
}

boost::shared_ptr<AnnotatedBundleData>
UploadStateMachineImpl::RetrieveLastUploadedBundle() {
  boost::shared_ptr<AnnotatedBundleData> ret_bundle = current_bundle_data_;
  PostEvent<UpdatedBundleRetrieved>();
  return ret_bundle;
}

PE_STATE_MACHINE_ACTION_HANDLER(UploadStateMachineImpl, GetVaultDescription) {
  assert(CHECK_NOTNULL(glacier_connection_)->is_open());

  // TODO: Error handling for the case where we fail to create a vault.
  assert(!attempted_vault_creation_ || vault_created_);

  bool success = glacier_connection_->DescribeVault(
      glacier_vault_name_, vault_description_.get(),
      CreateExternalEventCallback<VaultDescriptionReady>());

  assert(success);  // TODO: Error handling.
}

PE_STATE_MACHINE_ACTION_HANDLER(UploadStateMachineImpl,
                                InspectVaultDescription) {
  assert(CHECK_NOTNULL(glacier_connection_)->is_open());

  if (glacier_connection_->last_operation_succeeded() &&
      vault_description_->vault_name() == glacier_vault_name_) {
    PostEvent<VaultOk>();
  } else {
    PostEvent<VaultMissing>();
  }
}

PE_STATE_MACHINE_ACTION_HANDLER(UploadStateMachineImpl, CreateVault) {
  assert(CHECK_NOTNULL(glacier_connection_)->is_open());
  assert(!attempted_vault_creation_);

  attempted_vault_creation_ = true;
  bool success = glacier_connection_->CreateVault(
      glacier_vault_name_, &vault_created_,
      CreateExternalEventCallback<VaultCreated>());

  assert(success);  // TODO: Error handling.
}

PE_STATE_MACHINE_ACTION_HANDLER(UploadStateMachineImpl, WaitForInput) {
  SetIdle();
}

PE_STATE_MACHINE_ACTION_HANDLER(UploadStateMachineImpl, InspectNextBundle) {
  current_bundle_data_.reset();

  if (next_bundle_data_ != nullptr) {
    current_bundle_data_ = next_bundle_data_;
    next_bundle_data_.reset();
    PostEvent<NewBundleReady>();
  } else if (exit_requested_) {
    PostEvent<FlushForced>();
  } else {
    PostEvent<NoBundlePending>();
  }
}

PE_STATE_MACHINE_ACTION_HANDLER(UploadStateMachineImpl, StartUpload) {
  assert(CHECK_NOTNULL(glacier_connection_)->is_open());
  assert(CHECK_NOTNULL(current_bundle_data_)
             ->annotations().server_bundle_id().empty());

  glacier_connection_->UploadArchive(
      glacier_vault_name_,
      current_bundle_data_->file_contents(),
      current_bundle_data_->annotations().sha256_linear_digest(),
      current_bundle_data_->annotations().sha256_tree_digest(),
      current_bundle_data_->unique_filename(),
      current_bundle_data_->mutable_annotations()->mutable_server_bundle_id(),
      CreateExternalEventCallback<UploadCompleted>());
}

PE_STATE_MACHINE_ACTION_HANDLER(UploadStateMachineImpl, RecordUpload) {
  CHECK_NOTNULL(metadata_db_)
      ->RecordUploadedBundle(kTestServerId, current_bundle_data_,
                             CreateExternalEventCallback<UploadRecorded>());
}

PE_STATE_MACHINE_ACTION_HANDLER(UploadStateMachineImpl, DeleteBundle) {
  // This is a quick operation, so we can do it synchronously here without
  // defining a complex async object to handle it.
  boost::filesystem::remove(boost::filesystem::path(CHECK_NOTNULL(
      current_bundle_data_)->annotations().persistence_file_path()));
  current_bundle_data_->mutable_annotations()->clear_persistence_file_path();
  PostEvent<BundleDeleted>();
}

PE_STATE_MACHINE_ACTION_HANDLER(UploadStateMachineImpl,
                                ExecuteBundleUploadedCallback) {
  if (bundle_uploaded_callback_) {
    bundle_uploaded_callback_();
  }
  SetIdle();
}

PE_STATE_MACHINE_ACTION_HANDLER(UploadStateMachineImpl, CleanUp) {
  // Nothing to do in current implementation.
}

void UploadStateMachineImpl::InternalStart(
    const string& aws_region_name, const string& aws_access_key,
    const CryptoPP::SecByteBlock& aws_secret_key,
    const string& glacier_vault_name) {
  assert(!glacier_vault_name.empty());
  glacier_vault_name_ = glacier_vault_name;
  glacier_connection_->Open(AsioDispatcher::NetworkUsageType::kUplinkBound,
                            aws_region_name, aws_access_key, aws_secret_key,
                            CreateExternalEventCallback<ConnectionReady>());
}

}  // namespace polar_express
