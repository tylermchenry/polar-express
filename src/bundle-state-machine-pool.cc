#include "bundle-state-machine-pool.h"

#include <iostream>

#include "bundle.h"
#include "bundle-state-machine.h"

namespace polar_express {
namespace {

// TODO: Configurable.
const size_t kMaxSnapshotsWaitingToBundle = 100;
const size_t kMaxSimultaneousBundles = 2;

}  // namespace

BundleStateMachinePool::BundleStateMachinePool(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    const string& root, Cryptor::EncryptionType encryption_type,
    boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data,
    boost::shared_ptr<StateMachinePoolBase> preceding_pool)
    : PersistentStateMachinePool<BundleStateMachine, Snapshot>(
          strand_dispatcher, kMaxSnapshotsWaitingToBundle,
          kMaxSimultaneousBundles, preceding_pool),
      root_(root),
      encryption_type_(encryption_type),
      encryption_keying_data_(CHECK_NOTNULL(encryption_keying_data)),
      num_bundles_generated_(0) {}

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

void BundleStateMachinePool::StartNewStateMachine(
    boost::shared_ptr<BundleStateMachine> state_machine) {
  assert(state_machine != nullptr);

  state_machine->SetSnapshotDoneCallback(CreateStrandCallback(
      bind(&BundleStateMachinePool::TryRunNextInput, this, state_machine)));
  state_machine->SetBundleReadyCallback(CreateStrandCallback(
      bind(&BundleStateMachinePool::HandleBundleReady, this, state_machine)));

  state_machine->Start(root_, encryption_type_, encryption_keying_data_);
}

void BundleStateMachinePool::RunInputOnStateMachine(
    boost::shared_ptr<Snapshot> input,
    boost::shared_ptr<BundleStateMachine> state_machine) {
  CHECK_NOTNULL(state_machine)->BundleSnapshot(input);
}

void BundleStateMachinePool::HandleBundleReady(
    boost::shared_ptr<BundleStateMachine> state_machine) {
  boost::shared_ptr<AnnotatedBundleData> bundle_data =
      state_machine->RetrieveGeneratedBundle();
  if (bundle_data != nullptr) {
    ++num_bundles_generated_;
    std::cerr << "Wrote bundle to: "
              << bundle_data->annotations().persistence_file_path()
              << std::endl;
    if (next_pool_ != nullptr) {
      assert(next_pool_->CanAcceptNewInput());
      next_pool_->AddNewInput(bundle_data);
    }
  }
  // The bundle state machine keeps running -- it may still have snapshot data
  // to process that did not fit into the bundle that it just produced.
}

}  // namespace polar_express
