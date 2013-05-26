#ifndef CONTAINER_OFFSET_PTR
#define CONTAINER_OFFSET_PTR

#include "boost/shared_ptr.h"

namespace polar_express {

// A smart-pointer-like wrapper for the concept of a fixed offset into
// an STL-style container. The container can reallocate its underlying
// data storage, but so long as the container itself is not
// deallocated, and so long as the overall container size does not
// become smaller than the offset, the container offset pointer
// remains valid. If either of these two conditions are violated,
// behavior is undefined.
template <typename ContainerT>
class ContainerOffsetPtr {
 public:
  ContainerOffsetPtr(boost::shared_ptr<ContainerT> container, size_t offset)
      : container_(container),
        offset_(offset) {
  }

  ContainerT::value_type* get() const {
    return &(*(container_->begin() + offset_));
  }

  ContainerT::value_type* operator->() const {
    return get();
  }

  ContainerT::value_type& operator*() const {
    return *get();
  }

  bool operator==(const ContainerT::value_type* rhs) const {
    return get() == rhs;
  }

  bool operator!=(const ContainerT::value_type* rhs) const {
    return get() != rhs;
  }

 private:
  boost::shared_ptr<ContainerT> container_;
  size_t offset_;
};

}  // namespace polar_express

#endif  // CONTAINER_OFFSET_PTR
