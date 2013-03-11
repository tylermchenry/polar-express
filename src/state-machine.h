#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <iostream>
#include <queue>
#include <string>
#include <typeinfo>

#include "boost/bind.hpp"
#include "boost/msm/back/state_machine.hpp"
#include "boost/msm/front/functor_row.hpp"
#include "boost/msm/front/state_machine_def.hpp"
#include "boost/shared_ptr.hpp"

#include "asio-dispatcher.h"
#include "macros.h"
#include "callback.h"

namespace polar_express {

template <typename StateMachineImplT, typename StateMachineT>
class StateMachine
    : public msm::front::state_machine_def<StateMachineT> {
 public:
  typedef msm::back::state_machine<StateMachineImplT> BackEnd;

  virtual void SetDoneCallback(Callback done_callback);

  template <typename EventT, typename BackEndT> void no_transition(
      const EventT& event, BackEndT&, int state);

 protected:
  StateMachine();
  virtual ~StateMachine();

  virtual BackEnd* GetBackEnd() = 0;

  template <typename EventT> void PostEvent();
  
  template <typename EventT> Callback CreateExternalEventCallback();

 private:
  boost::shared_ptr<AsioDispatcher::StrandDispatcher> event_strand_dispatcher_;

  int num_active_external_callbacks_;
  queue<Callback> events_queue_;
  Callback done_callback_;
  
  void RunNextEvent(bool is_external);

  template <typename EventT> Callback CreateEventCallback();
  
  void PostEventCallback(Callback callback, bool is_external);
  
  DISALLOW_COPY_AND_ASSIGN(StateMachine);
};

#define PE_STATE_MACHINE_DEFINE_EVENT(EventName) \
  struct Event {}

#define PE_STATE_MACHINE_DEFINE_STATE(StateName) \
  struct State : public msm::front::state<> {}

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

#define PE_STATE_MACHINE_TRANSITION(                               \
    StartStateName, EventName, EndStateName, ActionName)           \
  msm::front::Row <StartState,                                     \
                   EventName,                                      \
                   EndStateName,                                   \
                   ActionName,                                     \
                   msm::front::none>

template <typename StateMachineImplT, typename StateMachineT>
StateMachine<StateMachineImplT, StateMachineT>::StateMachine()
    : event_strand_dispatcher_(
        AsioDispatcher::GetInstance()->NewStrandDispatcherStateMachine()),
      num_active_external_callbacks_(0) {
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
  std::cerr << "no transition from state " << state
            << " on event " << typeid(event).name() << std::endl;
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
  return bind(&StateMachine<StateMachineImplT, StateMachineT>::PostEventCallback,
              this, CreateEventCallback<EventT>(), true);
}

template <typename StateMachineImplT, typename StateMachineT>
void StateMachine<StateMachineImplT, StateMachineT>::RunNextEvent(
    bool is_external) {
  if (is_external && num_active_external_callbacks_ > 0) {
    --num_active_external_callbacks_;
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
