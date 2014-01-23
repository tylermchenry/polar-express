#ifndef UPLOAD_STATE_MACHINE_H
#define UPLOAD_STATE_MACHINE_H

#include <memory>
#include <string>

#include <boost/shared_ptr.hpp>
#include <crypto++/secblock.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/overrideable-unique-ptr.h"
#include "state_machines/state-machine.h"

namespace polar_express {

class AnnotatedBundleData;
class Bundle;
class MetadataDb;
class GlacierConnection;
class GlacierVaultDescription;
class UploadStateMachine;

// Just a skeleton for now.
class UploadStateMachineImpl
    : public StateMachine<UploadStateMachineImpl, UploadStateMachine> {
 public:
  UploadStateMachineImpl();
  virtual ~UploadStateMachineImpl();

  // Sets a callback to be invoked when the current bundle is finished being
  // uploaded, and a new bundle may be presented for uploading.
  //
  // After invoking this callback, the state machine will pause until another
  // bundle is presented.
  void SetBundleUploadedCallback(Callback callback);

  // Provides a new bundle to be uploaded.
  //
  // After the initial call to UploadBundle, additional calls may be
  // made only in response to the BundleUploaded callback being invoked.
  void UploadBundle(boost::shared_ptr<AnnotatedBundleData> bundle_data);

  // Returns the annotated bundle data for the last bundle that finished
  // uploading. The bundle will have been given an archive ID, which has been
  // recorded to the metadata database.
  //
  // It is only legal to call this method exactly once in response to
  // each invocation of the bundle ready callback.
  boost::shared_ptr<AnnotatedBundleData> RetrieveLastUploadedBundle();

  // Instructs the state machine to exit once it finishes uploading
  // its current bundle.
  //
  // Once UploadBundle has been invoked at least once, it is only
  // legal to call this method in response to an invocation of the
  // BundleUploaded callback. It is illegal to call UploadBundle
  // after calling FinishAndExit, or to call this method twice.
  void FinishAndExit();

  PE_STATE_MACHINE_DEFINE_INITIAL_STATE(WaitForConnection);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForVaultDescription);
  PE_STATE_MACHINE_DEFINE_STATE(HaveVaultDescription);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForVaultCreation);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForNewBundle);
  PE_STATE_MACHINE_DEFINE_STATE(ReadyToUpload);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForUploadToComplete);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForUploadToRecord);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForBundleToDelete);
  PE_STATE_MACHINE_DEFINE_STATE(BundleFinished);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForUpdatedBundleRetrieval);
  PE_STATE_MACHINE_DEFINE_STATE(Done);

 protected:
  PE_STATE_MACHINE_DEFINE_EVENT(ConnectionReady);
  PE_STATE_MACHINE_DEFINE_EVENT(ConnectionClosed);
  PE_STATE_MACHINE_DEFINE_EVENT(VaultDescriptionReady);
  PE_STATE_MACHINE_DEFINE_EVENT(VaultMissing);
  PE_STATE_MACHINE_DEFINE_EVENT(VaultCreated);
  PE_STATE_MACHINE_DEFINE_EVENT(VaultOk);
  PE_STATE_MACHINE_DEFINE_EVENT(NewBundlePending);
  PE_STATE_MACHINE_DEFINE_EVENT(NewBundleReady);
  PE_STATE_MACHINE_DEFINE_EVENT(NoBundlePending);
  PE_STATE_MACHINE_DEFINE_EVENT(UploadCompleted);
  PE_STATE_MACHINE_DEFINE_EVENT(UploadRecorded);
  PE_STATE_MACHINE_DEFINE_EVENT(BundleDeleted);
  PE_STATE_MACHINE_DEFINE_EVENT(BundleUploadedCallbackExecuted);
  PE_STATE_MACHINE_DEFINE_EVENT(UpdatedBundleRetrieved);
  PE_STATE_MACHINE_DEFINE_EVENT(FlushForced);

  PE_STATE_MACHINE_DEFINE_ACTION(ReopenConnection);
  PE_STATE_MACHINE_DEFINE_ACTION(GetVaultDescription);
  PE_STATE_MACHINE_DEFINE_ACTION(InspectVaultDescription);
  PE_STATE_MACHINE_DEFINE_ACTION(CreateVault);
  PE_STATE_MACHINE_DEFINE_ACTION(WaitForInput);
  PE_STATE_MACHINE_DEFINE_ACTION(InspectNextBundle);
  PE_STATE_MACHINE_DEFINE_ACTION(StartUpload);
  PE_STATE_MACHINE_DEFINE_ACTION(RecordUpload);
  PE_STATE_MACHINE_DEFINE_ACTION(DeleteBundle);
  PE_STATE_MACHINE_DEFINE_ACTION(ExecuteBundleUploadedCallback);
  PE_STATE_MACHINE_DEFINE_ACTION(CleanUp);

  PE_STATE_MACHINE_TRANSITION_TABLE(
      PE_STATE_MACHINE_TRANSITION(
          WaitForConnection,
          ConnectionReady,
          GetVaultDescription,
          WaitForVaultDescription),
      PE_STATE_MACHINE_TRANSITION(
          WaitForVaultDescription,
          ConnectionClosed,
          ReopenConnection,
          WaitForConnection),
      PE_STATE_MACHINE_TRANSITION(
          WaitForVaultDescription,
          VaultDescriptionReady,
          InspectVaultDescription,
          HaveVaultDescription),
      PE_STATE_MACHINE_TRANSITION(
          HaveVaultDescription,
          ConnectionClosed,
          ReopenConnection,
          WaitForConnection),
      PE_STATE_MACHINE_TRANSITION(
          HaveVaultDescription,
          VaultMissing,
          CreateVault,
          WaitForVaultCreation),
      PE_STATE_MACHINE_TRANSITION(
          WaitForVaultCreation,
          ConnectionClosed,
          ReopenConnection,
          WaitForConnection),
      PE_STATE_MACHINE_TRANSITION(
          WaitForVaultCreation,
          VaultCreated,
          GetVaultDescription,
          WaitForVaultDescription),
      PE_STATE_MACHINE_TRANSITION(
          HaveVaultDescription,
          VaultOk,
          InspectNextBundle,
          ReadyToUpload),
      PE_STATE_MACHINE_TRANSITION(
          ReadyToUpload,
          NoBundlePending,
          WaitForInput,
          WaitForNewBundle),
      PE_STATE_MACHINE_TRANSITION(
          ReadyToUpload,
          NewBundleReady,
          StartUpload,
          WaitForUploadToComplete),
      PE_STATE_MACHINE_TRANSITION(
          ReadyToUpload,
          FlushForced,
          CleanUp,
          Done),
      PE_STATE_MACHINE_TRANSITION(
          WaitForNewBundle,
          NewBundlePending,
          InspectNextBundle,
          ReadyToUpload),
      PE_STATE_MACHINE_TRANSITION(
          WaitForUploadToComplete,
          ConnectionClosed,
          ReopenConnection,
          WaitForConnection),
      PE_STATE_MACHINE_TRANSITION(
          WaitForUploadToComplete,
          UploadCompleted,
          RecordUpload,
          WaitForUploadToRecord),
      PE_STATE_MACHINE_TRANSITION(
          WaitForUploadToRecord,
          ConnectionClosed,
          ReopenConnection,
          WaitForConnection),
      PE_STATE_MACHINE_TRANSITION(
          WaitForUploadToRecord,
          UploadRecorded,
          DeleteBundle,
          WaitForBundleToDelete),
      PE_STATE_MACHINE_TRANSITION(
          WaitForBundleToDelete,
          BundleDeleted,
          ExecuteBundleUploadedCallback,
          WaitForUpdatedBundleRetrieval),
      PE_STATE_MACHINE_TRANSITION(
          WaitForUpdatedBundleRetrieval,
          UpdatedBundleRetrieved,
          InspectNextBundle,
          ReadyToUpload));

  void InternalStart(
      const string& aws_region_name,
      const string& aws_access_key,
      const CryptoPP::SecByteBlock& aws_secret_key,
      const string& glacier_vault_name);

 private:
  Callback bundle_uploaded_callback_;
  bool exit_requested_;

  OverrideableUniquePtr<MetadataDb> metadata_db_;
  OverrideableUniquePtr<GlacierConnection> glacier_connection_;

  string glacier_vault_name_;
  bool attempted_vault_creation_;
  bool vault_created_;
  std::unique_ptr<GlacierVaultDescription> vault_description_;

  boost::shared_ptr<AnnotatedBundleData> next_bundle_data_;
  boost::shared_ptr<AnnotatedBundleData> current_bundle_data_;

  DISALLOW_COPY_AND_ASSIGN(UploadStateMachineImpl);
};

class UploadStateMachine : public UploadStateMachineImpl::BackEnd {
 public:
  UploadStateMachine() {}

  virtual void Start(
      const string& aws_region_name,
      const string& aws_access_key,
      const CryptoPP::SecByteBlock& aws_secret_key,
      const string& glacier_vault_name);

 protected:
  virtual UploadStateMachineImpl::BackEnd* GetBackEnd();

 private:
  DISALLOW_COPY_AND_ASSIGN(UploadStateMachine);
};

}  // namespace polar_express

#endif  // UPLOAD_STATE_MACHINE_H

