#ifndef STATE_MACHINE_POOL_H
#define STATE_MACHINE_POOL_H

#include <queue>
#include <utility>

#include "boost/bind.hpp"
#include "boost/shared_ptr.hpp"

#include "asio-dispatcher.h"
#include "macros.h"

namespace polar_express {

// Interface for StateMachinePool (below) without any dependencies
// on InputT or other template arguments.
//
// This is entirely abstract except for helper method that allows any
// descendent of this class to call TryRunNextStateMachine on any other
// descendent of this class. (Under C++ inheritence rules, siblings cannot
// directly call each other's protected members.)
class StateMachinePoolBase {
 public:
  virtual ~StateMachinePoolBase() {}

  virtual bool CanAcceptNewInput(size_t weight = 1) const = 0;
  virtual size_t InputWeightRemaining() const = 0;
  virtual const char* name() const = 0;

 protected:
  StateMachinePoolBase() {}

  virtual bool IsExpectingMoreInput() const = 0;
  virtual size_t MaxNumSimultaneousStateMachines() const = 0;
  virtual size_t NumRunningStateMachines() const = 0;
  virtual bool IsCompletelyIdle() const = 0;
  virtual bool IsCompletelyIdleAndNotExpectingMoreInput() const = 0;
  virtual void TryRunNextStateMachine() = 0;
  virtual void PostCallbackToStrand(Callback callback) = 0;
  virtual bool StrandDispatcherMatches(boost::shared_ptr<
      AsioDispatcher::StrandDispatcher> strand_dispatcher) const = 0;

  bool PoolIsCompletelyIdle(
      boost::shared_ptr<StateMachinePoolBase> pool) const {
    return CHECK_NOTNULL(pool)->IsCompletelyIdle();
  }

  bool PoolIsCompletelyIdleAndNotExpectingMoreInput(
      boost::shared_ptr<StateMachinePoolBase> pool) const {
    return CHECK_NOTNULL(pool)->IsCompletelyIdleAndNotExpectingMoreInput();
  }

  size_t MaxNumSimultaneousStateMachinesForPool(
      boost::shared_ptr<StateMachinePoolBase> pool) const {
    return CHECK_NOTNULL(pool)->MaxNumSimultaneousStateMachines();
  }

  void TryRunNextStateMachineOnPool(
      boost::shared_ptr<StateMachinePoolBase> pool) {
    CHECK_NOTNULL(pool)->PostCallbackToStrand(boost::bind(
        &StateMachinePoolBase::TryRunNextStateMachine, pool.get()));
  }

  bool StrandDispatcherForPoolMatches(
      boost::shared_ptr<StateMachinePoolBase> pool,
      boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher)
  const {
    return CHECK_NOTNULL(pool)->StrandDispatcherMatches(strand_dispatcher);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StateMachinePoolBase);
};

// Abstract base class for classes that implement the concept of pool of state
// machines that can grow to a certain size, and can be fed input. The input may
// go to any machine in the pool.
//
// Classes derived from StateMachinePool also have the concept of previous and
// next pools so that pools can be chained together easily to transform data
// with pipeline back-up semantics. A pool will refuse to process additional
// input if its next pool cannot receive new input. And when a pool becomes able
// to receive new input after having been full, it will trigger the previous
// pool to restart input processing, in case it had stopped on account of the
// previously mentioned behavior.
template <typename InputT>
class StateMachinePool : public StateMachinePoolBase {
 public:
  virtual ~StateMachinePool();

  virtual bool CanAcceptNewInput(size_t weight = 1) const;

  virtual size_t InputWeightRemaining() const;

  // Add a new input for this pool of state machines to process. Returns true if
  // the input was added, and false if it could not be added because the pool
  // already has its maximum number of inputs pending.
  bool AddNewInput(boost::shared_ptr<InputT> input, size_t weight = 1);

  virtual const char* name() const = 0;

 protected:
  StateMachinePool(
      boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
      size_t max_pending_inputs_weight, size_t max_simultaneous_state_machines,
      boost::shared_ptr<StateMachinePoolBase> preceding_pool =
          boost::shared_ptr<StateMachinePoolBase>());

  // Derived classes must NOT override these methods. Override the *Internal
  // versions below instead.
  virtual void TryRunNextStateMachine();
  virtual bool IsCompletelyIdle() const;
  virtual bool IsCompletelyIdleAndNotExpectingMoreInput() const;
  virtual void PostCallbackToStrand(Callback callback);
  virtual bool StrandDispatcherMatches(boost::shared_ptr<
      AsioDispatcher::StrandDispatcher> strand_dispatcher) const;
  virtual size_t MaxNumSimultaneousStateMachines() const {
    return max_simultaneous_state_machines();
  }

  // Optional override. Called to see if more input is expected in the case that
  // the preceding pool object is null.
  virtual bool IsExpectingMoreInput() const { return false; }

  // Derived classes must override.
  virtual size_t NumRunningStateMachines() const = 0;

  // The top-level versions of these method are not to be overridden below this
  // level so that the StateMachinePool object can guarantee to execute some
  // logic in them before/after calling these internal methods, which can
  // contain logic specific to the derived class.
  virtual bool IsCompletelyIdleInternal() const = 0;
  virtual void TryRunNextStateMachineInternal() = 0;

  boost::shared_ptr<InputT> PopNextInput();

  Callback CreateStrandCallback(Callback callback);

  void set_next_pool(boost::shared_ptr<StateMachinePoolBase> next_pool);

  size_t max_pending_inputs_weight() const {
    return max_pending_inputs_weight_;
  }

  size_t max_simultaneous_state_machines() const {
    return max_simultaneous_state_machines_;
  }

  size_t pending_inputs_weight() const {
    return pending_inputs_weight_;
  }

 private:
  const size_t max_pending_inputs_weight_;
  const size_t max_simultaneous_state_machines_;
  const boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher_;
  const boost::shared_ptr<StateMachinePoolBase> preceding_pool_;

  queue<std::pair<boost::shared_ptr<InputT>, size_t> > pending_inputs_;
  size_t pending_inputs_weight_;
  boost::shared_ptr<StateMachinePoolBase> next_pool_;

  DISALLOW_COPY_AND_ASSIGN(StateMachinePool);
};

template <typename InputT>
StateMachinePool<InputT>::StateMachinePool(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    size_t max_pending_inputs_weight, size_t max_simultaneous_state_machines,
    boost::shared_ptr<StateMachinePoolBase> preceding_pool)
    : strand_dispatcher_(CHECK_NOTNULL(strand_dispatcher)),
      max_pending_inputs_weight_(max_pending_inputs_weight),
      max_simultaneous_state_machines_(max_simultaneous_state_machines),
      preceding_pool_(preceding_pool),
      pending_inputs_weight_(0) {
  // The preceding pool must be using the same strand dispatcher, or
  // concurrency problems will result. These classes are not internally
  // synchronized, and that is only OK if their callbacks all run on the same
  // strand (so no two will run concurrently).
  assert(preceding_pool_ == nullptr ||
         StrandDispatcherForPoolMatches(preceding_pool_, strand_dispatcher_));
}

template <typename InputT>
StateMachinePool<InputT>::~StateMachinePool() {
}

template <typename InputT>
bool StateMachinePool<InputT>::CanAcceptNewInput(size_t weight) const {
  return (pending_inputs_weight_ + weight <= max_pending_inputs_weight_);
}

template <typename InputT>
size_t StateMachinePool<InputT>::InputWeightRemaining() const {
  return (max_pending_inputs_weight_ - pending_inputs_weight_);
}

template <typename InputT>
bool StateMachinePool<InputT>::AddNewInput(boost::shared_ptr<InputT> input,
                                           size_t weight) {
  if (!CanAcceptNewInput(weight)) {
    return false;
  }
  pending_inputs_weight_ += weight;
  pending_inputs_.push(make_pair(input, weight));

  // Is possible, activate a new state machine to handle this input immediately.
  TryRunNextStateMachine();
  return true;
}

template <typename InputT>
void StateMachinePool<InputT>::TryRunNextStateMachine() {
  // NOTE: The logic here relies on the assumption that a state machine's output
  // will always have weight = 1. This is valid for now, as the only time we
  // expect weight > 1 is for the initial input to the pool chain. If this
  // assumption changes, the code below must be reworked.

  // Don't process new input if the next pool might not be able to accept our
  // output, keeping in mind that other simultaneously-running state machines
  // from this pool may produce output before the remaining input slots of the
  // next pool increase.
  if (next_pool_ != nullptr &&
      next_pool_->InputWeightRemaining() < max_simultaneous_state_machines()) {
    DLOG(std::cerr
         << name()
         << " will not run another state machine; next pool is too full. Next "
            "pool weight remaining = " << next_pool_->InputWeightRemaining()
         << ", This pool max state machines = "
         << max_simultaneous_state_machines() << "." << std::endl);
    return;
  }

  TryRunNextStateMachineInternal();

  // If we just reduced the wait queue to below maximum, allow the
  // preceding pool to start running again in case it stopped.
  //
  // There is no problem with "kicking" the preceding pool like this every
  // single time, except that it is wasted work. So instead we kick the
  // preceding pool only when our input is in the range where:
  //
  //   a) the preceding pool would be willing to start another state machine,
  //      and
  //   b) the preceding pool might still have some idled state machines left to
  //      kick.
  //
  // These can both be expressed in terms of the pending and max weights on this
  // pool, and the max number of state machines that the previous pool is
  // willing to run.
  if (preceding_pool_ != nullptr) {
    const size_t preceding_pool_max_state_machines =
        MaxNumSimultaneousStateMachinesForPool(preceding_pool_);
    if (pending_inputs_weight_ <
            (max_pending_inputs_weight_ - preceding_pool_max_state_machines) &&
        pending_inputs_weight_ >= (max_pending_inputs_weight_ -
                                   (2 * preceding_pool_max_state_machines))) {
      DLOG(std::cerr
           << name() << " is kicking preceding pool. Pending inputs weight = "
           << pending_inputs_weight_ << " (max = " << max_pending_inputs_weight_
           << "). Preceding pool max state machines "
              "= " << preceding_pool_max_state_machines << "." << std::endl);
      TryRunNextStateMachineOnPool(preceding_pool_);
    }
  }
}

template <typename InputT>
bool StateMachinePool<InputT>::IsCompletelyIdle() const {
  return pending_inputs_.empty() && IsCompletelyIdleInternal();
}

template <typename InputT>
bool StateMachinePool<InputT>::IsCompletelyIdleAndNotExpectingMoreInput()
    const {
  return IsCompletelyIdle() &&
         (preceding_pool_ != nullptr
              ? PoolIsCompletelyIdleAndNotExpectingMoreInput(preceding_pool_)
              : IsExpectingMoreInput());
}

template <typename InputT>
void StateMachinePool<InputT>::PostCallbackToStrand(Callback callback) {
  strand_dispatcher_->Post(callback);
}

template <typename InputT>
bool StateMachinePool<InputT>::StrandDispatcherMatches(boost::shared_ptr<
    AsioDispatcher::StrandDispatcher> strand_dispatcher) const {
  return strand_dispatcher_ == strand_dispatcher;
}

template <typename InputT>
boost::shared_ptr<InputT> StateMachinePool<InputT>::PopNextInput() {
  boost::shared_ptr<InputT> next_input;
  if (!pending_inputs_.empty()) {
    auto next_input_pair = pending_inputs_.front();
    next_input = next_input_pair.first;
    pending_inputs_.pop();
    pending_inputs_weight_ -= next_input_pair.second;
    DLOG(std::cerr << name() << " popped input. Remaining inputs = "
                   << pending_inputs_.size() << " (weight = "
                   << pending_inputs_weight_ << ")." << std::endl);
  } else {
    DLOG(std::cerr << name() << " is out of input." << std::endl);
  }

  return next_input;
}

template <typename InputT>
Callback StateMachinePool<InputT>::CreateStrandCallback(Callback callback) {
  return strand_dispatcher_->CreateStrandCallback(callback);
}

template <typename InputT>
void StateMachinePool<InputT>::set_next_pool(
    boost::shared_ptr<StateMachinePoolBase> next_pool) {
  next_pool_ = next_pool;

  // The next pool must be using the same strand dispatcher, or
  // concurrency problems will result. These classes are not internally
  // synchronized, and that is only OK if their callbacks all run on the same
  // strand (so no two will run concurrently).
  assert(next_pool_ == nullptr ||
         StrandDispatcherForPoolMatches(next_pool, strand_dispatcher_));
}

}  // namespace polar_express

#endif  // STATE_MACHINE_POOL_H
