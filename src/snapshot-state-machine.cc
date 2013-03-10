#include "snapshot-state-machine.h"

#include <iostream>

#include "boost/thread/mutex.hpp"

#include "asio-dispatcher.h"
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

void SnapshotStateMachineImpl::SetDoneCallback(Callback done_callback) {
  done_callback_ = done_callback;
}
  
void SnapshotStateMachineImpl::HandleRequestGenerateCandidateSnapshot() {
  candidate_snapshot_generator_->GenerateCandidateSnapshot(
      root_, filepath_, EventCallback<CandidateSnapshotReady>());
}

void SnapshotStateMachineImpl::HandlePrintCandidateSnapshot() {
  boost::shared_ptr<Snapshot> candidate_snapshot =
      candidate_snapshot_generator_->GetGeneratedCandidateSnapshot();
  {
    unique_lock<mutex> output_lock(output_mutex);
    cout << candidate_snapshot->DebugString();
  }
  RaiseEvent<CleanUp>();
}

void SnapshotStateMachineImpl::HandleExecuteDoneCallback() {
  if (!done_callback_.empty()) {
    done_callback_();
  }
}

void SnapshotStateMachineImpl::InternalStart(
    const string& root, const filesystem::path& filepath) {
  root_ = root;
  filepath_ = filepath;
  RaiseEvent<NewFilePathReady>();
}

// static
void SnapshotStateMachineImpl::ExecuteQueuedEventsWrapper(BackEnd* back_end) {
  if (back_end->get_message_queue_size() > 0) {
    back_end->execute_queued_events();
  }
}

// static
void SnapshotStateMachineImpl::PostNextEventCallback(BackEnd* back_end) {
  AsioDispatcher::GetInstance()->PostStateMachine(
      bind(&ExecuteQueuedEventsWrapper, back_end));
}

}  // namespace polar_express
