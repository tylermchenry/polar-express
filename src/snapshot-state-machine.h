#ifndef SNAPSHOT_STATE_MACHINE_H
#define SNAPSHOT_STATE_MACHINE_H

#include <boost/msm/front/state_machine_def.hpp>

#include "macros.h"

namespace polar_express {

class SnapshotStateMachine
    : public msm::front::state_machine_def<SnapshotStateMachine>
{
 public:
  SnapshotStateMachine();
  virtual ~SnapshotStateMachine();
  
  // Events
  struct NewFilePathReady {};
  struct CandidateSnapshotReady {};

  // States
  struct WaitForNewFilePath : public msm::front::state<> {};
  struct WaitForCandidateSnapshot : public msm::front::state<> {};

  // Transition actions
  virtual void RequestGenerateCandidateSnapshot(
      const NewFilePathReady& event);
  virtual void PrintCandidateSnapshot(
      const CandidateSnapshotReady& event);
  
  // Transition table
  struct transition_table : mpl::vector<
    a_row <WaitForNewFilePath,
           NewFilePathReady,
           WaitForCandidateSnapshot,
           &SnapshotStateMachine::RequestGenerateCandidateSnapshot>,
    a_row <WaitForCandidateSnapshot,
           CandidateSnapshotReady,
           WaitForNewFilePath,
           &SnapshotStateMachine::PrintCandidateSnapshot>
    > {};
  
 private:
  DISALLOW_COPY_AND_ASSIGN(SnapshotStateMachine);
};

}  // namespace polar_express

#endif  // SNAPSHOT_STATE_MACHINE_H
