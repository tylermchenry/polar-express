#include "snapshot-state-machine.h"

#include <iostream>

#include "boost/thread/mutex.hpp"

#include "candidate-snapshot-generator.h"
#include "chunk-hasher.h"
#include "proto/block.pb.h"
#include "proto/snapshot.pb.h"

namespace polar_express {
namespace {
mutex output_mutex;
}  // namespace

void SnapshotStateMachine::Start(
    const string& root, const filesystem::path& filepath) {
  InternalStart(root, filepath);
}

SnapshotStateMachineImpl::BackEnd* SnapshotStateMachine::GetBackEnd() {
  return this;
}

SnapshotStateMachineImpl::SnapshotStateMachineImpl()
    : candidate_snapshot_generator_(new CandidateSnapshotGenerator),
      chunk_hasher_(new ChunkHasher) {
}

SnapshotStateMachineImpl::~SnapshotStateMachineImpl() {
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, RequestGenerateCandidateSnapshot) {
  candidate_snapshot_generator_->GenerateCandidateSnapshot(
      root_, filepath_, CreateExternalEventCallback<CandidateSnapshotReady>());
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, GetCandidateSnapshot) {
  candidate_snapshot_ =
      candidate_snapshot_generator_->GetGeneratedCandidateSnapshot();
  if (candidate_snapshot_->is_regular() &&
      !candidate_snapshot_->is_deleted()) {
    PostEvent<NeedChunkHashes>();
  } else {
    PostEvent<ReadyToPrint>();
  }
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, RequestGenerateAndHashChunks) {
  candidate_snapshot_ =
      candidate_snapshot_generator_->GetGeneratedCandidateSnapshot();
  chunk_hasher_->GenerateAndHashChunks(
      filepath_, CreateExternalEventCallback<ChunkHashesReady>());
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, GetChunkHashes) {
  chunk_hasher_->GetGeneratedAndHashedChunks(&chunks_);
  PostEvent<ReadyToPrint>();
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, PrintCandidateSnapshotAndChunks) {
  unique_lock<mutex> output_lock(output_mutex);
  cout << candidate_snapshot_->DebugString();
  for (const auto& chunk : chunks_) {
    cout << chunk->DebugString();
  }
}

void SnapshotStateMachineImpl::InternalStart(
    const string& root, const filesystem::path& filepath) {
  root_ = root;
  filepath_ = filepath;
  PostEvent<NewFilePathReady>();
}
  
}  // namespace polar_express
