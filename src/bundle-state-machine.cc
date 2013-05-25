#include "bundle-state-machine.h"

namespace polar_express {

void BundleStateMachine::Start(const string& root) {
  InternalStart(root);
}

BundleStateMachineImpl::BackEnd* BundleStateMachine::GetBackEnd() {
  return this;
}

BundleStateMachineImpl::BundleStateMachineImpl() {
}

BundleStateMachineImpl::~BundleStateMachineImpl() {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, StartNewSnapshot) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, ResetForNextSnapshot) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, GetExistingBundleInfo) {
}

PE_STATE_MACHINE_ACTION_HANDLER(
    BundleStateMachineImpl, InspectExistingBundleInfo) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, DiscardBlock) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, ReadBlockContents) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, CompressBlockContents) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, FinishBlock) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, FinalizeBundle) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, EncryptBundle) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, RecordBundle) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, WriteBundle) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, StartNewBundle) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, CleanUp) {
}

void BundleStateMachineImpl::InternalStart(const string& root) {
  root_ = root;
}

}  // namespace polar_express
