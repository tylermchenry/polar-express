#include "bundle-state-machine.h"

#include "make-unique.h"

namespace polar_express {

void BundleStateMachine::Start(const string& root) {
  InternalStart(root);
}

BundleStateMachineImpl::BackEnd* BundleStateMachine::GetBackEnd() {
  return this;
}

BundleStateMachineImpl::BundleStateMachineImpl()
    : exit_requested_(false) {
}

BundleStateMachineImpl::~BundleStateMachineImpl() {
}

void BundleStateMachineImpl::BundleSnapshot(
    boost::shared_ptr<Snapshot> snapshot, Callback done_callback) {
  PushPendingSnapshot(snapshot, done_callback);
  PostEvent<NewSnapshotReady>();
}

void BundleStateMachineImpl::FinishAndExit(Callback exit_callback) {
  exit_callback_ = exit_callback;
  exit_requested_ = true;
  PostEvent<NewSnapshotReady>();
}

void BundleStateMachineImpl::GetAndClearGeneratedBundles(
    vector<boost::shared_ptr<Bundle>>* bundles) {
  boost::mutex::scoped_lock bundles_lock(bundles_mu_);
  CHECK_NOTNULL(bundles)->swap(finished_bundles_);
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

void BundleStateMachineImpl::PushPendingSnapshot(
    boost::shared_ptr<Snapshot> snapshot, Callback done_callback) {
  boost::mutex::scoped_lock snapshots_lock(snapshots_mu_);
  pending_snapshots_.push(
      make_unique<SnapshotBundlingInfo>(snapshot, done_callback));
}

bool BundleStateMachineImpl::PopPendingSnapshot(
    unique_ptr<SnapshotBundlingInfo>* snapshot) {
  boost::mutex::scoped_lock snapshots_lock(snapshots_mu_);
  if (pending_snapshots_.empty()) {
    return false;
  }
  *CHECK_NOTNULL(snapshot) = std::move(pending_snapshots_.front());
  pending_snapshots_.pop();
  return true;
}

void BundleStateMachineImpl::AppendFinishedBundle(
    boost::shared_ptr<Bundle> bundle) {
  boost::mutex::scoped_lock bundles_lock(bundles_mu_);
  finished_bundles_.push_back(bundle);
}

}  // namespace polar_express
