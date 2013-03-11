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
    : candidate_snapshot_generator_(new CandidateSnapshotGenerator),
      event_strand_dispatcher_(
          AsioDispatcher::GetInstance()->NewStrandDispatcherStateMachine()),
      num_active_external_callbacks_(0) {
}

SnapshotStateMachineImpl::~SnapshotStateMachineImpl() {
}

void SnapshotStateMachineImpl::SetDoneCallback(Callback done_callback) {
  done_callback_ = done_callback;
}
  
void SnapshotStateMachineImpl::HandleRequestGenerateCandidateSnapshot() {
  candidate_snapshot_generator_->GenerateCandidateSnapshot(
      root_, filepath_, CreateExternalEventCallback<CandidateSnapshotReady>());
}

void SnapshotStateMachineImpl::HandlePrintCandidateSnapshot() {
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

void SnapshotStateMachineImpl::RunNextEvent(bool is_external) {
  if (is_external && num_active_external_callbacks_ > 0) {
    --num_active_external_callbacks_;
  }

  if (!events_queue_.empty()) {
    Callback next_event_callback = events_queue_.front();
    events_queue_.pop();
    next_event_callback();
  }

  // If the event did not add any new events, then the state machine is
  // finished, so signal this condition to wake up anything blocked on
  // WaitForDone.
  if ((num_active_external_callbacks_ == 0) &&
      events_queue_.empty() &&
      !done_callback_.empty()) {
    done_callback_();
  }
}

void SnapshotStateMachineImpl::PostEventCallback(
    Callback callback, bool is_external) {
  events_queue_.push(callback);
  event_strand_dispatcher_->Post(
      bind(&SnapshotStateMachineImpl::RunNextEvent, this, is_external));
}
  
}  // namespace polar_express
