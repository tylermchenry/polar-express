#include "upload-state-machine-pool.h"

#include "bundle.h"
#include "proto/bundle-manifest.pb.h"
#include "upload-state-machine.h"

namespace polar_express {
namespace {

const size_t kMaxBytesWaitingToUpload = 100 * (1 << 20);  // 100 MiB
const size_t kMaxSimultaneousUploads = 2;

}  // namespace

UploadStateMachinePool::UploadStateMachinePool(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    const string& aws_region_name,
    const string& aws_access_key,
    const CryptoPP::SecByteBlock& aws_secret_key,
    const string& vault_name,
    boost::shared_ptr<StateMachinePoolBase> preceding_pool)
    : PersistentStateMachinePool<UploadStateMachine, AnnotatedBundleData>(
          strand_dispatcher, kMaxBytesWaitingToUpload,
          kMaxSimultaneousUploads, preceding_pool),
      aws_region_name_(aws_region_name),
      aws_access_key_(aws_access_key),
      aws_secret_key_(aws_secret_key),
      vault_name_(vault_name),
      num_bundles_uploaded_(0) {
}

UploadStateMachinePool::~UploadStateMachinePool() {
}

void UploadStateMachinePool::SetNextPool(
    boost::shared_ptr<StateMachinePool<AnnotatedBundleData> > next_pool) {
  next_pool_ = next_pool;
  set_next_pool(next_pool);
}

int UploadStateMachinePool::num_bundles_uploaded() const {
  return num_bundles_uploaded_;
}

size_t UploadStateMachinePool::OutputWeightToBeAddedByInputInternal(
    boost::shared_ptr<AnnotatedBundleData> input) const {
  return 1;
}

void UploadStateMachinePool::StartNewStateMachine(
    boost::shared_ptr<UploadStateMachine> state_machine) {
  assert(state_machine != nullptr);

  state_machine->SetBundleUploadedCallback(CreateStrandCallback(bind(
      &UploadStateMachinePool::HandleBundleUploaded, this, state_machine)));
  state_machine->Start(
      aws_region_name_, aws_access_key_, aws_secret_key_, vault_name_);
}

void UploadStateMachinePool::RunInputOnStateMachine(
    boost::shared_ptr<AnnotatedBundleData> input,
    boost::shared_ptr<UploadStateMachine> state_machine) {
  DLOG(std::cerr << "Starting Upload of Bundle " << input->annotations().id()
                 << std::endl);
  state_machine->UploadBundle(input);
}

void UploadStateMachinePool::HandleBundleUploaded(
    boost::shared_ptr<UploadStateMachine> state_machine) {
  StateMachineProducedOutput(state_machine, 0);

  boost::shared_ptr<AnnotatedBundleData> uploaded_bundle_data =
      CHECK_NOTNULL(state_machine)->RetrieveLastUploadedBundle();
  if (uploaded_bundle_data != nullptr) {
    ++num_bundles_uploaded_;
    // Not a DLOG until we get a UI.
    std::cerr << "Bundle " << uploaded_bundle_data->annotations().id()
              << " uploaded and assigned server-side ID "
              << uploaded_bundle_data->annotations().server_bundle_id()
              << std::endl;
    if (next_pool_ != nullptr) {
      assert(next_pool_->CanAcceptNewInput());
      next_pool_->AddNewInput(uploaded_bundle_data);
    }
  }
  // The upload state machine keeps running to maintain a single
  // continuous TCP connection with Glacier.
  DeactivateStateMachineAndTryRunNext(state_machine);
}

}  // namespace polar_express
