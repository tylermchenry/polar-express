#ifndef BACKUP_EXECUTOR_H
#define BACKUP_EXECUTOR_H

#include <queue>
#include <string>

#include "boost/filesystem.hpp"
#include "boost/pool/poolfwd.hpp"
#include "boost/scoped_ptr.hpp"
#include "boost/thread/mutex.hpp"

#include "macros.h"
#include "overrideable-scoped-ptr.h"

namespace polar_express {

class FilesystemScanner;
class SnapshotStateMachine;

class BackupExecutor {
 public:
  BackupExecutor();
  virtual ~BackupExecutor();

  virtual void Start(const string& root);

  virtual int GetNumFilesProcessed() const;
  
 private:
  void AddNewPendingSnapshotPaths();

  void RunNextSnapshotStateMachine();
  
  void PostRunNextSnapshotStateMachine();
  
  void DeleteSnapshotStateMachine(
      SnapshotStateMachine* snapshot_state_machine);

  void PostDeleteSnapshotStateMachine(
      SnapshotStateMachine* snapshot_state_machine);
 
  string root_;

  mutable boost::mutex mu_;
  queue<boost::filesystem::path> pending_snapshot_paths_ GUARDED_BY(mu_);
  boost::scoped_ptr<boost::object_pool<SnapshotStateMachine> >
  snapshot_state_machine_pool_ GUARDED_BY(mu_);
  int num_running_snapshot_state_machines_ GUARDED_BY(mu_);
  int num_finished_snapshot_state_machines_ GUARDED_BY(mu_);

  enum class ScanState {
    kNotStarted,
    kInProgress,
    kWaitingToContinue,
    kFinished,
  };        
  ScanState scan_state_ GUARDED_BY(mu_);
  
  OverrideableScopedPtr<FilesystemScanner> filesystem_scanner_;

  static const int kMaxPendingSnapshots;
  static const int kMaxSimultaneousSnapshots;

  DISALLOW_COPY_AND_ASSIGN(BackupExecutor);
};

}  // polar_express

#endif  // BACKUP_EXECUTOR_H
