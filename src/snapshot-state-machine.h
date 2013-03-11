#ifndef SNAPSHOT_STATE_MACHINE_H
#define SNAPSHOT_STATE_MACHINE_H

#include <cassert>
#include <iostream>
#include <queue>
#include <string>
#include <typeinfo>

#include "boost/bind.hpp"
#include "boost/filesystem.hpp"
#include "boost/msm/back/state_machine.hpp"
#include "boost/msm/front/functor_row.hpp"
#include "boost/msm/front/state_machine_def.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/condition_variable.hpp"

#include "asio-dispatcher.h"
#include "macros.h"
#include "callback.h"
#include "overrideable-scoped-ptr.h"
#include "state-machine.h"

namespace polar_express {

class CandidateSnapshotGenerator;
class Snapshot;
class SnapshotStateMachine;

class SnapshotStateMachineImpl
  : public StateMachine<SnapshotStateMachineImpl, SnapshotStateMachine> {
 public:
  using StateMachine<SnapshotStateMachineImpl, SnapshotStateMachine>::BackEnd;
  
  SnapshotStateMachineImpl();
  virtual ~SnapshotStateMachineImpl();

  // Events
  struct NewFilePathReady {};
  struct CandidateSnapshotReady {};
  
  // States
  struct WaitForNewFilePath : public msm::front::state<> {};
  struct WaitForCandidateSnapshot : public msm::front::state<> {};
  struct Done : public msm::front::state<> {};
  typedef WaitForNewFilePath initial_state;
  
  // Transition actions
  struct RequestGenerateCandidateSnapshot {
    template <typename EventT, typename BackEndT,
              typename SourceStateT, typename TargetStateT>
    void operator()(const EventT& event, BackEndT& back_end,
                    SourceStateT&, TargetStateT&) {
      back_end.HandleRequestGenerateCandidateSnapshot();
    }
  };

  struct PrintCandidateSnapshot {
    template <typename EventT, typename BackEndT,
              typename SourceStateT, typename TargetStateT>
    void operator()(const EventT&, BackEndT& back_end,
                    SourceStateT&, TargetStateT&) {
      back_end.HandlePrintCandidateSnapshot();
    }
  };
  
  // Transition table
  struct transition_table : mpl::vector<
    msm::front::Row <WaitForNewFilePath,
                     NewFilePathReady,
                     WaitForCandidateSnapshot,
                     SnapshotStateMachineImpl::RequestGenerateCandidateSnapshot,
                     msm::front::none>,
    msm::front::Row <WaitForCandidateSnapshot,
                     CandidateSnapshotReady,
                     Done,
                     SnapshotStateMachineImpl::PrintCandidateSnapshot,
                     msm::front::none>
    > {};
  
 protected:
  void InternalStart(
    const string& root, const filesystem::path& filepath);

  void HandleRequestGenerateCandidateSnapshot();
  void HandlePrintCandidateSnapshot();
  
 private:
  OverrideableScopedPtr<CandidateSnapshotGenerator>
  candidate_snapshot_generator_;

  string root_;
  filesystem::path filepath_;
  
  DISALLOW_COPY_AND_ASSIGN(SnapshotStateMachineImpl);
};

class SnapshotStateMachine : public SnapshotStateMachineImpl::BackEnd {
 public:  
  SnapshotStateMachine() {}
  
  virtual void Start(const string& root, const filesystem::path& filepath);

 protected:
  virtual SnapshotStateMachineImpl::BackEnd* GetBackEnd();
  
 private:
  DISALLOW_COPY_AND_ASSIGN(SnapshotStateMachine);
};

}  // namespace polar_express

#endif  // SNAPSHOT_STATE_MACHINE_H
