#ifndef SNAPSHOT_STATE_MACHINE_H
#define SNAPSHOT_STATE_MACHINE_H

#include <iostream>
#include <string>

#include "boost/filesystem.hpp"
#include "boost/msm/back/state_machine.hpp"
#include "boost/msm/front/functor_row.hpp"
#include "boost/msm/front/state_machine_def.hpp"
#include "boost/shared_ptr.hpp"

#include "macros.h"
#include "overrideable-scoped-ptr.h"

namespace polar_express {

class CandidateSnapshotGenerator;
class Snapshot;
class SnapshotStateMachine;

class SnapshotStateMachineImpl
    : public msm::front::state_machine_def<SnapshotStateMachine>
{
 public:
  typedef boost::function<void(SnapshotStateMachine*)> DoneCallback;
  
  typedef msm::back::state_machine<
    SnapshotStateMachineImpl, DoneCallback> BackEnd;
   
  explicit SnapshotStateMachineImpl(DoneCallback done_callback);
  virtual ~SnapshotStateMachineImpl();
 
  // Events
  struct NewFilePathReady {
    string root_;
    filesystem::path filepath_;
  };
  struct CandidateSnapshotReady {
    boost::shared_ptr<Snapshot> candidate_snapshot_;
  };
  struct CleanUp {};
  
  // States
  struct WaitForNewFilePath : public msm::front::state<> {};
  struct WaitForCandidateSnapshot : public msm::front::state<> {};
  struct WaitForCleanUp : public msm::front::state<> {};
  struct Done : public msm::front::state<> {};
  typedef WaitForNewFilePath initial_state;
  
  // Transition actions
  struct RequestGenerateCandidateSnapshot {
    template <typename EventT, typename BackEndT,
              typename SourceStateT, typename TargetStateT>
    void operator()(const EventT& event, BackEndT& back_end,
                    SourceStateT&, TargetStateT&) {
      back_end.HandleRequestGenerateCandidateSnapshot(event, back_end);
    }
  };

  struct PrintCandidateSnapshot {
    template <typename EventT, typename BackEndT,
              typename SourceStateT, typename TargetStateT>
    void operator()(const EventT& event, BackEndT& back_end,
                    SourceStateT&, TargetStateT&) {
      back_end.HandlePrintCandidateSnapshot(event, back_end);
    }
  };

  struct ExecuteDoneCallback {
    template <typename EventT, typename BackEndT,
              typename SourceStateT, typename TargetStateT>
    void operator()(const EventT& event, BackEndT& back_end,
                    SourceStateT&, TargetStateT&) {
      back_end.HandleExecuteDoneCallback(event, back_end);
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
                     WaitForCleanUp,
                     SnapshotStateMachineImpl::PrintCandidateSnapshot,
                     msm::front::none>,
    msm::front::Row <WaitForCleanUp,
                     CleanUp,
                     Done,
                     SnapshotStateMachineImpl::ExecuteDoneCallback,
                     msm::front::none>
    > {};
 
  template <class FSM,class Event>
  void no_transition(Event const& e, FSM&,int state)
  {
    std::cerr << "no transition from state " << state
              << " on event " << typeid(e).name() << std::endl;
  }

  template <typename EventT, typename BackEndT>
  static void PostEvent(const EventT& event, BackEndT* back_end) {
    back_end->enqueue_event(event);
    back_end->PostNextEventCallback(back_end);
  }
  
 protected:
  virtual void InternalStart(
    const string& root, const filesystem::path& filepath, BackEnd* back_end);

  virtual void HandleRequestGenerateCandidateSnapshot(
      const NewFilePathReady& event, BackEnd& back_end);
  virtual void HandlePrintCandidateSnapshot(
      const CandidateSnapshotReady& event, BackEnd& back_end);
  virtual void HandleExecuteDoneCallback(
      const CleanUp& event, BackEnd& back_end);
  
 private:
  DoneCallback done_callback_;

  OverrideableScopedPtr<CandidateSnapshotGenerator>
  candidate_snapshot_generator_;

  static void ExecuteEventsCallback(BackEnd* back_end);
  static void PostNextEventCallback(BackEnd* back_end);
  
  DISALLOW_COPY_AND_ASSIGN(SnapshotStateMachineImpl);
};

class SnapshotStateMachine : public SnapshotStateMachineImpl::BackEnd {
 public:  
  SnapshotStateMachine(SnapshotStateMachineImpl::DoneCallback done_callback);
  
  virtual void Start(const string& root, const filesystem::path& filepath);
  
 private:
  DISALLOW_COPY_AND_ASSIGN(SnapshotStateMachine);
};


}  // namespace polar_express

#endif  // SNAPSHOT_STATE_MACHINE_H
