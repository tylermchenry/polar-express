#ifndef BACKUP_EXECUTOR_H
#define BACKUP_EXECUTOR_H

#include <deque>
#include <memory>
#include <queue>
#include <set>
#include <string>

#include "boost/filesystem.hpp"
#include "boost/pool/poolfwd.hpp"
#include "boost/shared_ptr.hpp"
#include "crypto++/secblock.h"

#include "asio-dispatcher.h"
#include "callback.h"
#include "cryptor.h"
#include "macros.h"
#include "overrideable-unique-ptr.h"

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

  // Returns the number of files processed during the backup. Should be called
  // only after the backup has completed.
  virtual int GetNumFilesProcessed() const;

  // Returns the number of new snapshots generated during the backup. Should be
  // called only after the backup has completed.
  virtual int GetNumSnapshotsGenerated() const;

  // Returns the number of bundles generated during the backup. Should
  // be called only after the backup has completed.
  virtual int GetNumBundlesGenerated() const;

  // Returns the number of bundles uploaded during the backup. Should
  // be called only after the backup has completed.
  virtual int GetNumBundlesUploaded() const;

 private:
  // Snapshot-Generation methods:

  // Obtains new file paths from the directory scanner and enqueues them. Posts
  // a callback to try to start the next snapshot state machine. If the
  // directory scanner had no new file paths, it considers the scan to be
  // complete.
  void AddNewPendingSnapshotPaths();

  void TryScanMorePaths();

  enum class ScanState {
    kNotStarted,
    kInProgress,
    kWaitingToContinue,
    kFinished,
  };
  ScanState scan_state_;

  boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher_;

  OverrideableUniquePtr<FilesystemScanner> filesystem_scanner_;
  int num_files_processed_;

  boost::shared_ptr<SnapshotStateMachinePool> snapshot_state_machine_pool_;
  boost::shared_ptr<BundleStateMachinePool> bundle_state_machine_pool_;
  boost::shared_ptr<UploadStateMachinePool> upload_state_machine_pool_;

  DISALLOW_COPY_AND_ASSIGN(BackupExecutor);
};

}  // namespace polar_express

#endif  // BACKUP_EXECUTOR_H
