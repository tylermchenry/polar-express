#ifndef BUNDLE_HASHER_IMPL_H
#define BUNDLE_HASHER_IMPL_H

#include <string>

#include <boost/shared_ptr.hpp>

#include "base/callback.h"
#include "base/macros.h"
#include "services/bundle-hasher.h"

namespace polar_express {

class BundleHasherImpl : public BundleHasher {
 public:
  BundleHasherImpl();
  virtual ~BundleHasherImpl();

  virtual void ComputeSequentialHashes(
      const vector<const vector<byte>*>& sequential_data,
      string* sha256_linear_digest, string* sha256_tree_digest,
      Callback callback);

  virtual void ValidateHashes(
      const vector<byte>* data, const string& sha256_linear_digest,
      const string& sha256_tree_digest, bool* is_valid, Callback callback);

 private:
  void HashData(const vector<const vector<byte>*>& sequential_data,
                string* sha256_linear_digest, string* sha256_tree_digest) const;

  void ComputeFinalTreeHash(
      const vector<vector<byte> >::const_iterator
        sha256_intermediate_digests_begin,
      const vector<vector<byte> >::const_iterator
        sha256_intermediate_digests_end,
      vector<byte>* sha256_tree_digest) const;

  friend class BundleHasherImplTest;
  DISALLOW_COPY_AND_ASSIGN(BundleHasherImpl);
};

}   // namespace chunk_hasher

#endif  // BUNDLE_HASHER_IMPL_H
