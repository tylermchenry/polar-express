#include "bundle-state-machine.h"

#include "proto/block.pb.h"
#include "proto/snapshot.pb.h"

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

void BundleStateMachineImpl::SetBundleReadyCallback(Callback callback) {
  bundle_ready_callback_ = callback;
}

void BundleStateMachineImpl::BundleSnapshot(
    boost::shared_ptr<Snapshot> snapshot) {
  PushPendingSnapshot(snapshot);
  PostEvent<NewSnapshotReady>();
}

void BundleStateMachineImpl::FinishAndExit() {
  exit_requested_ = true;
  PostEvent<NewSnapshotReady>();
}

void BundleStateMachineImpl::GetAndClearGeneratedBundles(
    vector<boost::shared_ptr<Bundle>>* bundles) {
  boost::mutex::scoped_lock bundles_lock(bundles_mu_);
  CHECK_NOTNULL(bundles)->swap(finished_bundles_);
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, StartNewSnapshot) {
  boost::shared_ptr<Snapshot> snapshot;
  if (PopPendingSnapshot(&snapshot)) {
    PushPendingChunksForSnapshot(snapshot);
  }
  NextChunk();
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, ResetForNextSnapshot) {
  // Nothing to do in current implementation.
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, GetExistingBundleInfo) {
}

PE_STATE_MACHINE_ACTION_HANDLER(
    BundleStateMachineImpl, InspectExistingBundleInfo) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, DiscardChunk) {
  NextChunk();
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, ReadChunkContents) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, CompressChunkContents) {
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, FinishChunk) {
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
  // Nothing to do in current implementation.
}

void BundleStateMachineImpl::InternalStart(const string& root) {
  root_ = root;
}

void BundleStateMachineImpl::PushPendingSnapshot(
    boost::shared_ptr<Snapshot> snapshot) {
  boost::mutex::scoped_lock snapshots_lock(snapshots_mu_);
  pending_snapshots_.push(snapshot);
}

bool BundleStateMachineImpl::PopPendingSnapshot(
    boost::shared_ptr<Snapshot>* snapshot) {
  boost::mutex::scoped_lock snapshots_lock(snapshots_mu_);
  if (pending_snapshots_.empty()) {
    return false;
  }
  *CHECK_NOTNULL(snapshot) = pending_snapshots_.front();
  pending_snapshots_.pop();
  return true;
}

void BundleStateMachineImpl::PushPendingChunksForSnapshot(
    boost::shared_ptr<Snapshot> snapshot) {
  const Chunk* chunk_ptr = nullptr;
  for (const auto& chunk : snapshot->chunks()) {
    chunk_ptr = &chunk;
    pending_chunks_.push(chunk_ptr);
  }
  if (chunk_ptr != nullptr) {
    last_chunk_to_snapshot_.insert(make_pair(chunk_ptr, snapshot));
  }
}

bool BundleStateMachineImpl::PopPendingChunk(const Chunk** chunk) {
  if (pending_chunks_.empty()) {
    return false;
  }
  *CHECK_NOTNULL(chunk) = pending_chunks_.front();
  pending_chunks_.pop();
  return true;
}

void BundleStateMachineImpl::NextChunk() {
  if (active_chunk_ != nullptr) {
    last_chunk_to_snapshot_.erase(active_chunk_);
    active_chunk_ = nullptr;
  }
  if (PopPendingChunk(&active_chunk_)) {
    PostEvent<NewChunkReady>();
  } else if (exit_requested_) {
    PostEvent<FlushForced>();
  } else {
    PostEvent<NoChunksRemaining>();
  }
}

void BundleStateMachineImpl::AppendFinishedBundle(
    boost::shared_ptr<Bundle> bundle) {
  boost::mutex::scoped_lock bundles_lock(bundles_mu_);
  finished_bundles_.push_back(bundle);
}

}  // namespace polar_express
