#ifndef ASIO_DISPATCHER_H
#define ASIO_DISPATCHER_H

#include "boost/asio.hpp"
#include "boost/function.hpp"
#include "boost/shared_ptr.hpp"

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
  
  virtual void PostCpuBound(boost::function<void()> callback);
  virtual void PostDiskBound(boost::function<void()> callback);
  virtual void PostUplinkBound(boost::function<void()> callback);
  virtual void PostDownlinkBound(boost::function<void()> callback);
  virtual void PostStateMachine(boost::function<void()> callback);

 private:
  AsioDispatcher();

  boost::shared_ptr<asio::io_service> StartService();
  void PostToService(
      boost::function<void()> callback,
      boost::shared_ptr<asio::io_service> io_service);
  
  static void InitInstance();
  static void WorkerThread(boost::shared_ptr<asio::io_service> io_service);
  static void RunCallbackAndDeleteWork(
      boost::function<void()> callback,
      asio::io_service::work* work);

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
