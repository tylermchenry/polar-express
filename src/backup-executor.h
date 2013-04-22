#ifndef BACKUP_EXECUTOR_H
#define BACKUP_EXECUTOR_H

#include <queue>
#include <string>

#include "boost/filesystem.hpp"
#include "boost/pool/poolfwd.hpp"
#include "boost/scoped_ptr.hpp"
#include "boost/shared_ptr.hpp"

#include "asio-dispatcher.h"
#include "callback.h"
#include "macros.h"
#include "overrideable-scoped-ptr.h"

namespace polar_express {

class FilesystemScanner;
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
  virtual void Start(const string& root);

  // Returns the number of files processed during the backup. Should be called
  // only after the backup has completed.
  virtual int GetNumFilesProcessed() const;

  // Returns the number of new snapshots generated during the backup. Should be
  // called only after the backup has completed.
  virtual int GetNumSnapshotsGenerated() const;
  
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

  // Deletes the given snapshot state machine, counts it as finished, and posts
  // a callback to try to start the next snapshot state machine.
  void DeleteSnapshotStateMachine(
      SnapshotStateMachine* snapshot_state_machine);

  // Returns a callback that will post the given callback on this object's
  // strand.
  Callback CreateStrandCallback(Callback callback);
  
  string root_;

  queue<boost::filesystem::path> pending_snapshot_paths_;
  boost::scoped_ptr<boost::object_pool<SnapshotStateMachine> >
  snapshot_state_machine_pool_;
  int num_running_snapshot_state_machines_;
  int num_finished_snapshot_state_machines_;
  int num_snapshots_generated_;
  
  enum class ScanState {
    kNotStarted,
    kInProgress,
    kWaitingToContinue,
    kFinished,
  };        
  ScanState scan_state_;

  boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher_;
  
  OverrideableScopedPtr<FilesystemScanner> filesystem_scanner_;

  static const int kMaxPendingSnapshots;
  static const int kMaxSimultaneousSnapshots;

  DISALLOW_COPY_AND_ASSIGN(BackupExecutor);
};

}  // polar_express

#endif  // BACKUP_EXECUTOR_H
