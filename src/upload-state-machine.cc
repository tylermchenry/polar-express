#include "upload-state-machine.h"

namespace polar_express {

void UploadStateMachine::Start() {
  InternalStart();
}

UploadStateMachineImpl::BackEnd* UploadStateMachine::GetBackEnd() {
  return this;
}

UploadStateMachineImpl::UploadStateMachineImpl()
    : exit_requested_(false) {
}

UploadStateMachineImpl::~UploadStateMachineImpl() {
}

void UploadStateMachineImpl::SetBundleUploadedCallback(Callback callback) {
  bundle_uploaded_callback_ = callback;
}

void UploadStateMachineImpl::UploadBundle(
    boost::shared_ptr<AnnotatedBundleData> bundle) {
  // TODO: Implement.
}

void UploadStateMachineImpl::FinishAndExit() {
  exit_requested_ = true;
  // TODO: Implement.
}

boost::shared_ptr<AnnotatedBundleData>
UploadStateMachineImpl::GetLastUploadedBundle() const {
  // TODO: Implement.
  return boost::shared_ptr<AnnotatedBundleData>();
}

void UploadStateMachineImpl::InternalStart() {
  // TODO: Implement.
}

}  // namespace polar_express
