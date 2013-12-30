#ifndef IO_UTIL_H
#define IO_UTIL_H

#include <cstdlib>
#include <ctime>
#include <string>

namespace polar_express {
namespace io_util {

std::string HumanReadableSize(size_t bytes);

std::string HumanReadableDuration(time_t duration);

}  // namespace io_util
}  // namespace polar_express

#endif  // IO_UTIL

