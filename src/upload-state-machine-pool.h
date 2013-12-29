#ifndef UPLOAD_STATE_MACHINE_POOL_H
#define UPLOAD_STATE_MACHINE_POOL_H

#include <string>

#include "boost/shared_ptr.hpp"
#include "crypto++/secblock.h"

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
      const string& aws_region_name,
      const string& aws_access_key,
      const CryptoPP::SecByteBlock& aws_secret_key,
      const string& vault_name,
      boost::shared_ptr<StateMachinePoolBase> preceding_pool =
          boost::shared_ptr<StateMachinePoolBase>());

  virtual ~UploadStateMachinePool();

  void SetNextPool(
      boost::shared_ptr<StateMachinePool<AnnotatedBundleData> > next_pool);

  int num_bundles_uploaded() const;

  virtual const char* name() const {
    return "Upload State Machine Pool";
  }

 private:
  virtual void StartNewStateMachine(
      boost::shared_ptr<UploadStateMachine> state_machine);

  virtual void RunInputOnStateMachine(
      boost::shared_ptr<AnnotatedBundleData> input,
      boost::shared_ptr<UploadStateMachine> state_machine);

  void HandleBundleUploaded(
      boost::shared_ptr<UploadStateMachine> state_machine);

  const string aws_region_name_;
  const string aws_access_key_;
  const CryptoPP::SecByteBlock aws_secret_key_;
  const string vault_name_;

  int num_bundles_uploaded_;
  boost::shared_ptr<StateMachinePool<AnnotatedBundleData> > next_pool_;

  DISALLOW_COPY_AND_ASSIGN(UploadStateMachinePool);
};

}  // namespace polar_express

#endif  // UPLOAD_STATE_MACHINE_POOL_H
