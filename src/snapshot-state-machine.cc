#include "snapshot-state-machine.h"

#include <iostream>

#include "candidate-snapshot-generator.h"
#include "chunk-hasher.h"
#include "metadata-db.h"
#include "proto/block.pb.h"
#include "proto/snapshot.pb.h"
#include "snapshot-util.h"

namespace polar_express {

void SnapshotStateMachine::Start(
    const string& root, const filesystem::path& filepath) {
  InternalStart(root, filepath);
}

SnapshotStateMachineImpl::BackEnd* SnapshotStateMachine::GetBackEnd() {
  return this;
}

SnapshotStateMachineImpl::SnapshotStateMachineImpl()
    : snapshot_util_(new SnapshotUtil),
      candidate_snapshot_generator_(new CandidateSnapshotGenerator),
      chunk_hasher_(new ChunkHasher),
      metadata_db_(new MetadataDb) {
}

SnapshotStateMachineImpl::~SnapshotStateMachineImpl() {
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, RequestGenerateCandidateSnapshot) {
  candidate_snapshot_generator_->GenerateCandidateSnapshot(
      root_, filepath_, &candidate_snapshot_,
      CreateExternalEventCallback<CandidateSnapshotReady>());
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, RequestPreviousSnapshot) {
  metadata_db_->GetLatestSnapshot(
      candidate_snapshot_->file(), previous_snapshot_,
      CreateExternalEventCallback<PreviousSnapshotReady>());
}
  
PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, InspectSnapshots) {
  if (!snapshot_util_->AllMetadataEqual(
          *candidate_snapshot_, *previous_snapshot_)) {
    PostEvent<NoUpdatesNecessary>();
  } else if (snapshot_util_->FileContentsEqual(
      *candidate_snapshot_, *previous_snapshot_)) {
    PostEvent<ReadyToRecord>();
  } else {
    PostEvent<NeedChunkHashes>();
  }
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, RequestGenerateAndHashChunks) {
  chunk_hasher_->GenerateAndHashChunks(
      filepath_, candidate_snapshot_,
      CreateExternalEventCallback<ChunkHashesReady>());
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, InspectChunkHashes) {
  PostEvent<ReadyToRecord>();
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, RecordCandidateSnapshot) {
  metadata_db_->RecordNewSnapshot(
      candidate_snapshot_,
      CreateExternalEventCallback<SnapshotRecorded>());
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, CleanUp) {
  // Nothing to do here yet.
}

void SnapshotStateMachineImpl::InternalStart(
    const string& root, const filesystem::path& filepath) {
  root_ = root;
  filepath_ = filepath;
  PostEvent<NewFilePathReady>();
}

}  // namespace polar_express
