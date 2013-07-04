#ifndef HASHER_H
#define HASHER_H

#include <memory>
#include <string>

#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "macros.h"

namespace polar_express {

class HasherImpl;

// Generic hashing class, not specific to any particular proto, good
// for generating a simple one-shot digest of a chunk of data.
class Hasher {
 public:
  Hasher();
  virtual ~Hasher();

  virtual void ComputeHash(
      const string& data, string* sha1_digest, Callback callback);

  virtual void ValidateHash(
      const string& data, const string& sha1_digest, bool* is_valid,
      Callback callback);

 protected:
  explicit Hasher(bool create_impl);

 private:
  unique_ptr<HasherImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(Hasher);
};

}   // namespace chunk_hasher

#endif  // HASHER_H
