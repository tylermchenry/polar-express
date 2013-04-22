#include "backup-executor.h"

#include <iostream>

#include <cassert>

#include "boost/pool/object_pool.hpp"

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
      num_snapshots_generated_(0),
      scan_state_(ScanState::kNotStarted),
      strand_dispatcher_(
          AsioDispatcher::GetInstance()->NewStrandDispatcherStateMachine()),
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

  filesystem_scanner_->StartScan(
      root, kMaxPendingSnapshots / 2,
      CreateStrandCallback(
          bind(&BackupExecutor::AddNewPendingSnapshotPaths, this)));
  scan_state_ = ScanState::kInProgress;
}

int BackupExecutor::GetNumFilesProcessed() const {
  return num_finished_snapshot_state_machines_;
}

int BackupExecutor::GetNumSnapshotsGenerated() const {
  return num_snapshots_generated_;
}

void BackupExecutor::AddNewPendingSnapshotPaths() {
  vector<boost::filesystem::path> paths;
  if (filesystem_scanner_->GetPaths(&paths)) {
    filesystem_scanner_->ClearPaths();
    scan_state_ = ScanState::kWaitingToContinue;
    for (const boost::filesystem::path& path : paths) {
      pending_snapshot_paths_.push(path);
    }
    strand_dispatcher_->Post(
        bind(&BackupExecutor::RunNextSnapshotStateMachine, this));
  } else {
    scan_state_ = ScanState::kFinished;
  }
}

void BackupExecutor::RunNextSnapshotStateMachine() {
  if (num_running_snapshot_state_machines_ >= kMaxSimultaneousSnapshots ||
      pending_snapshot_paths_.empty()) {
    return;
  }
  
  boost::filesystem::path path = pending_snapshot_paths_.front();
  pending_snapshot_paths_.pop();
  
  SnapshotStateMachine* snapshot_state_machine =
      snapshot_state_machine_pool_->construct();
  snapshot_state_machine->SetDoneCallback(CreateStrandCallback(
      bind(&BackupExecutor::HandleSnapshotStateMachineFinished,
           this, snapshot_state_machine)));
  snapshot_state_machine->Start(root_, path);

  ++num_running_snapshot_state_machines_;

  // If the paths queue has drained below half of its maximum size, and the
  // filesystem scanner is waiting to scan more paths, tell it to do so.
  if (scan_state_ == ScanState::kWaitingToContinue &&
      (pending_snapshot_paths_.size() < kMaxPendingSnapshots / 2)) {
    filesystem_scanner_->ContinueScan(
        kMaxPendingSnapshots / 2,
        CreateStrandCallback(
            bind(&BackupExecutor::AddNewPendingSnapshotPaths, this)));
    scan_state_ = ScanState::kInProgress;
  } 
}
  
void BackupExecutor::HandleSnapshotStateMachineFinished(
    SnapshotStateMachine* snapshot_state_machine) {
  if (snapshot_state_machine->GetGeneratedSnapshot().get() != nullptr) {
    ++num_snapshots_generated_;
  }

  DeleteSnapshotStateMachine(snapshot_state_machine);
}

void BackupExecutor::DeleteSnapshotStateMachine(
    SnapshotStateMachine* snapshot_state_machine) {
  snapshot_state_machine_pool_->destroy(snapshot_state_machine);
  --num_running_snapshot_state_machines_;
  ++num_finished_snapshot_state_machines_;
  strand_dispatcher_->Post(
      bind(&BackupExecutor::RunNextSnapshotStateMachine, this));
}

Callback BackupExecutor::CreateStrandCallback(Callback callback) {
  return bind(&AsioDispatcher::StrandDispatcher::Post,
              strand_dispatcher_.get(), callback);
}  

}  // polar_express
