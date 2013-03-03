#ifndef THREAD_LAUNCHER_H
#define THREAD_LAUNCHER_H

#include <boost/thread/thread.hpp>

#include "macros.h"

namespace polar_express {

// A thin, mockable wrapper around starting an new Boost thread.
class ThreadLauncher {
 public:
  ThreadLauncher() {}
  virtual ~ThreadLauncher() {}

  template <typename FunctorT>
  thread* Launch(FunctorT functor) const {
    return ShouldLaunchThread() ? new thread(functor) : nullptr;
  }

 protected:
  // Always return true except when using a mock.
  virtual bool ShouldLaunchThread() const {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadLauncher);
};

}  // namespace polar_express

#endif  // THREAD_LAUNCHER_H

