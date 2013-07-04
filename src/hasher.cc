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
    const string& data, string* sha1_digest, Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&Hasher::ComputeHash,
           impl_.get(), data, sha1_digest, callback));
}

void Hasher::ValidateHash(
    const string& data, const string& sha1_digest, bool* is_valid,
    Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&Hasher::ValidateHash,
           impl_.get(), data, sha1_digest, is_valid, callback));
}

}  // namespace polar_express

