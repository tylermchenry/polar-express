#ifndef CONTAINER_OFFSET_PTR
#define CONTAINER_OFFSET_PTR

#include "boost/shared_ptr.hpp"

namespace polar_express {

// Declared below
namespace internal {
template <typename ValueT> class ContainerOffsetPtrImplBase;
template <typename ContainerT> class ContainerOffsetPtrImpl;
}  // namespace internal

// A smart-pointer-like wrapper for the concept of a fixed offset into
// an STL-style container. The container can reallocate its underlying
// data storage, but so long as the container itself is not
// deallocated, and so long as the overall container size does not
// become smaller than the offset, the container offset pointer
// remains valid. If either of these two conditions are violated,
// behavior is undefined.
//
// The container offset pointer is copyable and intended to be passed
// around by value. The user of the offset pointer should not need to
// know what type of container is being used.
//
// Obviously this is very inefficient if the container is not random
// access. Don't use this with linked lists.
//
// Since the ContainerOffsetPtr class is not templated on container
// type, you cannot directly construct a ContainerOffsetPtr
// object. Instead, use the make_offset_ptr utility function (defined
// below).
template <typename ValueT>
class ContainerOffsetPtr {
 public:
  ValueT* get() const {
    return impl_->get();
  }

  ValueT* operator->() const {
    return get();
  }

  ValueT& operator*() const {
    return *get();
  }

  bool operator==(const ValueT* rhs) const {
    return get() == rhs;
  }

  bool operator!=(const ValueT* rhs) const {
    return get() != rhs;
  }

 private:
  ContainerOffsetPtr(
      boost::shared_ptr<internal::ContainerOffsetPtrImpl<ValueT> > impl)
      : impl_(impl) {
  }

  boost::shared_ptr<internal::ContainerOffsetPtrImplBase<ValueT> > impl_;

  template<typename ContainerT>
  friend ContainerOffsetPtr make_offset_ptr(
      boost::shared_ptr<ContainerT> container, size_t offset);
};

namespace internal {

// Abstract base class for implementation class that relies only on
// value type, not container type.
template <typename ValueT>
class ContainerOffsetPtrImplBase {
 public:
  virtual ValueT* get() const = 0;
};

// Actual implementation class, specific to a type of container.
template <typename ContainerT>
class ContainerOffsetPtrImpl
    : public ContainerOffsetPtrImplBase<typename ContainerT::value_type> {
 public:
  ContainerOffsetPtrImpl(boost::shared_ptr<ContainerT> container, size_t offset)
      : container_(container),
        offset_(offset) {
  }

  virtual typename ContainerT::value_type* get() const {
    return &(*(container_->begin() + offset_));
  }

 private:
  boost::shared_ptr<ContainerT> container_;
  size_t offset_;
};

}  // namespace internal

template <typename ContainerT>
ContainerOffsetPtr<typename ContainerT::value_type> make_offset_ptr(
    boost::shared_ptr<ContainerT> container, size_t offset) {
  return ContainerOffsetPtr<typename ContainerT::value_type>(
      boost::shared_ptr<
        internal::ContainerOffsetPtrImplBase<typename ContainerT::value_type> >(
            new internal::ContainerOffsetPtrImpl<ContainerT>(
                container, offset)));
}

}  // namespace polar_express

#endif  // CONTAINER_OFFSET_PTR
