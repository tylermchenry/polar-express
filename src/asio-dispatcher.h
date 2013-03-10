#ifndef ASIO_DISPATCHER_H
#define ASIO_DISPATCHER_H

#include "boost/asio.hpp"
#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "macros.h"

namespace boost {
class thread_group;
}  // namespace boost

namespace polar_express {

class AsioDispatcher {
 public:
  static boost::shared_ptr<AsioDispatcher> GetInstance();

  virtual ~AsioDispatcher();
  
  virtual void Start();
  virtual void WaitForFinish();
  
  virtual void PostCpuBound(Callback callback);
  virtual void PostDiskBound(Callback callback);
  virtual void PostUplinkBound(Callback callback);
  virtual void PostDownlinkBound(Callback callback);
  virtual void PostStateMachine(Callback callback);

  class StrandDispatcher {
   public:
    StrandDispatcher(
        const AsioDispatcher* asio_dispatcher,
        boost::shared_ptr<asio::io_service> io_service);
    void Post(Callback callback);
   private:
    const AsioDispatcher* const asio_dispatcher_;
    const boost::shared_ptr<asio::io_service::strand> strand_;
  };
  
  virtual boost::shared_ptr<StrandDispatcher> NewStrandDispatcherCpuBound();
  virtual boost::shared_ptr<StrandDispatcher> NewStrandDispatcherDiskBound();
  virtual boost::shared_ptr<StrandDispatcher> NewStrandDispatcherUplinkBound();
  virtual boost::shared_ptr<StrandDispatcher> NewStrandDispatcherDownlinkBound();
  virtual boost::shared_ptr<StrandDispatcher> NewStrandDispatcherStateMachine();
  
 private:
  AsioDispatcher();

  boost::shared_ptr<asio::io_service> StartService();

  Callback WrapCallbackWithMasterWork(Callback callback) const;
  void PostToService(
      Callback callback,
      boost::shared_ptr<asio::io_service> io_service);
  
  static void InitInstance();
  static void WorkerThread(boost::shared_ptr<asio::io_service> io_service);
  static void RunCallbackAndDeleteWork(
      Callback callback, asio::io_service::work* work);

  boost::shared_ptr<asio::io_service> master_io_service_;
  
  boost::shared_ptr<asio::io_service> cpu_io_service_;
  boost::shared_ptr<asio::io_service> disk_io_service_;
  boost::shared_ptr<asio::io_service> uplink_io_service_;
  boost::shared_ptr<asio::io_service> downlink_io_service_;
  boost::shared_ptr<asio::io_service> state_machine_io_service_;

  vector<boost::shared_ptr<asio::io_service::work> > work_;
  boost::shared_ptr<thread_group> worker_threads_;
  
  static int kNumWorkersPerService;
  static boost::shared_ptr<AsioDispatcher> instance_;

  DISALLOW_COPY_AND_ASSIGN(AsioDispatcher);
};

}  // namespace polar_express

#endif  // ASIO_DISPATCHER_H
