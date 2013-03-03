#ifndef THREAD_LAUNCHER_MOCK_H
#define THREAD_LAUNCHER_MOCK_H

#include "gmock/gmock.h"

#include "thread-launcher.h"

namespace polar_express {

class ThreadLauncherMock : public ThreadLauncher {
 public:
  MOCK_CONST_METHOD0(ShouldLaunchThread, bool());
};

}  // namespace polar_express

#endif  // THREAD_LAUNCHER_MOCK_H

