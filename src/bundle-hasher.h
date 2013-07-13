#ifndef BUNDLE_HASHER_H
#define BUNDLE_HASHER_H

#include <memory>
#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "macros.h"

namespace polar_express {

class BundleHasherImpl;

// Class for computing the hashes of bundles, prior to uploading.
class BundleHasher {
 public:
  BundleHasher();
  virtual ~BundleHasher();

  void ComputeHashes(
      const vector<byte>& data, string* sha256_linear_digest,
      string* sha256_tree_digest, Callback callback);

  // Version of ComputeHash with multiple data inputs, which are fed
  // sequentially into the hash function to create a single digest.
  virtual void ComputeSequentialHashes(
      const vector<const vector<byte>*>& sequential_data,
      string* sha256_linear_digest, string* sha256_tree_digest,
      Callback callback);

  virtual void ValidateHashes(
      const vector<byte>& data, const string& sha256_linear_digest,
      const string& sha256_tree_digest, bool* is_valid, Callback callback);

  // TODO(tylermchenry): Is a sequential version of validate needed?

 protected:
  explicit BundleHasher(bool create_impl);

 private:
  unique_ptr<BundleHasherImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(BundleHasher);
};

}   // namespace chunk_hasher

#endif  // BUNDLE_HASHER_H
