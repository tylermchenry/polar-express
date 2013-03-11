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

class BackupExecutor {
 public:
  BackupExecutor();
  virtual ~BackupExecutor();

  virtual void Start(const string& root);

  virtual int GetNumFilesProcessed() const;
  
 private:
  void AddNewPendingSnapshotPaths();

  void RunNextSnapshotStateMachine();
  
  void DeleteSnapshotStateMachine(
      SnapshotStateMachine* snapshot_state_machine);

  Callback CreateStrandCallback(Callback callback);
  
  string root_;

  queue<boost::filesystem::path> pending_snapshot_paths_;
  boost::scoped_ptr<boost::object_pool<SnapshotStateMachine> >
  snapshot_state_machine_pool_;
  int num_running_snapshot_state_machines_;
  int num_finished_snapshot_state_machines_;

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
