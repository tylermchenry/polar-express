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
  InternalStart(root, filepath, this);
}

  SnapshotStateMachineImpl::SnapshotStateMachineImpl()
    : candidate_snapshot_generator_(new CandidateSnapshotGenerator) {
}

SnapshotStateMachineImpl::~SnapshotStateMachineImpl() {
}

void SnapshotStateMachineImpl::SetDoneCallback(Callback done_callback) {
  done_callback_ = done_callback;
}
  
void SnapshotStateMachineImpl::HandleRequestGenerateCandidateSnapshot(
    const NewFilePathReady& event, BackEnd& back_end) {
  candidate_snapshot_generator_->GenerateCandidateSnapshot(
      event.root_, event.filepath_,
      bind(&PostEvent<decltype(CandidateSnapshotReady()), BackEnd>,
           CandidateSnapshotReady(), &back_end));
}

void SnapshotStateMachineImpl::HandlePrintCandidateSnapshot(
    const CandidateSnapshotReady& event, BackEnd& back_end) {
  boost::shared_ptr<Snapshot> candidate_snapshot =
      candidate_snapshot_generator_->GetGeneratedCandidateSnapshot();
  {
    unique_lock<mutex> output_lock(output_mutex);
    cout << candidate_snapshot->DebugString();
  }
  back_end.enqueue_event(CleanUp());
}

void SnapshotStateMachineImpl::HandleExecuteDoneCallback(
    const CleanUp&, BackEnd& back_end) {
  if (!done_callback_.empty()) {
    done_callback_();
  }
}

void SnapshotStateMachineImpl::InternalStart(
    const string& root, const filesystem::path& filepath, BackEnd* back_end) {
  NewFilePathReady event;
  event.root_ = root;
  event.filepath_ = filepath;
  CHECK_NOTNULL(back_end)->enqueue_event(event);
  PostNextEventCallback(back_end);
}
  
// static
void SnapshotStateMachineImpl::ExecuteEventsCallback(BackEnd* back_end) {
  CHECK_NOTNULL(back_end)->execute_queued_events();
  PostNextEventCallback(back_end);
}

// static
void SnapshotStateMachineImpl::PostNextEventCallback(BackEnd* back_end) {
  if (back_end->get_message_queue_size() > 0) {
    AsioDispatcher::GetInstance()->PostStateMachine(
        bind(&SnapshotStateMachineImpl::ExecuteEventsCallback, back_end));
  }
}
  
}  // namespace polar_express
