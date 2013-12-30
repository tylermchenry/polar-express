#include "io-util.h"

#include <cassert>
#include <array>
#include <iomanip>
#include <sstream>

namespace polar_express {
namespace io_util {
namespace  {

std::string HumanReadableDuration(time_t duration,
                                  int recursion_levels_remaining) {
  static const std::array<const char*, 6> kMagnitudes =
      { "seconds", "minutes", "hours", "days", "weeks", "years" };
  static const std::array<time_t, kMagnitudes.size()> kSecondsPerMagnitude =
      { 1, 60, 60 * 60, 60 * 60 * 24, 60 * 60 * 24 * 7, 60 * 60 * 24 * 365 };
  for (size_t i = 0; i < kMagnitudes.size(); ++i) {
    const size_t mag = kMagnitudes.size() - i - 1;
    const time_t mag_in_seconds = kSecondsPerMagnitude[mag];
    if (duration >= mag_in_seconds) {
      std::ostringstream osstr;
      if (recursion_levels_remaining > 0) {
        const time_t duration_mag_whole_part = duration / mag_in_seconds;
        const time_t duration_remainder = duration % mag_in_seconds;
        osstr << duration_mag_whole_part << " " << kMagnitudes[mag];
        if (mag > 0) {
          osstr << " and "
                << HumanReadableDuration(duration_remainder,
                                         recursion_levels_remaining - 1);
        }
      } else if (mag > 0) {
        const double mag_fraction =
            static_cast<double>(duration) / mag_in_seconds;
        osstr << std::fixed << std::setprecision(1) << mag_fraction << " "
              << kMagnitudes[mag];
      } else {
        osstr << duration << " " << kMagnitudes[0];
      }
      return osstr.str();
    }
  }
  // Should never reach here.
  assert(false);
  return "???? seconds";
}

}  // namespace

std::string HumanReadableSize(size_t bytes) {
  static std::array<const char*, 6> kMagnitudes =
      { "bytes", "KiB", "MiB", "GiB", "TiB", "PiB" };
  for (size_t i = 0; i < kMagnitudes.size(); ++i) {
    const size_t mag = kMagnitudes.size() - i - 1;
    const size_t mag_in_bytes = (static_cast<size_t>(1) << (10 * mag));
    if (bytes >= mag_in_bytes) {
      std::ostringstream osstr;
      if (mag > 0) {
        const double mag_fraction = static_cast<double>(bytes) / mag_in_bytes;
        osstr << std::fixed << std::setprecision(2) << mag_fraction << " "
              << kMagnitudes[mag];
      } else {
        osstr << bytes << " " << kMagnitudes[0];
      }
      return osstr.str();
    }
  }
  // Should never reach here.
  assert(false);
  return "???? bytes";
}

std::string HumanReadableDuration(time_t duration) {
  return HumanReadableDuration(duration, 1);
}

}  // namespace io_util
}  // namespace polar_express
