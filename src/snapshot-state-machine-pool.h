#ifndef SNAPSHOT_STATE_MACHINE_POOL_H
#define SNAPSHOT_STATE_MACHINE_POOL_H

#include <string>

#include "boost/shared_ptr.hpp"
#include "boost/filesystem.hpp"

#include "cryptor.h"
#include "macros.h"
#include "one-shot-state-machine-pool.h"

namespace polar_express {

class Snapshot;
class SnapshotStateMachine;

class SnapshotStateMachinePool : public OneShotStateMachinePool<
    SnapshotStateMachine, boost::filesystem::path> {
 public:
  SnapshotStateMachinePool(
      boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
      const string& root);

  virtual ~SnapshotStateMachinePool();

  void SetNextPool(boost::shared_ptr<StateMachinePool<Snapshot> > next_pool);

  void SetNeedMoreInputCallback(Callback callback);

  void NotifyInputFinished();

  int num_snapshots_generated() const;

  virtual const char* name() const {
    return "Snapshot State Machine Pool";
  }

 private:
  virtual size_t OutputWeightToBeAddedByInput(
      boost::shared_ptr<boost::filesystem::path> input) const;

  virtual bool IsExpectingMoreInput() const;

  virtual void RunInputOnStateMachine(
      boost::shared_ptr<boost::filesystem::path> input,
      SnapshotStateMachine* state_machine);

  virtual void HandleStateMachineFinishedInternal(
      SnapshotStateMachine* state_machine);

  const string root_;

  Callback need_more_input_callback_;
  bool input_finished_;
  int num_snapshots_generated_;
  boost::shared_ptr<StateMachinePool<Snapshot> > next_pool_;

  DISALLOW_COPY_AND_ASSIGN(SnapshotStateMachinePool);
};

}  // namespace polar_express

#endif   // SNAPSHOT_STATE_MACHINE_POOL_H
