#ifndef UPLOAD_STATE_MACHINE_POOL_H
#define UPLOAD_STATE_MACHINE_POOL_H

#include <string>

#include "boost/shared_ptr.hpp"

#include "cryptor.h"
#include "macros.h"
#include "persistent-state-machine-pool.h"

namespace polar_express {

class AnnotatedBundleData;
class UploadStateMachine;

class UploadStateMachinePool : public PersistentStateMachinePool<
    UploadStateMachine, AnnotatedBundleData> {
 public:
  UploadStateMachinePool(
      boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
      boost::shared_ptr<StateMachinePoolBase> preceding_pool =
          boost::shared_ptr<StateMachinePoolBase>());

  virtual ~UploadStateMachinePool();

  int num_bundles_uploaded() const;

 private:
  virtual void StartNewStateMachine(
      boost::shared_ptr<UploadStateMachine> state_machine);

  virtual void RunInputOnStateMachine(
      boost::shared_ptr<AnnotatedBundleData> input,
      boost::shared_ptr<UploadStateMachine> state_machine);

  int num_bundles_uploaded_;

  DISALLOW_COPY_AND_ASSIGN(UploadStateMachinePool);
};

}  // namespace polar_express

#endif  // UPLOAD_STATE_MACHINE_POOL_H
