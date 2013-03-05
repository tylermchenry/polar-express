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
  io_service->post(bind(
      &SnapshotStateMachine::ExecuteEventsCallback, back_end));
}

}  // namespace

SnapshotStateMachine::SnapshotStateMachine(
    boost::shared_ptr<asio::io_service> io_service)
    : io_service_(io_service),
      candidate_snapshot_generator_(new CandidateSnapshotGenerator) {
}

SnapshotStateMachine::~SnapshotStateMachine() {
}

void SnapshotStateMachine::Initialize(
    BackEnd* back_end, DoneCallback done_callback) {
  back_end_.reset(CHECK_NOTNULL(back_end));
  done_callback_ = done_callback;
}   

void SnapshotStateMachine::RequestGenerateCandidateSnapshot(
    const NewFilePathReady& event) {
  candidate_snapshot_generator_->GenerateCandidateSnapshot(
      event.root_, event.filepath_,
      bind(&GenerateCandidateSnapshotCallback,
           io_service_, back_end_.get(), _1));
}

void SnapshotStateMachine::PrintCandidateSnapshot(
    const CandidateSnapshotReady& event) {
  {
    unique_lock<mutex> output_lock(output_mutex);
    cout << event.candidate_snapshot_->DebugString();
  }
  back_end_->enqueue_event(CleanUp());
  io_service_->post(bind(
      &SnapshotStateMachine::ExecuteEventsCallback, back_end_.get()));
}

void SnapshotStateMachine::ExecuteDoneCallback(const CleanUp&) {
  done_callback_(back_end_.release());
}

// static
void SnapshotStateMachine::ExecuteEventsCallback(
    SnapshotStateMachine::BackEnd* back_end) {
  CHECK_NOTNULL(back_end)->execute_queued_events();
}

}  // namespace polar_express
