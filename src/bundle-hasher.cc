#include "bundle-hasher.h"

#include "asio-dispatcher.h"
#include "bundle-hasher-impl.h"

namespace polar_express {

BundleHasher::BundleHasher()
    : impl_(new BundleHasherImpl) {
}

BundleHasher::BundleHasher(bool create_impl)
    : impl_(create_impl ? new BundleHasherImpl : nullptr) {
}

BundleHasher::~BundleHasher() {
}

void BundleHasher::ComputeHashes(
    const vector<byte>* data, string* sha256_linear_digest,
    string* sha256_tree_digest, Callback callback) {
  ComputeSequentialHashes({ data }, sha256_linear_digest,
                          sha256_tree_digest, callback);
}

void BundleHasher::ComputeSequentialHashes(
    const vector<const vector<byte>*>& sequential_data,
    string* sha256_linear_digest, string* sha256_tree_digest,
    Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&BundleHasher::ComputeSequentialHashes,
           impl_.get(), sequential_data, sha256_linear_digest,
           sha256_tree_digest, callback));
}

void BundleHasher::ValidateHashes(
    const vector<byte>* data, const string& sha256_linear_digest,
    const string& sha256_tree_digest, bool* is_valid, Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&BundleHasher::ValidateHashes,
           impl_.get(), data, sha256_linear_digest,
           sha256_tree_digest, is_valid, callback));
}

}  // namespace polar_express

