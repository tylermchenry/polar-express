#include "snapshot-state-machine-pool.h"

#include <iostream>

#include "proto/snapshot.pb.h"
#include "snapshot-state-machine.h"

namespace polar_express {
namespace {

// TODO: Configurable.
const size_t kMaxPendingBlockBytes = 50 * (1 << 20);  // 50 MiB
const size_t kMaxSimultaneousSnapshots = 20;

}  // namespace

SnapshotStateMachinePool::SnapshotStateMachinePool(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    const string& root)
    : OneShotStateMachinePool<SnapshotStateMachine, boost::filesystem::path>(
        strand_dispatcher, kMaxPendingBlockBytes, kMaxSimultaneousSnapshots),
      root_(root),
      input_finished_(false),
      num_snapshots_generated_(0),
      size_of_snapshots_generated_(0) {
}

SnapshotStateMachinePool::~SnapshotStateMachinePool() {
}

void SnapshotStateMachinePool::SetNextPool(
    boost::shared_ptr<StateMachinePool<Snapshot> > next_pool) {
  next_pool_ = next_pool;
  set_next_pool(next_pool);
}

void SnapshotStateMachinePool::SetNeedMoreInputCallback(Callback callback) {
  need_more_input_callback_ = callback;
}

size_t SnapshotStateMachinePool::OutputWeightToBeAddedByInput(
    boost::shared_ptr<boost::filesystem::path> input) const {
  return std::min<size_t>(
      next_pool_max_input_weight(),
      std::max<size_t>(
          1, is_regular(*CHECK_NOTNULL(input)) ? file_size(*input) : 0));
}

void SnapshotStateMachinePool::NotifyInputFinished() {
  input_finished_ = true;
}

int SnapshotStateMachinePool::num_snapshots_generated() const {
  return num_snapshots_generated_;
}

size_t SnapshotStateMachinePool::size_of_snapshots_generated() const {
  return size_of_snapshots_generated_;
}

bool SnapshotStateMachinePool::IsExpectingMoreInput() const {
  return !input_finished_;
}

void SnapshotStateMachinePool::RunInputOnStateMachine(
    boost::shared_ptr<boost::filesystem::path> input,
    SnapshotStateMachine* state_machine) {
  state_machine->Start(root_, *input);

  DLOG(std::cerr << "Snapshotting " << *input << std::endl);

  if (IsExpectingMoreInput() &&
      (pending_inputs_weight() < (max_pending_inputs_weight() / 2)) &&
      need_more_input_callback_ != nullptr) {
    need_more_input_callback_();
  }
}

void SnapshotStateMachinePool::HandleStateMachineFinishedInternal(
    SnapshotStateMachine* state_machine) {
  boost::shared_ptr<Snapshot> generated_snapshot =
      state_machine->GetGeneratedSnapshot();
  if (generated_snapshot != nullptr) {
    if (generated_snapshot->is_regular() &&
        generated_snapshot->length() > 0) {
      const size_t snapshot_size = generated_snapshot->length();
      ++num_snapshots_generated_;
      size_of_snapshots_generated_ += snapshot_size;
      if (next_pool_ != nullptr) {
        assert(next_pool_->CanAcceptNewInput());
        next_pool_->AddNewInput(
            generated_snapshot,
            std::min(next_pool_max_input_weight(), snapshot_size));
      }
    }
    // TODO: Handle non-regular files (directories, deletions, etc.)
  }
}

}  // namespace polar_express
