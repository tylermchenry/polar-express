#ifndef BACKUP_EXECUTOR_H
#define BACKUP_EXECUTOR_H

#include <queue>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <crypto++/secblock.h>

#include "base/asio-dispatcher.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/overrideable-unique-ptr.h"
#include "services/cryptor.h"

namespace polar_express {

class AnnotatedBundleData;
class BundleStateMachinePool;
class FilesystemScanner;
class Snapshot;
class SnapshotStateMachinePool;
class UploadStateMachinePool;

// Class which coordinates the execution of a backup: scanning for files,
// creating snapshots for them, creating bundles for the new snapshots, and
// uploading the bundles.
//
// This class is not internally synchronized, but runs all of its asynchronous
// code in a single strand, so will never be executing in more than one thread
// simultaneously.
class BackupExecutor {
 public:
  BackupExecutor();
  virtual ~BackupExecutor();

  // Starts a new backup job at a given root path. This method returns
  // immediately as the backup tasks continue asynchronously.
  //
  // TODO: Maybe add a Done callback? The current design assumes that the caller
  // is subsequently going to call AsioDispatcher::WaitForFinish to determine
  // when the backup is done.
  virtual void Start(
      const string& root,
      Cryptor::EncryptionType encryption_type,
      boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data,
      const string& aws_region_name,
      const string& aws_access_key,
      const CryptoPP::SecByteBlock& aws_secret_key,
      const string& glacier_vault_name);

  // Returns the number and total size (in bytes) of files processed during the
  // backup. Should be called only after the backup has completed.
  virtual int GetNumFilesProcessed() const;
  virtual size_t GetSizeOfFilesProcessed() const;

  // Returns the number and total size (in bytes) of new snapshots generated
  // during the backup. Should be called only after the backup has completed.
  virtual int GetNumSnapshotsGenerated() const;
  virtual size_t GetSizeOfSnapshotsGenerated() const;

  // Returns the number and total size (in bytes) of bundles generated during
  // the backup. Should be called only after the backup has completed.
  virtual int GetNumBundlesGenerated() const;
  virtual size_t GetSizeOfBundlesGenerated() const;

  // Returns the number and total size (in bytes) of bundles uploaded during the
  // backup. Should be called only after the backup has completed.
  virtual int GetNumBundlesUploaded() const;
  virtual size_t GetSizeOfBundlesUploaded() const;

 private:
  // Snapshot-Generation methods:

  // Obtains new file paths from the directory scanner and enqueues them. Posts
  // a callback to try to start the next snapshot state machine. If the
  // directory scanner had no new file paths, it considers the scan to be
  // complete.
  void AddNewPendingSnapshotPaths();

  void AddBufferedSnapshotPaths();

  void TryAddSnapshotPathWithSize(
      const std::pair<boost::filesystem::path, size_t>& path_with_size);

  void TryAddSnapshotPathWithWeight(
      const std::pair<boost::filesystem::path, size_t>& path_with_weight);

  void TryScanMorePaths();

  size_t WeightFromFilesize(size_t filesize) const;

  enum class ScanState {
    kNotStarted,
    kInProgress,
    kWaitingToContinue,
    kFinishedButPathsBuffered,
    kFinished,
  };
  ScanState scan_state_;

  boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher_;

  OverrideableUniquePtr<FilesystemScanner> filesystem_scanner_;
  size_t snapshot_state_machine_pool_max_weight_;

  std::queue<std::pair<boost::filesystem::path, size_t> >
      buffered_paths_with_weight_;
  size_t buffered_paths_total_weight_;

  int num_files_processed_;
  size_t size_of_files_processed_;

  boost::shared_ptr<SnapshotStateMachinePool> snapshot_state_machine_pool_;
  boost::shared_ptr<BundleStateMachinePool> bundle_state_machine_pool_;
  boost::shared_ptr<UploadStateMachinePool> upload_state_machine_pool_;

  DISALLOW_COPY_AND_ASSIGN(BackupExecutor);
};

}  // namespace polar_express

#endif  // BACKUP_EXECUTOR_H
