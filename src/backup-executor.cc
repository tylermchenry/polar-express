#include "backup-executor.h"

#include <iostream>

#include <cassert>

#include "boost/pool/object_pool.hpp"

#include "bundle.h"
#include "bundle-state-machine.h"
#include "filesystem-scanner.h"
#include "make-unique.h"
#include "proto/bundle-manifest.pb.h"
#include "proto/snapshot.pb.h"
#include "snapshot-state-machine.h"

namespace polar_express {

const int BackupExecutor::kMaxPendingSnapshots = 200;
const int BackupExecutor::kMaxSimultaneousSnapshots = 5;
const int BackupExecutor::kMaxSnapshotsWaitingToBundle = 100;
const int BackupExecutor::kMaxSimultaneousBundles = 2;

BackupExecutor::BackupExecutor()
    : snapshot_state_machine_pool_(
        new boost::object_pool<SnapshotStateMachine>),
      num_running_snapshot_state_machines_(0),
      num_finished_snapshot_state_machines_(0),
      num_snapshots_generated_(0),
      num_bundles_generated_(0),
      scan_state_(ScanState::kNotStarted),
      strand_dispatcher_(
          AsioDispatcher::GetInstance()->NewStrandDispatcherStateMachine()),
      filesystem_scanner_(new FilesystemScanner) {
}

BackupExecutor::~BackupExecutor() {
}

void BackupExecutor::Start(
    const string& root,
    Cryptor::EncryptionType encryption_type,
    boost::shared_ptr<const CryptoPP::SecByteBlock> encryption_key) {
  // Ensure that the given root is reasonable, and that Start has not been
  // called twice.
  assert(root_.empty());
  assert(!root.empty());
  root_ = root;

  assert(encryption_key != nullptr);
  encryption_type_ = encryption_type;
  encryption_key_ = encryption_key;

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

int BackupExecutor::GetNumBundlesGenerated() const {
  return num_bundles_generated_;
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
  boost::shared_ptr<Snapshot> generated_snapshot =
      snapshot_state_machine->GetGeneratedSnapshot();
  if (generated_snapshot != nullptr) {
    ++num_snapshots_generated_;
    if (generated_snapshot->is_regular() &&
        generated_snapshot->length() > 0) {
      AddNewSnapshotToBundle(generated_snapshot);
    }
    // TODO: Handle non-regular files (directories, deletions, etc.)
  }

  DeleteSnapshotStateMachine(snapshot_state_machine);
}

void BackupExecutor::DeleteSnapshotStateMachine(
    SnapshotStateMachine* snapshot_state_machine) {
  snapshot_state_machine_pool_->destroy(snapshot_state_machine);
  --num_running_snapshot_state_machines_;
  ++num_finished_snapshot_state_machines_;
  if (snapshots_waiting_to_bundle_.size() < kMaxSnapshotsWaitingToBundle) {
    strand_dispatcher_->Post(
        bind(&BackupExecutor::RunNextSnapshotStateMachine, this));
  }
}

void BackupExecutor::AddNewSnapshotToBundle(
    boost::shared_ptr<Snapshot> snapshot) {
  snapshots_waiting_to_bundle_.push(snapshot);

  // If possible, activate a new state machine to handle this snapshot
  // right away.
  boost::shared_ptr<BundleStateMachine> activated_bundle_state_machine =
      TryActivateBundleStateMachine();
  if (activated_bundle_state_machine != nullptr) {
    BundleNextSnapshot(activated_bundle_state_machine);
  }
}

boost::shared_ptr<BundleStateMachine>
BackupExecutor::TryActivateBundleStateMachine() {
  boost::shared_ptr<BundleStateMachine> activated_bundle_state_machine;
  if (!idle_bundle_state_machines_.empty()) {
    activated_bundle_state_machine = idle_bundle_state_machines_.front();
    idle_bundle_state_machines_.pop();
  } else if (active_bundle_state_machines_.size() < kMaxSimultaneousBundles) {
    activated_bundle_state_machine.reset(new BundleStateMachine);
    activated_bundle_state_machine->SetSnapshotDoneCallback(
        CreateStrandCallback(
            bind(&BackupExecutor::BundleNextSnapshot,
                 this, activated_bundle_state_machine)));
    activated_bundle_state_machine->SetBundleReadyCallback(
        CreateStrandCallback(
            bind(&BackupExecutor::HandleBundleStateMachineBundleReady,
                 this, activated_bundle_state_machine)));
    activated_bundle_state_machine->SetDoneCallback(
        CreateStrandCallback(
            bind(&BackupExecutor::HandleBundleStateMachineFinished,
                 this, activated_bundle_state_machine)));
    activated_bundle_state_machine->Start(
        root_, encryption_type_, encryption_key_);
  }

  if (activated_bundle_state_machine != nullptr) {
    active_bundle_state_machines_.insert(activated_bundle_state_machine);
  }
  return activated_bundle_state_machine;
}

void BackupExecutor::BundleNextSnapshot(
    boost::shared_ptr<BundleStateMachine> bundle_state_machine) {
  assert(bundle_state_machine != nullptr);

  boost::shared_ptr<Snapshot> snapshot;
  if (!snapshots_waiting_to_bundle_.empty()) {
    snapshot = snapshots_waiting_to_bundle_.front();
    snapshots_waiting_to_bundle_.pop();
  }

  if (snapshot != nullptr) {
    bundle_state_machine->BundleSnapshot(snapshot);
  } else {
    active_bundle_state_machines_.erase(bundle_state_machine);
    idle_bundle_state_machines_.push(bundle_state_machine);
  }

  if (active_bundle_state_machines_.empty() &&
      snapshots_waiting_to_bundle_.empty() &&
      num_running_snapshot_state_machines_ == 0 &&
      scan_state_ == ScanState::kFinished) {
    FlushAllBundleStateMachines();
  }

  // If we just reduced the wait queue to below maximum, allow the
  // snapshot state machines to start running again.
  if (snapshots_waiting_to_bundle_.size() == kMaxSnapshotsWaitingToBundle - 1) {
    int snapshot_state_machines_to_start = min<int>(
        kMaxSimultaneousSnapshots - num_running_snapshot_state_machines_,
        pending_snapshot_paths_.size());
    for (int i = 0; i < snapshot_state_machines_to_start; ++i) {
      strand_dispatcher_->Post(
          bind(&BackupExecutor::RunNextSnapshotStateMachine, this));
    }
  }
}

void BackupExecutor::HandleBundleStateMachineBundleReady(
    boost::shared_ptr<BundleStateMachine> bundle_state_machine) {
  boost::shared_ptr<AnnotatedBundleData> bundle_data =
      bundle_state_machine->RetrieveGeneratedBundle();
  if (bundle_data != nullptr) {
    ++num_bundles_generated_;
    std::cerr << "Wrote bundle to: "
              << bundle_data->annotations().persistence_file_path()
              << std::endl;
  }
}

void BackupExecutor::HandleBundleStateMachineFinished(
    boost::shared_ptr<BundleStateMachine> bundle_state_machine) {
  active_bundle_state_machines_.erase(bundle_state_machine);
}

void BackupExecutor::FlushAllBundleStateMachines() {
  assert(active_bundle_state_machines_.empty());
  while (!idle_bundle_state_machines_.empty()) {
    boost::shared_ptr<BundleStateMachine> bundle_state_machine =
        idle_bundle_state_machines_.front();
    idle_bundle_state_machines_.pop();
    active_bundle_state_machines_.insert(bundle_state_machine);
    bundle_state_machine->FinishAndExit();
  }
}

Callback BackupExecutor::CreateStrandCallback(Callback callback) {
  return bind(&AsioDispatcher::StrandDispatcher::Post,
              strand_dispatcher_.get(), callback);
}

}  // polar_express
