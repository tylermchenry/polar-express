#include "state_machines/bundle-state-machine-pool.h"

#include <iostream>

#include "base/options.h"
#include "file/bundle.h"
#include "proto/snapshot.pb.h"
#include "state_machines/bundle-state-machine.h"

DEFINE_OPTION(max_pending_bundle_bytes, size_t, 40 * (1 << 20) /* 40 MiB */,
              "Maximum amount of snapshotted file data that may be waiting to "
              "be bundled at any time.");

DEFINE_OPTION(max_bundle_size_bytes, size_t, 20 * (1 << 20) /* 20 MiB */,
              "Maximum size of bundles that will be uploaded to Glacier "
              "(before headers).");

DEFINE_OPTION(max_simultaneous_bundles, size_t, 3,
              "Maximum number of bundles that the system will simultaneously "
              "work on building.");

DEFINE_OPTION(max_upstream_idle_time_seconds, time_t, 30,
              "Amount of time elapse with no upstream traffic after which all "
              "pending bundles will be finalized and uploaded, even if they "
              "are not at maximum size.");

namespace polar_express {

BundleStateMachinePool::BundleStateMachinePool(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    const string& root, Cryptor::EncryptionType encryption_type,
    boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data,
    boost::shared_ptr<StateMachinePoolBase> preceding_pool)
    : PersistentStateMachinePool<BundleStateMachine, Snapshot>(
          strand_dispatcher, options::max_pending_bundle_bytes,
          options::max_simultaneous_bundles, preceding_pool),
      root_(root),
      encryption_type_(encryption_type),
      encryption_keying_data_(CHECK_NOTNULL(encryption_keying_data)),
      num_bundles_generated_(0),
      size_of_bundles_generated_(0),
      last_bundle_generated_time_(0) {}

BundleStateMachinePool::~BundleStateMachinePool() {
}

void BundleStateMachinePool::SetNextPool(
    boost::shared_ptr<StateMachinePool<AnnotatedBundleData> > next_pool) {
  next_pool_ = next_pool;
  set_next_pool(next_pool);
}

int BundleStateMachinePool::num_bundles_generated() const {
  return num_bundles_generated_;
}

size_t BundleStateMachinePool::size_of_bundles_generated() const {
  return size_of_bundles_generated_;
}

size_t BundleStateMachinePool::OutputWeightToBeAddedByInputInternal(
    boost::shared_ptr<Snapshot> input) const {
  // TODO: Fix int64 / size_t issue here.
  return std::min<size_t>(next_pool_max_input_weight(), input->length());
}

size_t BundleStateMachinePool::MaxOutputWeightGeneratedByAnyInput() const {
  return options::max_bundle_size_bytes;
}

void BundleStateMachinePool::StartNewStateMachine(
    boost::shared_ptr<BundleStateMachine> state_machine) {
  assert(state_machine != nullptr);

  state_machine->SetSnapshotDoneCallback(CreateStrandCallback(
      bind(&BundleStateMachinePool::HandleSnapshotDone, this, state_machine)));
  state_machine->SetBundleReadyCallback(CreateStrandCallback(
      bind(&BundleStateMachinePool::HandleBundleReady, this, state_machine)));

  state_machine->Start(root_, encryption_type_, encryption_keying_data_);
}

void BundleStateMachinePool::RunInputOnStateMachine(
    boost::shared_ptr<Snapshot> input,
    boost::shared_ptr<BundleStateMachine> state_machine) {
  DLOG(std::cerr << "Adding snapshot of " << input->file().path()
                 << " to bundle." << std::endl);
  CHECK_NOTNULL(state_machine)->BundleSnapshot(input);
}

bool BundleStateMachinePool::TryContinue(
    boost::shared_ptr<BundleStateMachine> state_machine) {
  assert(state_machine != nullptr);
  if (continueable_state_machines_.find(state_machine.get()) ==
      continueable_state_machines_.end()) {
    return false;
  }

  continueable_state_machines_.erase(state_machine.get());
  state_machine->Continue();
  return true;
}

void BundleStateMachinePool::HandleSnapshotDone(
    boost::shared_ptr<BundleStateMachine> state_machine) {
  continueable_state_machines_.erase(state_machine.get());

  time_t now = time(nullptr);
  if (next_pool_ != nullptr && last_bundle_generated_time_ > 0 &&
      now - last_bundle_generated_time_ >
          options::max_upstream_idle_time_seconds &&
      PoolIsCompletelyIdle(next_pool_)) {
    DLOG(std::cerr << "Flushing bundle due to timeout." << std::endl);
    CHECK_NOTNULL(state_machine)->FlushCurrentBundle();
  } else {
    if (next_pool_ != nullptr) {
      DLOG(std::cerr << "Bundle timeout info: now = " << now
                     << ", last bundle time = " << last_bundle_generated_time_
                     << " (diff = " << now - last_bundle_generated_time_
                     << ") Next is idle = " << PoolIsCompletelyIdle(next_pool_)
                     << std::endl);
    }
    DeactivateStateMachineAndTryRunNext(state_machine);
  }
}

void BundleStateMachinePool::HandleBundleReady(
    boost::shared_ptr<BundleStateMachine> state_machine) {
  const size_t output_weight_remaining =
      CHECK_NOTNULL(state_machine)->chunk_bytes_pending();
  StateMachineProducedOutput(
      state_machine,
      std::min(MaxOutputWeightGeneratedByAnyInput(), output_weight_remaining));

  boost::shared_ptr<AnnotatedBundleData> bundle_data =
      state_machine->RetrieveGeneratedBundle();
  if (bundle_data != nullptr) {
    const size_t bundle_size = bundle_data->file_contents_size();
    ++num_bundles_generated_;
    size_of_bundles_generated_ += bundle_size;
    last_bundle_generated_time_ = time(nullptr);
    // Not a DLOG until we provide a UI.
    std::cerr << "Wrote bundle to: "
              << bundle_data->annotations().persistence_file_path()
              << std::endl;
    if (next_pool_ != nullptr) {
      assert(next_pool_->CanAcceptNewInput());
      next_pool_->AddNewInput(
          bundle_data, std::min(next_pool_max_input_weight(), bundle_size));
    }
  }

  // This state machine should be continued to process existing left-over input
  // that did not make it into this bundle before it is given any new input.
  continueable_state_machines_.insert(state_machine.get());
  DeactivateStateMachineAndTryRunNext(state_machine);
}

}  // namespace polar_express
