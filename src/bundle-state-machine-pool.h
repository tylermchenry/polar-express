#ifndef BUNDLE_STATE_MACHINE_POOL_H
#define BUNDLE_STATE_MACHINE_POOL_H

#include <set>
#include <string>

#include "boost/shared_ptr.hpp"

#include "cryptor.h"
#include "macros.h"
#include "persistent-state-machine-pool.h"

namespace polar_express {

class AnnotatedBundleData;
class BundleStateMachine;
class Snapshot;

class BundleStateMachinePool
    : public PersistentStateMachinePool<BundleStateMachine, Snapshot> {
 public:
  BundleStateMachinePool(
      boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher,
      const string& root, Cryptor::EncryptionType encryption_type,
      boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data,
      boost::shared_ptr<StateMachinePoolBase> preceding_pool =
          boost::shared_ptr<StateMachinePoolBase>());

  virtual ~BundleStateMachinePool();

  void SetNextPool(
      boost::shared_ptr<StateMachinePool<AnnotatedBundleData> > next_pool);

  int num_bundles_generated() const;

 private:
  virtual void StartNewStateMachine(
      boost::shared_ptr<BundleStateMachine> state_machine);

  virtual void RunInputOnStateMachine(
      boost::shared_ptr<Snapshot> input,
      boost::shared_ptr<BundleStateMachine> state_machine);

  virtual bool TryContinue(
      boost::shared_ptr<BundleStateMachine> state_machine);

  void HandleSnapshotDone(boost::shared_ptr<BundleStateMachine> state_machine);

  void HandleBundleReady(boost::shared_ptr<BundleStateMachine> state_machine);

  const string root_;
  const Cryptor::EncryptionType encryption_type_;
  const boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data_;

  int num_bundles_generated_;
  std::set<const BundleStateMachine*> continueable_state_machines_;
  boost::shared_ptr<StateMachinePool<AnnotatedBundleData> > next_pool_;

  DISALLOW_COPY_AND_ASSIGN(BundleStateMachinePool);
};

}  // namespace polar_express

#endif  // BUNDLE_STATE_MACHINE_POOL_H
