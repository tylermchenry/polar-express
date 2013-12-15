#ifndef ONE_SHOT_STATE_MACHINE_POOL_H
#define ONE_SHOT_STATE_MACHINE_POOL_H

#include "boost/pool/object_pool.hpp"
#include "boost/shared_ptr.hpp"

#include "macros.h"
#include "state-machine-pool.h"

namespace polar_express {

// A state machine pool for one-shot state machines. That is, once started they
// may process a single input and then they terminate. Processing another input
// requires reconstructing the state machine object.
//
// The StateMachineT type must provide a method SetDoneCallback(Callback) which
// has the normal StateMachine semantics.
//
// This is an abstract base class. There must be a specialization for each
// StateMachineT type which at least provides an appropriate wrapper for the
// call to the state machine type's Start method, since the start method will
// take parameters that depend on the state machine type and the method for
// running inputs may be named differently on different state machines. The
// derived class may also handle additional type-specific callbacks for the
// state machine.
template <typename StateMachineT, typename InputT>
class OneShotStateMachinePool : public StateMachinePool<InputT> {
 public:
  virtual ~OneShotStateMachinePool();

 protected:
  OneShotStateMachinePool(
      boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
      size_t max_pending_inputs, size_t max_simultaneous_state_machines,
      boost::shared_ptr<StateMachinePoolBase> preceding_pool =
          boost::shared_ptr<StateMachinePoolBase>());

  virtual void RunInputOnStateMachine(boost::shared_ptr<InputT> input,
                                      StateMachineT* state_machine) = 0;

  // Optional override if the derived class needs to do any special handling on
  // a finished state machine.
  virtual void HandleStateMachineFinishedInternal(
      StateMachineT* state_machine) {}

 private:
  virtual size_t NumRunningStateMachines() const;

  virtual bool IsCompletelyIdleInternal() const;

  virtual void TryRunNextStateMachineInternal();

  void HandleStateMachineFinished(
      StateMachineT* state_machine);

  void DeleteStateMachine(
      StateMachineT* state_machine);

  const std::unique_ptr<boost::object_pool<StateMachineT> >
      state_machine_object_pool_;

  size_t num_running_state_machines_;
  size_t num_finished_state_machines_;

  DISALLOW_COPY_AND_ASSIGN(OneShotStateMachinePool);
};

template <typename StateMachineT, typename InputT>
OneShotStateMachinePool<StateMachineT, InputT>::OneShotStateMachinePool(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    size_t max_pending_inputs, size_t max_simultaneous_state_machines,
    boost::shared_ptr<StateMachinePoolBase> preceding_pool)
    : StateMachinePool<InputT>(strand_dispatcher, max_pending_inputs,
                               max_simultaneous_state_machines,
                               preceding_pool),
      state_machine_object_pool_(new boost::object_pool<StateMachineT>),
      num_running_state_machines_(0),
      num_finished_state_machines_(0) {
}

template <typename StateMachineT, typename InputT>
OneShotStateMachinePool<StateMachineT, InputT>::~OneShotStateMachinePool() {
}

template <typename StateMachineT, typename InputT>
size_t OneShotStateMachinePool<StateMachineT, InputT>::NumRunningStateMachines()
    const {
  return num_running_state_machines_;
}

template <typename StateMachineT, typename InputT>
bool OneShotStateMachinePool<StateMachineT, InputT>::IsCompletelyIdleInternal()
    const {
  return num_running_state_machines_ == 0;
}

template <typename StateMachineT, typename InputT>
void OneShotStateMachinePool<StateMachineT,
                             InputT>::TryRunNextStateMachineInternal() {
  if (num_running_state_machines_ >= this->max_simultaneous_state_machines()) {
    return;
  }

  boost::shared_ptr<InputT> input = this->PopNextInput();
  if (input == nullptr) {
    return;
  }

  StateMachineT* state_machine = state_machine_object_pool_->construct();
  state_machine->SetDoneCallback(this->CreateStrandCallback(
      bind(&OneShotStateMachinePool<StateMachineT,
                                    InputT>::HandleStateMachineFinished,
           this, state_machine)));

  RunInputOnStateMachine(input, state_machine);
  ++num_running_state_machines_;
}

template <typename StateMachineT, typename InputT>
void OneShotStateMachinePool<StateMachineT, InputT>::HandleStateMachineFinished(
    StateMachineT* state_machine) {
  HandleStateMachineFinishedInternal(state_machine);
  DeleteStateMachine(state_machine);
}

template <typename StateMachineT, typename InputT>
void OneShotStateMachinePool<StateMachineT, InputT>::DeleteStateMachine(
    StateMachineT* state_machine) {
  state_machine_object_pool_->destroy(state_machine);
  --num_running_state_machines_;
  ++num_finished_state_machines_;
  this->PostCallbackToStrand(bind(
      &OneShotStateMachinePool<StateMachineT, InputT>::TryRunNextStateMachine,
      this));
}

}  // namespace polar_express

#endif  // ONE_SHOT_STATE_MACHINE_POOL

