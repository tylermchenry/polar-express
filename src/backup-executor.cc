#include "backup-executor.h"

#include <iostream>

#include <cassert>

#include "boost/pool/object_pool.hpp"

#include "asio-dispatcher.h"
#include "filesystem-scanner.h"
#include "snapshot-state-machine.h"

namespace polar_express {

const int BackupExecutor::kMaxPendingSnapshots = 200;
const int BackupExecutor::kMaxSimultaneousSnapshots = 5;

BackupExecutor::BackupExecutor()
    : snapshot_state_machine_pool_(
        new boost::object_pool<SnapshotStateMachine>),
      num_running_snapshot_state_machines_(0),
      num_finished_snapshot_state_machines_(0),
      scan_state_(ScanState::kNotStarted),
      filesystem_scanner_(new FilesystemScanner) {
}

BackupExecutor::~BackupExecutor() {
}

void BackupExecutor::Start(const string& root) {
  // Ensure that the given root is reasonable, and that Start has not been
  // called twice.
  assert(root_.empty());
  assert(!root.empty());
  root_ = root;

  boost::mutex::scoped_lock lock(mu_);
  filesystem_scanner_->StartScan(
      root, kMaxPendingSnapshots / 2,
      bind(&BackupExecutor::AddNewPendingSnapshotPaths, this));
  scan_state_ = ScanState::kInProgress;
}

int BackupExecutor::GetNumFilesProcessed() const {
  boost::mutex::scoped_lock lock(mu_);
  return num_finished_snapshot_state_machines_;
}

void BackupExecutor::AddNewPendingSnapshotPaths() {
  vector<boost::filesystem::path> paths;
  if (filesystem_scanner_->GetPaths(&paths)) {
    filesystem_scanner_->ClearPaths();
    boost::mutex::scoped_lock lock(mu_);
    scan_state_ = ScanState::kWaitingToContinue;
    for (const boost::filesystem::path& path : paths) {
      pending_snapshot_paths_.push(path);
    }
    PostRunNextSnapshotStateMachine();
  } else {
    boost::mutex::scoped_lock lock(mu_);
    scan_state_ = ScanState::kFinished;
  }
}

void BackupExecutor::RunNextSnapshotStateMachine() {
  boost::mutex::scoped_lock lock(mu_);
  if (num_running_snapshot_state_machines_ >= kMaxSimultaneousSnapshots ||
      pending_snapshot_paths_.empty()) {
    return;
  }
  
  boost::filesystem::path path = pending_snapshot_paths_.front();
  pending_snapshot_paths_.pop();
  
  SnapshotStateMachine* snapshot_state_machine =
      snapshot_state_machine_pool_->construct();
  snapshot_state_machine->SetDoneCallback(
      bind(&BackupExecutor::PostDeleteSnapshotStateMachine,
           this, snapshot_state_machine));
  snapshot_state_machine->Start(root_, path);

  ++num_running_snapshot_state_machines_;

  if (scan_state_ == ScanState::kWaitingToContinue &&
      (pending_snapshot_paths_.size() < kMaxPendingSnapshots / 2)) {
    filesystem_scanner_->ContinueScan(
        kMaxPendingSnapshots / 2,
        bind(&BackupExecutor::AddNewPendingSnapshotPaths, this));
    scan_state_ = ScanState::kInProgress;
  } 
}

void BackupExecutor::PostRunNextSnapshotStateMachine() {
  AsioDispatcher::GetInstance()->PostStateMachine(
      bind(&BackupExecutor::RunNextSnapshotStateMachine, this));
}
  
void BackupExecutor::DeleteSnapshotStateMachine(
    SnapshotStateMachine* snapshot_state_machine) {
  boost::mutex::scoped_lock lock(mu_);
  snapshot_state_machine_pool_->destroy(snapshot_state_machine);
  --num_running_snapshot_state_machines_;
  ++num_finished_snapshot_state_machines_;
  PostRunNextSnapshotStateMachine();
}

void BackupExecutor::PostDeleteSnapshotStateMachine(
    SnapshotStateMachine* snapshot_state_machine) {
  AsioDispatcher::GetInstance()->PostStateMachine(
      bind(&BackupExecutor::DeleteSnapshotStateMachine,
           this, snapshot_state_machine));
}

}  // polar_express
