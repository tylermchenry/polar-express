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
#include "boost/thread/recursive_mutex.hpp"

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
  virtual void WaitForDone();
  
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

  template <typename EventT> Callback CreateExternalEventCallback();
 
  void HandleRequestGenerateCandidateSnapshot();
  void HandlePrintCandidateSnapshot();
  
 private:
  Callback done_callback_;

  OverrideableScopedPtr<CandidateSnapshotGenerator>
  candidate_snapshot_generator_;

  string root_;
  filesystem::path filepath_;

  mutable boost::recursive_mutex events_mu_;
  bool active_external_callback_ GUARDED_BY(events_mu_);
  queue<Callback> events_queue_ GUARDED_BY(events_mu_);
  boost::condition_variable_any events_queue_empty_ GUARDED_BY(events_mu_);
  
  void RunNextEvent();

  template <typename EventT> Callback CreateEventCallback();
  
  template <typename EventT> void PostEvent();
  void PostEventCallback(Callback callback);
  void PostEventCallbackExternal(Callback callback);
  
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

template <typename EventT>
Callback SnapshotStateMachineImpl::CreateEventCallback() {
  return bind(&SnapshotStateMachine::process_event<EventT>,
              GetBackEnd(), EventT());
}
  
template <typename EventT>
void SnapshotStateMachineImpl::PostEvent() {
  PostEventCallback(CreateEventCallback<EventT>());
}

template <typename EventT>
Callback SnapshotStateMachineImpl::CreateExternalEventCallback() {
  boost::recursive_mutex::scoped_lock lock(events_mu_);
  assert(active_external_callback_ == false);
  active_external_callback_ = true;
  return bind(&SnapshotStateMachineImpl::PostEventCallbackExternal,
              this, CreateEventCallback<EventT>());
}

}  // namespace polar_express

#endif  // SNAPSHOT_STATE_MACHINE_H
