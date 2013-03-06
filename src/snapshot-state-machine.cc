#include "snapshot-state-machine.h"

#include <iostream>

#include "boost/asio.hpp"
#include "boost/thread/mutex.hpp"

#include "candidate-snapshot-generator.h"
#include "proto/snapshot.pb.h"

namespace polar_express {
namespace {
mutex output_mutex;

void GenerateCandidateSnapshotCallback(
    boost::shared_ptr<asio::io_service> io_service,
    SnapshotStateMachine::BackEnd* back_end,
    boost::shared_ptr<Snapshot> candidate_snapshot) {
  SnapshotStateMachine::CandidateSnapshotReady event;
  event.candidate_snapshot_ = candidate_snapshot;
  back_end->enqueue_event(event);
}

}  // namespace

SnapshotStateMachine::SnapshotStateMachine(
    boost::shared_ptr<asio::io_service> io_service,
    SnapshotStateMachineImpl::DoneCallback done_callback)
    : SnapshotStateMachineImpl::BackEnd(io_service, done_callback) {
}

void SnapshotStateMachine::Start(
    const string& root, const filesystem::path& filepath) {
  InternalStart(root, filepath, this);
}

SnapshotStateMachineImpl::SnapshotStateMachineImpl(
    boost::shared_ptr<asio::io_service> io_service,
    DoneCallback done_callback)
    : io_service_(io_service),
      done_callback_(done_callback),
      candidate_snapshot_generator_(new CandidateSnapshotGenerator) {
}

SnapshotStateMachineImpl::~SnapshotStateMachineImpl() {
}

void SnapshotStateMachineImpl::HandleRequestGenerateCandidateSnapshot(
    const NewFilePathReady& event, BackEnd& back_end) {
  candidate_snapshot_generator_->GenerateCandidateSnapshot(
      event.root_, event.filepath_,
      bind(&GenerateCandidateSnapshotCallback, io_service_, &back_end, _1));
}

void SnapshotStateMachineImpl::HandlePrintCandidateSnapshot(
    const CandidateSnapshotReady& event, BackEnd& back_end) {
  {
    unique_lock<mutex> output_lock(output_mutex);
    cout << event.candidate_snapshot_->DebugString();
  }
  back_end.enqueue_event(CleanUp());
}

void SnapshotStateMachineImpl::HandleExecuteDoneCallback(
    const CleanUp&, BackEnd& back_end) {
  io_service_->post(bind(done_callback_,
                         dynamic_cast<SnapshotStateMachine*>(&back_end)));
}

void SnapshotStateMachineImpl::InternalStart(
    const string& root, const filesystem::path& filepath, BackEnd* back_end) {
  NewFilePathReady event;
  event.root_ = root;
  event.filepath_ = filepath;
  CHECK_NOTNULL(back_end)->enqueue_event(event);
  io_service_->post(
      bind(&SnapshotStateMachineImpl::ExecuteEventsCallback,
           io_service_, back_end));
}

// static
void SnapshotStateMachineImpl::ExecuteEventsCallback(
    boost::shared_ptr<asio::io_service> io_service, BackEnd* back_end) {
  CHECK_NOTNULL(back_end)->execute_queued_events();
  if (back_end->get_message_queue_size() > 0) {
    io_service->post(bind(
        &SnapshotStateMachine::ExecuteEventsCallback, io_service, back_end));    
  }
}

}  // namespace polar_express
