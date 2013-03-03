#ifndef THREAD_LAUNCHER_H
#define THREAD_LAUNCHER_H

#include <iostream>

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
    // TODO: Is there a way that we can mock boost::thread directly? If so, this
    // would allow unit tests to exercise thread interrupting/joining calls.
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

