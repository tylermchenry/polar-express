#ifndef SNAPSHOT_STATE_MACHINE_H
#define SNAPSHOT_STATE_MACHINE_H

#include <iostream>
#include <string>

#include "boost/bind.hpp"
#include "boost/filesystem.hpp"
#include "boost/msm/back/state_machine.hpp"
#include "boost/msm/front/functor_row.hpp"
#include "boost/msm/front/state_machine_def.hpp"
#include "boost/shared_ptr.hpp"

#include "macros.h"
#include "callback.h"
#include "overrideable-scoped-ptr.h"

namespace polar_express {

class CandidateSnapshotGenerator;
class Snapshot;
class SnapshotStateMachine;

class SnapshotStateMachineImpl
    : public msm::front::state_machine_def<SnapshotStateMachine>
{
 public:
  typedef msm::back::state_machine<SnapshotStateMachineImpl> BackEnd;

  SnapshotStateMachineImpl();
  virtual ~SnapshotStateMachineImpl();

  virtual void SetDoneCallback(Callback done_callback);
  
  // Events
  struct NewFilePathReady {};
  struct CandidateSnapshotReady {};
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

  struct ExecuteDoneCallback {
    template <typename EventT, typename BackEndT,
              typename SourceStateT, typename TargetStateT>
    void operator()(const EventT&, BackEndT& back_end,
                    SourceStateT&, TargetStateT&) {
      back_end.HandleExecuteDoneCallback();
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
  
 protected:
  virtual BackEnd* GetBackEnd() = 0;
  
  void InternalStart(
    const string& root, const filesystem::path& filepath);

  template <typename EventT>
  Callback EventCallback() {
    return bind(&PostEvent<EventT, BackEnd>, EventT(), GetBackEnd());
  }

  template <typename EventT>
  void RaiseEvent() {
    EventCallback<EventT>()();
  }
  
  void HandleRequestGenerateCandidateSnapshot();
  void HandlePrintCandidateSnapshot();
  void HandleExecuteDoneCallback();
  
 private:
  Callback done_callback_;

  OverrideableScopedPtr<CandidateSnapshotGenerator>
  candidate_snapshot_generator_;

  string root_;
  filesystem::path filepath_;

  static void ExecuteQueuedEventsWrapper(BackEnd* back_end);
  static void PostNextEventCallback(BackEnd* back_end);

  template <typename EventT, typename BackEndT>
  static void PostEvent(const EventT& event, BackEndT* back_end) {
    back_end->enqueue_event(event);
    back_end->PostNextEventCallback(back_end);
  }
  
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
