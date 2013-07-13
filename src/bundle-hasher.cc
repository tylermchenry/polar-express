#include "bundle-hasher.h"

#include "asio-dispatcher.h"
#include "hasher-impl.h"

namespace polar_express {

BundleHasher::BundleHasher()
    : impl_(new BundleHasherImpl) {
}

BundleHasher::BundleHasher(bool create_impl)
    : impl_(create_impl ? new BundleHasherImpl : nullptr) {
}

BundleHasher::~BundleHasher() {
}

void BundleHasher::ComputeHash(
    const vector<byte>& data, string* sha1_digest, Callback callback) {
  ComputeSequentialHash({ &data }, sha1_digest, callback);
}

void BundleHasher::ComputeSequentialHash(
    const vector<const vector<byte>*>& sequential_data,
    string* sha1_digest, Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&BundleHasher::ComputeSequentialHash,
           impl_.get(), sequential_data, sha1_digest, callback));
}

void BundleHasher::ValidateHash(
    const vector<byte>& data, const string& sha1_digest, bool* is_valid,
    Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&BundleHasher::ValidateHash,
           impl_.get(), data, sha1_digest, is_valid, callback));
}

}  // namespace polar_express

