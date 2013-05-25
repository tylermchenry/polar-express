#ifndef OVERRIDEABLE_SCOPED_PTR_H
#define OVERRIDEABLE_SCOPED_PTR_H

#include "boost/scoped_ptr.hpp"

#include "macros.h"

namespace polar_express {

// A wrapper around boost::scoped_ptr which must be initialized with a non-null
// value, and which can be temporarily overridden (usually by a pointer to a
// mock).
template <typename T>
class OverrideableScopedPtr {
 public:
  explicit OverrideableScopedPtr(T* owned_ptr)
    : owned_ptr_(CHECK_NOTNULL(owned_ptr)),
      override_ptr_(nullptr) {
  }

  T* get() const {
    return (override_ptr_ != nullptr) ? override_ptr_ : owned_ptr_.get();
  }

  void set_override(T* override_ptr) {
    override_ptr_ = override_ptr;
  }

  T* operator->() const {
    return get();
  }

  T& operator*() const {
    return *get();
  }

  bool operator==(const T* rhs) const {
    return get() == rhs;
  }

  bool operator!=(const T* rhs) const {
    return get() != rhs;
  }

 private:
  const scoped_ptr<T> owned_ptr_;
  T* override_ptr_;

  DISALLOW_COPY_AND_ASSIGN(OverrideableScopedPtr);
};

}  // namespace polar_express

#endif  // OVERRIDEABLE_SCOPED_PTR_H
