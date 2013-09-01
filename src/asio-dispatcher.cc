#include "asio-dispatcher.h"

#include "boost/bind.hpp"
#include "boost/thread/once.hpp"
#include "boost/thread/thread.hpp"

namespace polar_express {

int AsioDispatcher::kNumWorkersPerService = 2;
boost::shared_ptr<AsioDispatcher> AsioDispatcher::instance_;

// static
boost::shared_ptr<AsioDispatcher> AsioDispatcher::GetInstance() {
  static once_flag once = BOOST_ONCE_INIT;
  call_once(InitInstance, once);
  return instance_;
}

AsioDispatcher::AsioDispatcher() {
}

AsioDispatcher::~AsioDispatcher() {
}

void AsioDispatcher::Start() {
  work_.clear();
  worker_threads_.reset(new thread_group);
  master_io_service_.reset(new asio::io_service);
  cpu_io_service_ = StartService();
  disk_io_service_ = StartService();
  uplink_io_service_ = StartService();
  downlink_io_service_ = StartService();
  state_machine_io_service_ = StartService();
}

void AsioDispatcher::WaitForFinish() {
  master_io_service_->run();
  work_.clear();
  worker_threads_->join_all();
}

void AsioDispatcher::PostCpuBound(Callback callback) {
  PostToService(callback, cpu_io_service_);
}

void AsioDispatcher::PostDiskBound(Callback callback) {
  PostToService(callback, disk_io_service_);
}

void AsioDispatcher::PostUplinkBound(Callback callback) {
  PostToService(callback, uplink_io_service_);
}

void AsioDispatcher::PostDownlinkBound(Callback callback) {
  PostToService(callback, downlink_io_service_);
}

void AsioDispatcher::PostStateMachine(Callback callback) {
  PostToService(callback, state_machine_io_service_);
}

boost::shared_ptr<AsioDispatcher::StrandDispatcher>
AsioDispatcher::NewStrandDispatcherCpuBound() {
  return boost::shared_ptr<StrandDispatcher>(
      new StrandDispatcher(this, cpu_io_service_));
}

boost::shared_ptr<AsioDispatcher::StrandDispatcher>
AsioDispatcher::NewStrandDispatcherDiskBound() {
  return boost::shared_ptr<StrandDispatcher>(
      new StrandDispatcher(this, disk_io_service_));
}

boost::shared_ptr<AsioDispatcher::StrandDispatcher>
AsioDispatcher::NewStrandDispatcherUplinkBound() {
  return boost::shared_ptr<StrandDispatcher>(
      new StrandDispatcher(this, uplink_io_service_));
}

boost::shared_ptr<AsioDispatcher::StrandDispatcher>
AsioDispatcher::NewStrandDispatcherDownlinkBound() {
  return boost::shared_ptr<StrandDispatcher>(
      new StrandDispatcher(this, downlink_io_service_));
}

boost::shared_ptr<AsioDispatcher::StrandDispatcher>
AsioDispatcher::NewStrandDispatcherStateMachine() {
  return boost::shared_ptr<StrandDispatcher>(
      new StrandDispatcher(this, state_machine_io_service_));
}

boost::shared_ptr<asio::io_service> AsioDispatcher::StartService() {
  boost::shared_ptr<asio::io_service> io_service(new asio::io_service);
  work_.push_back(boost::shared_ptr<asio::io_service::work>(
      new asio::io_service::work(*io_service)));
  for (int i = 0; i < kNumWorkersPerService; ++i) {
    worker_threads_->create_thread(bind(&WorkerThread, io_service));
  }
  return io_service;
}

Callback AsioDispatcher::WrapCallbackWithMasterWork(Callback callback) const {
  return boost::bind(&AsioDispatcher::RunCallbackAndDeleteWork, callback,
                     new asio::io_service::work(*master_io_service_));
}

void AsioDispatcher::PostToService(
    Callback callback,
    boost::shared_ptr<asio::io_service> io_service) {
  io_service->post(WrapCallbackWithMasterWork(callback));
}

// static
void AsioDispatcher::InitInstance() {
  instance_.reset(new AsioDispatcher);
}

// static
void AsioDispatcher::WorkerThread(
    boost::shared_ptr<asio::io_service> io_service) {
  io_service->run();
}

// static
void AsioDispatcher::RunCallbackAndDeleteWork(
    Callback callback, asio::io_service::work* work) {
  callback();
  delete work;
}

AsioDispatcher::StrandDispatcher::StrandDispatcher(
    const AsioDispatcher* asio_dispatcher,
    boost::shared_ptr<asio::io_service> io_service)
    : asio_dispatcher_(CHECK_NOTNULL(asio_dispatcher)),
      io_service_(io_service),
      strand_(new asio::io_service::strand(*io_service)) {
}

void AsioDispatcher::StrandDispatcher::Post(Callback callback) {
  strand_->post(asio_dispatcher_->WrapCallbackWithMasterWork(callback));
}

Callback AsioDispatcher::StrandDispatcher::CreateStrandCallback(
    Callback callback) {
  return bind(&AsioDispatcher::StrandDispatcher::Post, this, callback);
}

asio::io_service& AsioDispatcher::StrandDispatcher::io_service() {
  return *io_service_;
}

}  // namespace polar_express
