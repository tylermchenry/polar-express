#ifndef HASHER_H
#define HASHER_H

#include <memory>
#include <string>
#include <vector>

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

  void ComputeHash(
      const vector<byte>& data, string* sha1_digest, Callback callback);

  // Version of ComputeHash with multiple data inputs, which are fed
  // sequentially into the hash function to create a single digest.
  virtual void ComputeSequentialHash(
      const vector<const vector<byte>*>& sequential_data,
      string* sha1_digest, Callback callback);

  virtual void ValidateHash(
      const vector<byte>& data, const string& sha1_digest, bool* is_valid,
      Callback callback);

  // TODO(tylermchenry): Is a sequential version of validate needed?

 protected:
  explicit Hasher(bool create_impl);

 private:
  unique_ptr<HasherImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(Hasher);
};

}   // namespace chunk_hasher

#endif  // HASHER_H
