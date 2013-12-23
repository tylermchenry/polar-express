#include "backup-executor.h"

#include "bundle-state-machine-pool.h"
#include "filesystem-scanner.h"
#include "snapshot-state-machine-pool.h"
#include "upload-state-machine-pool.h"

namespace polar_express {

BackupExecutor::BackupExecutor()
    : scan_state_(ScanState::kNotStarted),
      strand_dispatcher_(
          AsioDispatcher::GetInstance()->NewStrandDispatcherStateMachine()),
      filesystem_scanner_(new FilesystemScanner),
      num_files_processed_(0) {
}

BackupExecutor::~BackupExecutor() {
}

void BackupExecutor::Start(
    const string& root,
    Cryptor::EncryptionType encryption_type,
    boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data,
    const string& aws_region_name,
    const string& aws_access_key,
    const CryptoPP::SecByteBlock& aws_secret_key,
    const string& glacier_vault_name) {
  // Ensure that the given root is reasonable, and that Start has not been
  // called twice.
  assert(!root.empty());
  assert(encryption_keying_data != nullptr);
  assert(snapshot_state_machine_pool_ == nullptr);
  assert(bundle_state_machine_pool_ == nullptr);
  assert(upload_state_machine_pool_ == nullptr);

  snapshot_state_machine_pool_.reset(
      new SnapshotStateMachinePool(strand_dispatcher_, root));

  bundle_state_machine_pool_.reset(new BundleStateMachinePool(
      strand_dispatcher_, root, encryption_type, encryption_keying_data,
      snapshot_state_machine_pool_));
  snapshot_state_machine_pool_->SetNextPool(bundle_state_machine_pool_);

  upload_state_machine_pool_.reset(new UploadStateMachinePool(
      strand_dispatcher_, aws_region_name, aws_access_key, aws_secret_key,
      glacier_vault_name, bundle_state_machine_pool_));
  bundle_state_machine_pool_->SetNextPool(upload_state_machine_pool_);

  snapshot_state_machine_pool_->SetNeedMoreInputCallback(
      strand_dispatcher_->CreateStrandCallback(
          bind(&BackupExecutor::TryScanMorePaths, this)));

  filesystem_scanner_->StartScan(
      root, snapshot_state_machine_pool_->InputSlotsRemaining() / 2,
      strand_dispatcher_->CreateStrandCallback(
          bind(&BackupExecutor::AddNewPendingSnapshotPaths, this)));
  scan_state_ = ScanState::kInProgress;
}

int BackupExecutor::GetNumFilesProcessed() const {
  return num_files_processed_;
}

int BackupExecutor::GetNumSnapshotsGenerated() const {
  return CHECK_NOTNULL(snapshot_state_machine_pool_)->num_snapshots_generated();
}

int BackupExecutor::GetNumBundlesGenerated() const {
  return CHECK_NOTNULL(bundle_state_machine_pool_)->num_bundles_generated();
}

int BackupExecutor::GetNumBundlesUploaded() const {
  return CHECK_NOTNULL(upload_state_machine_pool_)->num_bundles_uploaded();
}

void BackupExecutor::AddNewPendingSnapshotPaths() {
  vector<boost::filesystem::path> paths;
  if (filesystem_scanner_->GetPaths(&paths)) {
    filesystem_scanner_->ClearPaths();
    scan_state_ = ScanState::kWaitingToContinue;
    for (const boost::filesystem::path& path : paths) {
      ++num_files_processed_;
      assert(CHECK_NOTNULL(snapshot_state_machine_pool_)->CanAcceptNewInput());
      // TODO: It might be nice if the StateMachinePool did not require inputs
      // to be shared pointers.
      snapshot_state_machine_pool_->AddNewInput(
          boost::shared_ptr<boost::filesystem::path>(
              new boost::filesystem::path(path)));
    }
  } else {
    scan_state_ = ScanState::kFinished;
  }
}

void BackupExecutor::TryScanMorePaths() {
  if (scan_state_ == ScanState::kWaitingToContinue) {
    filesystem_scanner_->ContinueScan(
        CHECK_NOTNULL(snapshot_state_machine_pool_)->InputSlotsRemaining() / 2,
        strand_dispatcher_->CreateStrandCallback(
            bind(&BackupExecutor::AddNewPendingSnapshotPaths, this)));
    scan_state_ = ScanState::kInProgress;
  }
}

}  // polar_express
