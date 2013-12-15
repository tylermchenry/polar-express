#include "upload-state-machine-pool.h"

#include "upload-state-machine.h"

namespace polar_express {
namespace {

const size_t kMaxBundlesWaitingToUpload = 10;
const size_t kMaxSimultaneousUploads = 2;

}  // namespace

UploadStateMachinePool::UploadStateMachinePool(
    boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
    boost::shared_ptr<StateMachinePoolBase> preceding_pool)
    : PersistentStateMachinePool<UploadStateMachine, AnnotatedBundleData>(
          strand_dispatcher, kMaxBundlesWaitingToUpload,
          kMaxSimultaneousUploads, preceding_pool),
      num_bundles_uploaded_(0) {
}

UploadStateMachinePool::~UploadStateMachinePool() {
}

int UploadStateMachinePool::num_bundles_uploaded() const {
  return num_bundles_uploaded_;
}

void UploadStateMachinePool::StartNewStateMachine(
    boost::shared_ptr<UploadStateMachine> state_machine) {
  CHECK_NOTNULL(state_machine)->Start();
}

void UploadStateMachinePool::RunInputOnStateMachine(
    boost::shared_ptr<AnnotatedBundleData> input,
    boost::shared_ptr<UploadStateMachine> state_machine) {
  // TODO: Implement.
}

}  // namespace polar_express
