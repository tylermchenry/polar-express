#ifndef CONTAINER_UTIL
#define CONTAINER_UTIL

#include <string>

#include <boost/algorithm/string/predicate.hpp>

namespace polar_express {
namespace container_util {

template <typename T>
class CaseInsensitiveLexicographicalComparator {
 public:
  bool operator()(const T& lhs, const T& rhs) const {
    return algorithm::ilexicographical_compare(lhs, rhs);
  }
};

typedef set<string, CaseInsensitiveLexicographicalComparator<string> >
    CaseInsensitiveStringSet;

template <typename ContainerT>
bool Contains(const ContainerT& c, const typename ContainerT::value_type& v) {
  return c.find(v) != c.end();
}

}  // namespace container_util
}  // namespace container_util

#endif  // CONTAINER_UTIL
