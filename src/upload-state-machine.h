#ifndef UPLOAD_STATE_MACHINE_H
#define UPLOAD_STATE_MACHINE_H

#include <boost/shared_ptr.hpp>

#include "macros.h"
#include "state-machine.h"

namespace polar_express {

class AnnotatedBundleData;
class Bundle;
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
  // uploading.
  boost::shared_ptr<AnnotatedBundleData> GetLastUploadedBundle() const;

  // Instructs the state machine to exit once it finishes uploading
  // its current bundle.
  //
  // Once UploadBundle has been invoked at least once, it is only
  // legal to call this method in response to an invocation of the
  // BundleUploaded callback. It is illegal to call UploadBundle
  // after calling FinishAndExit, or to call this method twice.
  void FinishAndExit();

  PE_STATE_MACHINE_DEFINE_INITIAL_STATE(WaitForNewBundle);

 protected:
  PE_STATE_MACHINE_TRANSITION_TABLE();

  // TODO: Add parameters.
  void InternalStart();

 private:
  Callback bundle_uploaded_callback_;
  bool exit_requested_;

  DISALLOW_COPY_AND_ASSIGN(UploadStateMachineImpl);
};

class UploadStateMachine : public UploadStateMachineImpl::BackEnd {
 public:
  UploadStateMachine() {}

  // TODO: Add parameters.
  virtual void Start();

 protected:
  virtual UploadStateMachineImpl::BackEnd* GetBackEnd();

 private:
  DISALLOW_COPY_AND_ASSIGN(UploadStateMachine);
};

}  // namespace polar_express

#endif  // UPLOAD_STATE_MACHINE_H

