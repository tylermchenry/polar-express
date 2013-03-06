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
  cpu_io_service_ = StartService();
  disk_io_service_ = StartService();
  uplink_io_service_ = StartService();
  downlink_io_service_ = StartService();
  state_machine_io_service_ = StartService();
}

void AsioDispatcher::Finish() {
  work_.clear();
  worker_threads_->join_all();
}

void AsioDispatcher::PostCpuBound(boost::function<void()> callback) {
  cpu_io_service_->post(callback);
}

void AsioDispatcher::PostDiskBound(boost::function<void()> callback) {
  disk_io_service_->post(callback);
}

void AsioDispatcher::PostUplinkBound(boost::function<void()> callback) {
  uplink_io_service_->post(callback);
}

void AsioDispatcher::PostDownlinkBound(boost::function<void()> callback) {
  downlink_io_service_->post(callback);
}

void AsioDispatcher::PostStateMachine(boost::function<void()> callback) {
  state_machine_io_service_->post(callback);
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

// static
void AsioDispatcher::InitInstance() {
  instance_.reset(new AsioDispatcher);
}

// static
void AsioDispatcher::WorkerThread(
    boost::shared_ptr<asio::io_service> io_service) {
  io_service->run();
}

}  // namespace polar_express
