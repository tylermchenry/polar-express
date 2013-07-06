#include "hasher.h"

#include "asio-dispatcher.h"
#include "hasher-impl.h"

namespace polar_express {

Hasher::Hasher()
    : impl_(new HasherImpl) {
}

Hasher::Hasher(bool create_impl)
    : impl_(create_impl ? new HasherImpl : nullptr) {
}

Hasher::~Hasher() {
}

void Hasher::ComputeHash(
    const vector<byte>& data, string* sha1_digest, Callback callback) {
  ComputeSequentialHash({ &data }, sha1_digest, callback);
}

void Hasher::ComputeSequentialHash(
    const vector<const vector<byte>*>& sequential_data,
    string* sha1_digest, Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&Hasher::ComputeSequentialHash,
           impl_.get(), sequential_data, sha1_digest, callback));
}

void Hasher::ValidateHash(
    const vector<byte>& data, const string& sha1_digest, bool* is_valid,
    Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&Hasher::ValidateHash,
           impl_.get(), data, sha1_digest, is_valid, callback));
}

}  // namespace polar_express

