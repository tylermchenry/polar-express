#include "backup-executor.h"

#include "bundle-state-machine-pool.h"
#include "filesystem-scanner.h"
#include "snapshot-state-machine-pool.h"
#include "upload-state-machine-pool.h"

namespace polar_express {
namespace {

// TODO: Should be configurable.
// Until configurable, must match value in chunk-hasher-impl.cc.
const size_t kBlockSizeBytes = 1024 * 1024;  // 1 MiB

}  // namespace

BackupExecutor::BackupExecutor()
    : scan_state_(ScanState::kNotStarted),
      strand_dispatcher_(
          AsioDispatcher::GetInstance()->NewStrandDispatcherStateMachine()),
      filesystem_scanner_(new FilesystemScanner),
      snapshot_state_machine_pool_max_weight_(0),
      buffered_paths_total_weight_(0),
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

  snapshot_state_machine_pool_max_weight_ =
      snapshot_state_machine_pool_->InputWeightRemaining();
  filesystem_scanner_->StartScan(
      root, snapshot_state_machine_pool_max_weight_ / 2,
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
  vector<std::pair<boost::filesystem::path, size_t> > paths_with_size;
  if (filesystem_scanner_->GetPathsWithFilesize(&paths_with_size)) {
    filesystem_scanner_->ClearPaths();
    scan_state_ = ScanState::kWaitingToContinue;
    for (const auto& path_with_size : paths_with_size) {
      TryAddSnapshotPathWithSize(path_with_size);
    }
  } else if (buffered_paths_with_weight_.empty()) {
    scan_state_ = ScanState::kFinished;
  } else {
    scan_state_ = ScanState::kFinishedButPathsBuffered;
  }
}

void BackupExecutor::AddBufferedSnapshotPaths() {
  const size_t initial_buffer_size = buffered_paths_with_weight_.size();
  for (size_t i = 0; i < initial_buffer_size; ++i) {
    const auto path_with_weight = buffered_paths_with_weight_.front();
    buffered_paths_with_weight_.pop();
    buffered_paths_total_weight_ -= path_with_weight.second;
    TryAddSnapshotPathWithWeight(path_with_weight);
  }
}

void BackupExecutor::TryAddSnapshotPathWithSize(
    const std::pair<boost::filesystem::path, size_t>& path_with_size) {
  const auto path = path_with_size.first;
  const size_t weight = WeightFromFilesize(path_with_size.second);
  TryAddSnapshotPathWithWeight(make_pair(path, weight));
}

void BackupExecutor::TryAddSnapshotPathWithWeight(
    const std::pair<boost::filesystem::path, size_t>& path_with_weight) {
  const auto path = path_with_weight.first;
  const size_t weight = path_with_weight.second;
  ++num_files_processed_;
  if (CHECK_NOTNULL(snapshot_state_machine_pool_)->CanAcceptNewInput(weight)) {
    // TODO: It might be nice if the StateMachinePool did not require inputs
    // to be shared pointers.
    snapshot_state_machine_pool_->AddNewInput(
        boost::shared_ptr<boost::filesystem::path>(
            new boost::filesystem::path(path)), weight);
  } else {
    buffered_paths_total_weight_ += weight;
    buffered_paths_with_weight_.push(path_with_weight);
  }
}

void BackupExecutor::TryScanMorePaths() {
  AddBufferedSnapshotPaths();
  if (scan_state_ == ScanState::kFinishedButPathsBuffered &&
      buffered_paths_with_weight_.empty()) {
    scan_state_ = ScanState::kFinished;
    return;
  }

  const size_t input_weight_remaining =
      CHECK_NOTNULL(snapshot_state_machine_pool_)->InputWeightRemaining();
  if (scan_state_ == ScanState::kWaitingToContinue &&
      input_weight_remaining >= 2) {
    filesystem_scanner_->ContinueScan(
        input_weight_remaining / 2,
        strand_dispatcher_->CreateStrandCallback(
            bind(&BackupExecutor::AddNewPendingSnapshotPaths, this)));
    scan_state_ = ScanState::kInProgress;
  }
}

size_t BackupExecutor::WeightFromFilesize(size_t filesize) const {
  // Weight is equal to the filesize, capped between 1 and max_weight / 2. The
  // upper bound is necessary to support very large files. If we allowed weights
  // larger than max_weight, these files would never be processed. If we allowed
  // files with max_weight / 2 < weight < max_weight, these would sit in the
  // buffer until the scanner was finished scanning everything else under the
  // root. If there were many such files, the buffer could get very large.
  return std::min<size_t>(snapshot_state_machine_pool_max_weight_ / 2,
                          std::max<size_t>(1, filesize));
}

}  // polar_express
