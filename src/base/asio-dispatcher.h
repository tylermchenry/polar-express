#ifndef ASIO_DISPATCHER_H
#define ASIO_DISPATCHER_H

#include <memory>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include "base/callback.h"
#include "base/macros.h"

namespace boost {
class thread_group;
}  // namespace boost

namespace polar_express {

// Singleton class which maintains a collection of seperate ASIO services which
// handle tasks that block on a certain type of I/O. (This prevents different
// types of blocking I/O from unnecessarily starving each other.)
//
// Since tasks running in one service can post callbacks to another service, the
// dispatcher ensures that no service shuts down until all services are
// completely out of work.
//
// This is accomplished by having a master service which runs no tasks, but
// holds work on each of the other services, and whose run() method is called
// directly in WaitForFinish. Every one of the other services holds a work
// object on the master service for each task that it is running or has
// pending. These work objects are deleted when the tasks complete. When all
// tasks across all other services are complete, the master service's run()
// method will return, and it can then delete the work it holds on each of the
// other services, which will cause them to shut down.
//
// In order for this system to function correctly, neither the services nor any
// strands associated with them can be made directly accessible. Instead, all
// posting of tasks must be done through wrapper methods.
class AsioDispatcher {
 public:
  static boost::shared_ptr<AsioDispatcher> GetInstance();

  virtual ~AsioDispatcher();

  // Starts all contained ASIO services and their worker threads, and returns
  // immediately.
  virtual void Start();

  // Blocks until all contained ASIO services no longer have any active or
  // pending work. When this returns, all worker threads have been joined.
  virtual void WaitForFinish();

  // These methods post callbacks to one of the contained ASIO services. The
  // services are distinguished as follows:
  //
  //  CPU-Bound: For long-running, CPU-intensive tasks.
  //  Disk-Bound: For tasks that access the local disk.
  //  Uplink-Bound: For tasks that upload data.
  //  Downlink-Bound: For tasks that download data.
  //  State Machine: For tasks that execute a short-running, non-CPU-intensive
  //                 piece of state machine logic.
  //  User Interface: All tasks that are interactive with a user.
  virtual void PostCpuBound(Callback callback);
  virtual void PostDiskBound(Callback callback);
  virtual void PostUplinkBound(Callback callback);
  virtual void PostDownlinkBound(Callback callback);
  virtual void PostStateMachine(Callback callback);
  virtual void PostUserInterface(Callback callback);

  // Used externally to tell other classes whether they should consider
  // themselves uplink or downlink bound when posting to dispatcher
  // threads.
  enum class NetworkUsageType {
    kInvalid,
    kUplinkBound,
    kDownlinkBound,
    kLocalhost,
  };

  // A strand dispatcher is a wrapper around an ASIO strand that is associated
  // with one of the contained ASIO services.
  class StrandDispatcher {
   public:
    StrandDispatcher(
        const AsioDispatcher* asio_dispatcher,
        boost::shared_ptr<asio::io_service> io_service);
    void Post(Callback callback);

    // Creates a wrapper callback that will call the given callback in
    // this strand.
    Callback CreateStrandCallback(Callback callback);

    // The following is some template magic that allows creation of
    // callbacks which will be run in the appropriate strand for the purpose of
    // passing to certain boost methods. This requires some work, because unlike
    // all of the internal callbacks in polar express, which are zero-argument
    // and fully bound, the some callbacks invoked by boost objects need to take
    // arguments.
    //
    // So the following templates allow us to create a callback that takes
    // the arguments that boost wants to pass. When it is invoked, it
    // creates a new, fully-bound callback using those arguments and posts
    // it to the appropriate strand.
    template <typename T1, typename PT1>
    boost::function<void(T1)> MakeStrandCallbackWithArgs(
        boost::function<void(T1)> callback_with_args, PT1 arg1_placeholder);

    template <typename T1, typename T2, typename PT1, typename PT2>
    boost::function<void(T1, T2)> MakeStrandCallbackWithArgs(
        boost::function<void(T1, T2)> callback_with_args, PT1 arg1_placeholder,
        PT2 arg2_placeholder);

    // USE THESE SPARINGLY. Misuse of raw io_service objects can really
    // screw up the dispatch loop. This should only really be
    // necessary for creating low-level boost ASIO-based networking objects.
    asio::io_service& io_service();
    unique_ptr<asio::io_service::work> make_work();
   private:
    template <typename T1>
    void StrandCallbackWithArgs(
        boost::function<void(T1)> callback_with_args, T1 arg1);

    template <typename T1, typename T2>
    void StrandCallbackWithArgs(
        boost::function<void(T1, T2)> callback_with_args, T1 arg1, T2 arg2);

    const AsioDispatcher* const asio_dispatcher_;
    const boost::shared_ptr<asio::io_service> io_service_;
    const boost::shared_ptr<asio::io_service::strand> strand_;
  };

  // Methods to obtain a strand associated with one of the contained ASIO
  // services. Strands should be used when an object wants to make sure that all
  // of its callbacks are run serially, to avoid explicit interal
  // synchronization.
  virtual boost::shared_ptr<StrandDispatcher> NewStrandDispatcherCpuBound();
  virtual boost::shared_ptr<StrandDispatcher> NewStrandDispatcherDiskBound();
  virtual boost::shared_ptr<StrandDispatcher> NewStrandDispatcherUplinkBound();
  virtual boost::shared_ptr<StrandDispatcher>
      NewStrandDispatcherDownlinkBound();
  virtual boost::shared_ptr<StrandDispatcher> NewStrandDispatcherStateMachine();
  virtual boost::shared_ptr<StrandDispatcher>
      NewStrandDispatcherUserInterface();

  // When network_usage_type is kLocalhost, this returns a dispatcher for the
  // User Interface thread pool. (This assumes that the UI is always going to be
  // interacting with the main daemon program over a local socket of some sort,
  // which is currently the plan.)
  virtual boost::shared_ptr<StrandDispatcher> NewStrandDispatcherNetworkBound(
      NetworkUsageType network_usage_type);

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
  boost::shared_ptr<asio::io_service> user_interface_io_service_;

  vector<boost::shared_ptr<asio::io_service::work> > work_;
  boost::shared_ptr<thread_group> worker_threads_;

  static int kNumWorkersPerService;
  static boost::shared_ptr<AsioDispatcher> instance_;

  DISALLOW_COPY_AND_ASSIGN(AsioDispatcher);
};

template <typename T1>
void AsioDispatcher::StrandDispatcher::StrandCallbackWithArgs(
    boost::function<void(T1)> callback_with_args, T1 arg1) {
  Post(boost::bind(callback_with_args, arg1));
}

template <typename T1, typename T2>
void AsioDispatcher::StrandDispatcher::StrandCallbackWithArgs(
    boost::function<void(T1, T2)> callback_with_args, T1 arg1, T2 arg2) {
  Post(boost::bind(callback_with_args, arg1, arg2));
}

template <typename T1, typename PT1>
boost::function<void(T1)>
AsioDispatcher::StrandDispatcher::MakeStrandCallbackWithArgs(
    boost::function<void(T1)> callback_with_args, PT1 arg1_placeholder) {
  return boost::bind(
      &AsioDispatcher::StrandDispatcher::StrandCallbackWithArgs<T1>, this,
      callback_with_args, arg1_placeholder);
}

template <typename T1, typename T2, typename PT1, typename PT2>
boost::function<void(T1, T2)>
AsioDispatcher::StrandDispatcher::MakeStrandCallbackWithArgs(
    boost::function<void(T1, T2)> callback_with_args, PT1 arg1_placeholder,
    PT2 arg2_placeholder) {
  return boost::bind(
      &AsioDispatcher::StrandDispatcher::StrandCallbackWithArgs<T1, T2>, this,
      callback_with_args, arg1_placeholder, arg2_placeholder);
}

}  // namespace polar_express

#endif  // ASIO_DISPATCHER_H
