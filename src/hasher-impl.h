#ifndef HASHER_IMPL_H
#define HASHER_IMPL_H

#include <string>

#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "hasher.h"
#include "macros.h"

namespace polar_express {

class HasherImpl : public Hasher {
 public:
  HasherImpl();
  virtual ~HasherImpl();

  virtual void ComputeHash(
      const string& data, string* sha1_digest, Callback callback);

  virtual void ValidateHash(
      const string& data, const string& sha1_digest, bool* is_valid,
      Callback callback);

 private:
  void HashData(const string& data, string* sha1_digest) const;

  DISALLOW_COPY_AND_ASSIGN(HasherImpl);
};

}   // namespace chunk_hasher

#endif  // HASHER_IMPL_H
