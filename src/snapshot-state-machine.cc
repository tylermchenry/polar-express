#include "snapshot-state-machine.h"

#include <iostream>

#include "boost/thread/mutex.hpp"

#include "candidate-snapshot-generator.h"
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
    : candidate_snapshot_generator_(new CandidateSnapshotGenerator) {
}

SnapshotStateMachineImpl::~SnapshotStateMachineImpl() {
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, RequestGenerateCandidateSnapshot) {
  candidate_snapshot_generator_->GenerateCandidateSnapshot(
      root_, filepath_, CreateExternalEventCallback<CandidateSnapshotReady>());
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, PrintCandidateSnapshot) {
  boost::shared_ptr<Snapshot> candidate_snapshot =
      candidate_snapshot_generator_->GetGeneratedCandidateSnapshot();
  unique_lock<mutex> output_lock(output_mutex);
  cout << candidate_snapshot->DebugString();
}

void SnapshotStateMachineImpl::InternalStart(
    const string& root, const filesystem::path& filepath) {
  root_ = root;
  filepath_ = filepath;
  PostEvent<NewFilePathReady>();
}
  
}  // namespace polar_express
