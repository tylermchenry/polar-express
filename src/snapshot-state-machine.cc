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
    : candidate_snapshot_generator_(new CandidateSnapshotGenerator),
      active_external_callback_(false) {
}

SnapshotStateMachineImpl::~SnapshotStateMachineImpl() {
}

void SnapshotStateMachineImpl::SetDoneCallback(Callback done_callback) {
  done_callback_ = done_callback;
}

void SnapshotStateMachineImpl::WaitForDone() {
  boost::recursive_mutex::scoped_lock lock(events_mu_);
  while (!events_queue_.empty()) {
    events_queue_empty_.wait(lock);
  }
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

void SnapshotStateMachineImpl::RunNextEvent() {
  boost::recursive_mutex::scoped_lock lock(events_mu_);
  if (!events_queue_.empty()) {
    Callback next_event_callback = events_queue_.front();
    events_queue_.pop();
    next_event_callback();
  }

  // If the event did not add any new events, then the state machine is
  // finished, so signal this condition to wake up anything blocked on
  // WaitForDone.
  if (!active_external_callback_ && events_queue_.empty()) {
    if (!done_callback_.empty()) {
      done_callback_();
    }
    events_queue_empty_.notify_all();
  }
}

void SnapshotStateMachineImpl::PostEventCallback(Callback callback) {
  boost::recursive_mutex::scoped_lock lock(events_mu_);
  events_queue_.push(callback);
  AsioDispatcher::GetInstance()->PostStateMachine(
      bind(&SnapshotStateMachineImpl::RunNextEvent, this));
}

void SnapshotStateMachineImpl::PostEventCallbackExternal(Callback callback) {
  boost::recursive_mutex::scoped_lock lock(events_mu_);
  assert(active_external_callback_ == true);
  active_external_callback_ = false;
  PostEventCallback(callback);
}
  
}  // namespace polar_express
