#include "hasher-impl.h"

#include "crypto++/hex.h"
#include "crypto++/sha.h"

namespace polar_express {
namespace {

// TODO(tylermchenry): Duplicated in chunk-hasher-impl.cc; consolidate.
template <int N>
void WriteHashToString(unsigned char (&raw_digest)[N], string* str) {
  CryptoPP::HexEncoder encoder;
  encoder.Attach(new CryptoPP::StringSink(*CHECK_NOTNULL(str)));
  encoder.Put(raw_digest, N);
  encoder.MessageEnd();
}

}  // namespace

HasherImpl::HasherImpl()
    : Hasher(false) {
}

HasherImpl::~HasherImpl() {
}

void HasherImpl::ComputeHash(
    const string& data, string* sha1_digest, Callback callback) {
  HashData(data, sha1_digest);
  callback();
}

void HasherImpl::ValidateHash(
    const string& data, const string& sha1_digest, bool* is_valid,
    Callback callback) {
  string tmp_sha1_digest;
  HashData(data, &tmp_sha1_digest);
  *CHECK_NOTNULL(is_valid) = (sha1_digest == tmp_sha1_digest);
  callback();
}

void HasherImpl::HashData(const string& data, string* sha1_digest) const {
  CryptoPP::SHA1 sha1_engine;
  unsigned char raw_digest[CryptoPP::SHA1::DIGESTSIZE];
  sha1_engine.CalculateDigest(
      raw_digest,
      reinterpret_cast<const unsigned char*>(data.data()),
      data.size());
  WriteHashToString(raw_digest, sha1_digest);
}

}  // namespace polar_express
