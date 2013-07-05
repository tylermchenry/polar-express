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

class BundleStateMachine;
class FilesystemScanner;
class Snapshot;
class SnapshotStateMachine;

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
      boost::shared_ptr<const CryptoPP::SecByteBlock> encryption_key);

  // Returns the number of files processed during the backup. Should be called
  // only after the backup has completed.
  virtual int GetNumFilesProcessed() const;

  // Returns the number of new snapshots generated during the backup. Should be
  // called only after the backup has completed.
  virtual int GetNumSnapshotsGenerated() const;

  // Returns the number of bundles generated during the backup. Should
  // be called only after the backup has completed.
  virtual int GetNumBundlesGenerated() const;

 private:
  // Obtains new file paths from the directory scanner and enqueues them. Posts
  // a callback to try to start the next snapshot state machine. If the
  // directory scanner had no new file paths, it considers the scan to be
  // complete.
  void AddNewPendingSnapshotPaths();

  // Runs another snapshot state machine if possible, on the next queued
  // path. If after starting the state machine the path queue has dropped below
  // half maximum, and the directory scanner is in the "waiting to continue"
  // state, it tells the scanner to continue.
  void RunNextSnapshotStateMachine();

  void HandleSnapshotStateMachineFinished(
      SnapshotStateMachine* snapshot_state_machine);

  // Deletes the given snapshot state machine, counts it as finished, and posts
  // a callback to try to start the next snapshot state machine.
  void DeleteSnapshotStateMachine(
      SnapshotStateMachine* snapshot_state_machine);

  void AddNewSnapshotToBundle(boost::shared_ptr<Snapshot> snapshot);

  // Attempts to active a bundle state machine from the idle queue, or
  // to create a new one. If this is not possible (due to the limit on
  // the number of simultaneous bundle state machines) return null.
  boost::shared_ptr<BundleStateMachine> TryActivateBundleStateMachine();

  void BundleNextSnapshot(
      boost::shared_ptr<BundleStateMachine> bundle_state_machine);

  void HandleBundleStateMachineBundleReady(
      boost::shared_ptr<BundleStateMachine> bundle_state_machine);

  void HandleBundleStateMachineFinished(
      boost::shared_ptr<BundleStateMachine> bundle_state_machine);

  void FlushAllBundleStateMachines();

  // Returns a callback that will post the given callback on this object's
  // strand.
  Callback CreateStrandCallback(Callback callback);

  string root_;
  Cryptor::EncryptionType encryption_type_;
  boost::shared_ptr<const CryptoPP::SecByteBlock> encryption_key_;

  queue<boost::filesystem::path> pending_snapshot_paths_;
  unique_ptr<boost::object_pool<SnapshotStateMachine> >
  snapshot_state_machine_pool_;
  int num_running_snapshot_state_machines_;
  int num_finished_snapshot_state_machines_;
  int num_snapshots_generated_;

  queue<boost::shared_ptr<Snapshot> > snapshots_waiting_to_bundle_;
  queue<boost::shared_ptr<BundleStateMachine> > idle_bundle_state_machines_;
  set<boost::shared_ptr<BundleStateMachine> > active_bundle_state_machines_;
  int num_bundles_generated_;

  enum class ScanState {
    kNotStarted,
    kInProgress,
    kWaitingToContinue,
    kFinished,
  };
  ScanState scan_state_;

  boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher_;

  OverrideableUniquePtr<FilesystemScanner> filesystem_scanner_;

  static const int kMaxPendingSnapshots;
  static const int kMaxSimultaneousSnapshots;
  static const int kMaxSnapshotsWaitingToBundle;
  static const int kMaxSimultaneousBundles;

  DISALLOW_COPY_AND_ASSIGN(BackupExecutor);
};

}  // polar_express

#endif  // BACKUP_EXECUTOR_H
