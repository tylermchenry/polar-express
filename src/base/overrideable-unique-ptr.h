#ifndef OVERRIDEABLE_UNIQUE_PTR_H
#define OVERRIDEABLE_UNIQUE_PTR_H

#include <memory>

#include "base/macros.h"

namespace polar_express {

// A wrapper around boost::unique_ptr which must be initialized with a non-null
// value, and which can be temporarily overridden (usually by a pointer to a
// mock).
template <typename T>
class OverrideableUniquePtr {
 public:
  explicit OverrideableUniquePtr(T* owned_ptr)
    : owned_ptr_(CHECK_NOTNULL(owned_ptr)),
      override_ptr_(nullptr) {
  }

  explicit OverrideableUniquePtr(unique_ptr<T>&& owned_ptr)
    : owned_ptr_(std::move(CHECK_NOTNULL(owned_ptr))),
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
  const unique_ptr<T> owned_ptr_;
  T* override_ptr_;

  DISALLOW_COPY_AND_ASSIGN(OverrideableUniquePtr);
};

}  // namespace polar_express

#endif  // OVERRIDEABLE_UNIQUE_PTR_H
