#ifndef BUNDLE_HASHER_IMPL_H
#define BUNDLE_HASHER_IMPL_H

#include <string>

#include "boost/shared_ptr.hpp"

#include "bundle-hasher.h"
#include "callback.h"
#include "macros.h"

namespace polar_express {

class BundleHasherImpl : public BundleHasher {
 public:
  BundleHasherImpl();
  virtual ~BundleHasherImpl();

  virtual void ComputeSequentialHash(
      const vector<const vector<byte>*>& sequential_data,
      string* sha1_digest, Callback callback);

  virtual void ValidateHash(
      const vector<byte>& data, const string& sha1_digest, bool* is_valid,
      Callback callback);

 private:
  void HashData(const vector<const vector<byte>*>& sequential_data,
                string* sha1_digest) const;

  DISALLOW_COPY_AND_ASSIGN(BundleHasherImpl);
};

}   // namespace chunk_hasher

#endif  // BUNDLE_HASHER_IMPL_H
