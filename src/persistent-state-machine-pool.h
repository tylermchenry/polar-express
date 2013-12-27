#ifndef PERSISTENT_STATE_MACHINE_POOL_H
#define PERSISTENT_STATE_MACHINE_POOL_H

#include <queue>
#include <set>

#include <boost/shared_ptr.hpp>

#include "macros.h"
#include "state-machine-pool.h"

namespace polar_express {

// A state machine pool for state machines that are persistent. That is, once
// started, they can be fed input indefinitely until explicitly terminated. The
// state machines will be terminated when there is no more work for them to do,
// that is, when none are active, and the input queue is empty, and the
// preceding pool (if any) is also idle.
//
// The StateMachineT type must provide a method SetDoneCallback(Callback) which
// has the normal StateMachine semantics, and a method FinishAndExit which
// causes the state machine to finish processing any pending input, and then
// terminate, invoking the done callback. There are no other restrictions on the
// properties of the template types.
//
// This is an abstract base class. There must be a specialization for each
// StateMachineT type which at least provides an appropriate wrapper for the
// call to the state machine type's Start method, and method to run inputs,
// since the start method will take parameters that depend on the state machine
// type and the method for running inputs may be named differently on different
// state machines. The derived class may also handle additional type-specific
// callbacks for the state machine.
template <typename StateMachineT, typename InputT>
class PersistentStateMachinePool : public StateMachinePool<InputT> {
 public:
  virtual ~PersistentStateMachinePool();

 protected:
  PersistentStateMachinePool(
      boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
      size_t max_pending_inputs, size_t max_simultaneous_state_machines,
      boost::shared_ptr<StateMachinePoolBase> preceding_pool =
          boost::shared_ptr<StateMachinePoolBase>());

  virtual void StartNewStateMachine(
      boost::shared_ptr<StateMachineT> state_machine) = 0;

  virtual void RunInputOnStateMachine(
      boost::shared_ptr<InputT> input,
      boost::shared_ptr<StateMachineT> state_machine) = 0;

  // Optional override if the derived class needs to do any special handling on
  // a finished state machine.
  virtual void HandleStateMachineFinishedInternal(
      boost::shared_ptr<StateMachineT> state_machine) {}

  // Deactivates the current state machine, and then tries to run the next
  // available state machine if possible. It is necessary to go through these
  // two steps in sequence, rather than just trying to run the next input on the
  // given already-active state machine because this pool may need to wait for
  // the next pool to have available input slots.
  void DeactivateStateMachineAndTryRunNext(
      boost::shared_ptr<StateMachineT> state_machine);

 protected:

  // Pools for state machines that may retain partial input after producing
  // output can override this method if they want to try to continue working on
  // retained input before receiving new input. This method should return true
  // if the given state machine CANNOT be continued, and needs new input.
  virtual bool TryContinue(
      boost::shared_ptr<StateMachineT> state_machine) {
    return false;
  }

 private:
  virtual size_t NumRunningStateMachines() const;

  virtual bool IsCompletelyIdleInternal() const;

  virtual void TryRunNextStateMachineInternal();

  boost::shared_ptr<StateMachineT> TryActivateStateMachine();

  void DeactivateStateMachine(
      boost::shared_ptr<StateMachineT> state_machine);

  void TryRunNextInput(boost::shared_ptr<StateMachineT> state_machine);

  void HandleStateMachineFinished(
      boost::shared_ptr<StateMachineT> state_machine);

  void TerminateAllStateMachines();

  queue<boost::shared_ptr<InputT> > pending_inputs_;
  queue<boost::shared_ptr<StateMachineT> > idle_state_machines_;
  set<boost::shared_ptr<StateMachineT> > active_state_machines_;

  DISALLOW_COPY_AND_ASSIGN(PersistentStateMachinePool);
};

template <typename StateMachineT, typename InputT>
PersistentStateMachinePool<StateMachineT, InputT>::PersistentStateMachinePool(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    size_t max_pending_inputs, size_t max_simultaneous_state_machines,
    boost::shared_ptr<StateMachinePoolBase> preceding_pool)
    : StateMachinePool<InputT>(strand_dispatcher, max_pending_inputs,
                               max_simultaneous_state_machines,
                               preceding_pool) {
}

template <typename StateMachineT, typename InputT>
PersistentStateMachinePool<StateMachineT, InputT>::
~PersistentStateMachinePool() {
}

template <typename StateMachineT, typename InputT>
void PersistentStateMachinePool<StateMachineT,
                                InputT>::DeactivateStateMachineAndTryRunNext(
    boost::shared_ptr<StateMachineT> state_machine) {
  DeactivateStateMachine(state_machine);
  StateMachinePool<InputT>::TryRunNextStateMachine();
}

template <typename StateMachineT, typename InputT>
size_t PersistentStateMachinePool<StateMachineT,
                                  InputT>::NumRunningStateMachines() const {
  return active_state_machines_.size();
}

template <typename StateMachineT, typename InputT>
bool PersistentStateMachinePool<StateMachineT,
                                InputT>::IsCompletelyIdleInternal() const {
  return active_state_machines_.empty();
}

template <typename StateMachineT, typename InputT>
void PersistentStateMachinePool<StateMachineT,
                                InputT>::TryRunNextStateMachineInternal() {
  boost::shared_ptr<StateMachineT> activated_state_machine =
      TryActivateStateMachine();
  if (activated_state_machine != nullptr) {
    if (!TryContinue(activated_state_machine)) {
      TryRunNextInput(activated_state_machine);
    }
  }
}

template <typename StateMachineT, typename InputT>
boost::shared_ptr<StateMachineT>
PersistentStateMachinePool<StateMachineT, InputT>::TryActivateStateMachine() {
  boost::shared_ptr<StateMachineT> activated_state_machine;
  if (!idle_state_machines_.empty()) {
    activated_state_machine = idle_state_machines_.front();
    idle_state_machines_.pop();
  } else if (active_state_machines_.size() <
             this->max_simultaneous_state_machines()) {
    activated_state_machine.reset(new StateMachineT);
    activated_state_machine->SetDoneCallback(
        this->CreateStrandCallback(
            bind(&PersistentStateMachinePool<
                     StateMachineT, InputT>::HandleStateMachineFinished,
                 this, activated_state_machine)));
    StartNewStateMachine(activated_state_machine);
  }

  if (activated_state_machine != nullptr) {
    active_state_machines_.insert(activated_state_machine);
  }
  return activated_state_machine;
}

template <typename StateMachineT, typename InputT>
void PersistentStateMachinePool<StateMachineT, InputT>::DeactivateStateMachine(
    boost::shared_ptr<StateMachineT> state_machine) {
  assert(state_machine != nullptr);
  if(active_state_machines_.find(state_machine) !=
     active_state_machines_.end()) {
    active_state_machines_.erase(state_machine);
    idle_state_machines_.push(state_machine);
  }
}

template <typename StateMachineT, typename InputT>
void PersistentStateMachinePool<StateMachineT, InputT>::TryRunNextInput(
    boost::shared_ptr<StateMachineT> state_machine) {
  assert(state_machine != nullptr);

  boost::shared_ptr<InputT> input = this->PopNextInput();
  if (input != nullptr) {
    RunInputOnStateMachine(input, state_machine);
  } else {
    DeactivateStateMachine(state_machine);
  }

  if (this->IsCompletelyIdleAndNotExpectingMoreInput()) {
    TerminateAllStateMachines();
  }
}

template <typename StateMachineT, typename InputT>
void
PersistentStateMachinePool<StateMachineT, InputT>::HandleStateMachineFinished(
    boost::shared_ptr<StateMachineT> state_machine) {
  HandleStateMachineFinishedInternal(state_machine);

  // Presistent state machines by definition are not restartable. They only
  // finish when explicitly terminated, So the state machine should not return
  // to the idle pool.
  active_state_machines_.erase(state_machine);
}

template <typename StateMachineT, typename InputT>
void
PersistentStateMachinePool<StateMachineT, InputT>::TerminateAllStateMachines() {
  assert(active_state_machines_.empty());
  while (!idle_state_machines_.empty()) {
    boost::shared_ptr<StateMachineT> state_machine =
        idle_state_machines_.front();
    idle_state_machines_.pop();
    active_state_machines_.insert(state_machine);
    state_machine->FinishAndExit();
  }
}

}  // namespace polar_express

#endif  // PERSISTENT_STATE_MACHINE_POOL_H
