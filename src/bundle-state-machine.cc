#include "bundle-state-machine.h"

#include "bundle.h"
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

void BundleStateMachineImpl::SetSnapshotDoneCallback(Callback callback) {
  snapshot_done_callback_ = callback;
}

void BundleStateMachineImpl::SetBundleReadyCallback(Callback callback) {
  bundle_ready_callback_ = callback;
}

void BundleStateMachineImpl::BundleSnapshot(
    boost::shared_ptr<Snapshot> snapshot) {
  assert(pending_snapshot_ == nullptr);
  pending_snapshot_ = snapshot;
  PostEvent<NewSnapshotReady>();
}

void BundleStateMachineImpl::FinishAndExit() {
  exit_requested_ = true;
  PostEvent<NewSnapshotReady>();
}

boost::shared_ptr<AnnotatedBundleData>
BundleStateMachineImpl::RetrieveGeneratedBundle() {
  boost::shared_ptr<AnnotatedBundleData> ret_bundle = generated_bundle_;
  generated_bundle_.reset();
  return ret_bundle;
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, StartNewSnapshot) {
  if (pending_snapshot_ != nullptr) {
    PushPendingChunksForSnapshot(pending_snapshot_);
  }
  NextChunk();
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, ResetForNextSnapshot) {
  assert(pending_chunks_.empty());
  pending_snapshot_.reset();
  if (snapshot_done_callback_) {
    snapshot_done_callback_();
  }
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
  assert(active_bundle_ != nullptr);
  assert(!active_bundle_->is_finalized());
  if (active_bundle_->size() > 0) {
    active_bundle_->Finalize();
    PostEvent<BundleReady>();
  } else {
    PostEvent<BundleEmpty>();
  }
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, EncryptBundle) {
  assert(active_bundle_ != nullptr);
  assert(active_bundle_->is_finalized());
  assert(generated_bundle_ == nullptr);

  generated_bundle_.reset(new AnnotatedBundleData);
  generated_bundle_->mutable_manifest()->CopyFrom(active_bundle_->manifest());

  const vector<char>& plaintext = active_bundle_->data();
  string* cyphertext = generated_bundle_->mutable_raw_data();

  // TODO: Actually encrypt! :)
  cyphertext->assign(plaintext.begin(), plaintext.end());

  // Get rid of plaintext.
  active_bundle_.reset();
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

void BundleStateMachineImpl::PushPendingChunksForSnapshot(
    boost::shared_ptr<Snapshot> snapshot) {
  const Chunk* chunk_ptr = nullptr;
  for (const auto& chunk : snapshot->chunks()) {
    chunk_ptr = &chunk;
    pending_chunks_.push(chunk_ptr);
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
  if (PopPendingChunk(&active_chunk_)) {
    PostEvent<NewChunkReady>();
  } else if (exit_requested_) {
    PostEvent<FlushForced>();
  } else {
    PostEvent<NoChunksRemaining>();
  }
}

}  // namespace polar_express
