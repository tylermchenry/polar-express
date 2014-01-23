#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <iostream>
#include <queue>
#include <string>
#include <typeinfo>

#include <boost/bind.hpp>
#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/shared_ptr.hpp>

#include "base/asio-dispatcher.h"
#include "base/macros.h"
#include "base/callback.h"

namespace polar_express {

// This template class and following macros allow definition of a simple and
// easy-to-read state machine based on boost::msm. Only a small subset of the
// functionality in boost::msm is available through this interface, but it
// suffices for everything we want to do in Polar Express. This interface also
// makes use of the AsioDispatcher singleton, and provides a Done callback that
// is automatically invoked when the state machine can execute no further
// actions.
//
// == Conceptual overview:
//
// First, a set of states are defined, then a set of events and a set of
// actions. Then a transition table is defined. The transition table is a list
// of instructions of the form "When in state X, and event E occurs, perform
// action A and go to state Y." Then handlers are implemented for each possible
// action.
//
// This interface does not support boost::msm features such as hooks for state
// entry/exit, does not support events carrying data, and does not support
// actions being explicitly aware of which states they are occuring between.
//
// This simplified interface serializes actions in an ASIO strand so that no two
// action handlers will ever execute simultaneously. Therefore, no explicit
// internal synchronization is required.
//
// == How to define a state machine:
//
// To define a state machine FooStateMachine, first forward-declare
//
//   class FooStateMachine;
//
// Then, define an implementation, derived from this class
//
//   class FooStateMachineImpl
//     : public StateMachine<FooStateMachineImpl, FooStateMachine> {
//    /* ... */
//   };
//
// Then define FooStateMachine to derive from FooStateMachineImpl::BackEnd,
// which must implement the protected method GetBackEnd(), returning itself
//
//   class FooStateMachine : public FooStateMachineImpl::BackEnd {
//     protected:
//       virtual FooStateMachineImpl::BackEnd* GetBackEnd() { return this; }
//     /* ... */
//   };
//
// Then, in FooStateMachineImpl, use the macros below to define the states,
// events, actions, and transition table. You must use the DEFINE_INITIAL_STATE
// and TRANSITION_TABLE macros exactly once, but all others may be repeated as
// many time as necessary. The DEFINE_INITIAL_STATE macro must be used in the
// public section of the class, but all others may be protected.
//
// Finally, in the .cc file corresponding to FooStateMachineImpl, just the
// ACTION_HANDLER macro to implement the handler code for each of the defined
// actions.
//
// == Triggering Events:
//
// In this simplified model, all events are triggered internally. An action
// handler may trigger an event directly by invoking PostEvent<EventName>().
//
// If there is an external class which accepts a callback parameter, and this
// callback should trigger an event, create such a callback by invoking
// CreateExternalEventCallback<EventName>(). This ensures both that the action
// handler triggered by the event runs in the state machine's strand, and allows
// the state machine to do the record-keeping necessary to determine when to
// execute the Done callback. Note that once an external callback is created, it
// MUST be executed at some point, otherwise the state machine's Done callback
// will never be invoked.
//
// The mechanism for starting the the state machine (by triggering an initial
// event) is not prescribed by this class. Normally, the way to do this is to
// give FooStateMachine a Start() method which invokes PostEvent for some
// event which yields a transition away from the initial state.
//
// == How this class works (internals):
//
// Because boost:msm does not allow execution of a single queued event, this
// class maintains its own event queue. Each time an event is added to the
// queue (via PostEvent), it posts a task to run RunNextEvent to the state
// machine's ASIO strand. RunNextEvent executes exactly one queued event.
//
// External event callbacks are essentially callbacks to PostEvent with a
// special notation that they are external (a boolean argument). The state
// machine keeps count of how many outstanding external callbacks there are,
// incrementing this count whenever one is created, and decrementing it when one
// is executed.
//
// When RunNextEvent finishes running an event, if there are no queued events
// and no outstanding external event callbacks, it knows that the state machine
// can make no further progress and invokes the Done callback.
//
// "Events" in the event queue are actually callbacks which will execute
// boost::msm's process_event method with the appropriate arguments on the
// backend object for the state machine.
template <typename StateMachineImplT, typename StateMachineT>
class StateMachine
    : public msm::front::state_machine_def<StateMachineT> {
 public:
  typedef msm::back::state_machine<StateMachineImplT> BackEnd;

  // Sets a callback to be invoked when the state machine can make no further
  // progress. Inside this callback it is safe to delete the state machine.
  virtual void SetDoneCallback(Callback done_callback);

  template <typename EventT, typename BackEndT> void no_transition(
      const EventT& event, BackEndT&, int state);

 protected:
  StateMachine();
  virtual ~StateMachine();

  // The back end class for a state machine using this base class must implement
  // this method to return itself.
  virtual BackEnd* GetBackEnd() = 0;

  // Call this method with an appropriate template argument to trigger a new
  // event. The event will enter the event queue and will be executed after any
  // previously-triggered events.
  template <typename EventT> void PostEvent();

  // Call this method with an appropriate template argument to create a callback
  // which will trigger a new event when invoked. This callback may be given to
  // an external object. When the callback is invoked the event will enter the
  // event queue and will be executed after any previously triggered events.
  //
  // The state machine will not consider itself done (and so will not invoke its
  // Done callback) until all callbacks created using this method have been
  // executed.
  template <typename EventT> Callback CreateExternalEventCallback();

  // Sets an idle flag that prevents the done callback from being run when there
  // are no more events left to execute. The idle flag is automatically cleared
  // on the next EXTERNAL event.
  void SetIdle(bool is_idle = true);
  bool IsIdle() const;

 private:
  boost::shared_ptr<AsioDispatcher::StrandDispatcher> event_strand_dispatcher_;

  int num_active_external_callbacks_;
  queue<Callback> events_queue_;
  Callback done_callback_;
  bool idle_;

  // Runs the next event in the event queue (i.e. call process_event on the back
  // end with the event's type). If is_external is true, then the count of
  // outstanding external event callbacks is decremented.
  //
  // If after the event finishes running there are no further events or
  // outstanding external callbacks, the Done callback is invoked.
  void RunNextEvent(bool is_external);

  // Creates a callback which will run process_event on the back end object
  // using EventT as the template parameter.
  template <typename EventT> Callback CreateEventCallback();

  // Pushes callback into the event queue, and posts a task to the strand which
  // will invoke RunNextEvent with the is_external argument given.
  void PostEventCallback(Callback callback, bool is_external);

  DISALLOW_COPY_AND_ASSIGN(StateMachine);
};

#define PE_STATE_MACHINE_DEFINE_EVENT(EventName) \
  struct EventName {}

#define PE_STATE_MACHINE_DEFINE_STATE(StateName) \
  struct StateName : public msm::front::state<> {}

#define PE_STATE_MACHINE_DEFINE_INITIAL_STATE(StateName)           \
  PE_STATE_MACHINE_DEFINE_STATE(StateName);                        \
  typedef StateName initial_state

#define PE_STATE_MACHINE_ACTION_HANDLER_NAME(ActionName) \
  Handle ## ActionName

#define PE_STATE_MACHINE_DEFINE_ACTION(ActionName)                 \
  struct ActionName {                                              \
    template <typename EventT, typename BackEndT,                  \
              typename SourceStateT, typename TargetStateT>        \
    void operator()(const EventT&, BackEndT& back_end,             \
                    SourceStateT&, TargetStateT&) {                \
      back_end.PE_STATE_MACHINE_ACTION_HANDLER_NAME(ActionName)(); \
    }                                                              \
  };                                                               \
  void PE_STATE_MACHINE_ACTION_HANDLER_NAME(ActionName)();         \

#define PE_STATE_MACHINE_ACTION_HANDLER(                           \
    StateMachineName, ActionName)                                  \
  void StateMachineName::PE_STATE_MACHINE_ACTION_HANDLER_NAME(     \
      ActionName)()

#define PE_STATE_MACHINE_TRANSITION(                               \
    StartStateName, EventName, ActionName, EndStateName)           \
  msm::front::Row <StartStateName,                                 \
                   EventName,                                      \
                   EndStateName,                                   \
                   ActionName,                                     \
                   msm::front::none>

#define PE_STATE_MACHINE_TRANSITION_TABLE(...) \
  struct transition_table : mpl::vector<__VA_ARGS__> {}

template <typename StateMachineImplT, typename StateMachineT>
StateMachine<StateMachineImplT, StateMachineT>::StateMachine()
    : event_strand_dispatcher_(
        AsioDispatcher::GetInstance()->NewStrandDispatcherStateMachine()),
      num_active_external_callbacks_(0),
      idle_(false) {
}

template <typename StateMachineImplT, typename StateMachineT>
StateMachine<StateMachineImplT, StateMachineT>::~StateMachine() {
}

template <typename StateMachineImplT, typename StateMachineT>
void StateMachine<StateMachineImplT, StateMachineT>::SetDoneCallback(
    Callback done_callback) {
  done_callback_ = done_callback;
}

template <typename StateMachineImplT, typename StateMachineT>
template <typename EventT, typename BackEndT>
void StateMachine<StateMachineImplT, StateMachineT>::no_transition(
    const EventT& event, BackEndT&, int state) {
  DLOG(std::cerr << "no transition from state " << state << " on event "
                 << typeid(event).name() << std::endl);
}

template <typename StateMachineImplT, typename StateMachineT>
template <typename EventT>
void StateMachine<StateMachineImplT, StateMachineT>::PostEvent() {
  PostEventCallback(CreateEventCallback<EventT>(), false);
}

template <typename StateMachineImplT, typename StateMachineT>
template <typename EventT>
Callback
StateMachine<StateMachineImplT, StateMachineT>::CreateExternalEventCallback() {
  ++num_active_external_callbacks_;
  return bind(
      &StateMachine<StateMachineImplT, StateMachineT>::PostEventCallback,
      this, CreateEventCallback<EventT>(), true);
}

template <typename StateMachineImplT, typename StateMachineT>
void StateMachine<StateMachineImplT, StateMachineT>::SetIdle(bool is_idle) {
  idle_ = is_idle;
}

template <typename StateMachineImplT, typename StateMachineT>
bool StateMachine<StateMachineImplT, StateMachineT>::IsIdle() const {
  return idle_;
}

template <typename StateMachineImplT, typename StateMachineT>
void StateMachine<StateMachineImplT, StateMachineT>::RunNextEvent(
    bool is_external) {
  if (is_external) {
    idle_ = false;
    if (num_active_external_callbacks_ > 0) {
      --num_active_external_callbacks_;
    }
  }

  if (!events_queue_.empty()) {
    Callback next_event_callback = events_queue_.front();
    events_queue_.pop();
    next_event_callback();
  }

  // If there are no more pending events, then the state machine is
  // finished, so invoke the done callback (if present).
  if ((num_active_external_callbacks_ == 0) &&
      events_queue_.empty() &&
      !idle_ &&
      !done_callback_.empty()) {
    done_callback_();
  }
}

template <typename StateMachineImplT, typename StateMachineT>
template <typename EventT>
Callback StateMachine<StateMachineImplT, StateMachineT>::CreateEventCallback() {
  return bind(&StateMachineT::template process_event<EventT>,
              GetBackEnd(), EventT());
}

template <typename StateMachineImplT, typename StateMachineT>
void StateMachine<StateMachineImplT, StateMachineT>::PostEventCallback(
    Callback callback, bool is_external) {
  events_queue_.push(callback);
  event_strand_dispatcher_->Post(
      bind(&StateMachine<StateMachineImplT, StateMachineT>::RunNextEvent,
           this, is_external));
}

}  // namespace polar_express

#endif  // STATE_MACHINE_H
