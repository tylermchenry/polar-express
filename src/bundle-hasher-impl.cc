#include "bundle-hasher-impl.h"

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

BundleHasherImpl::BundleHasherImpl()
    : BundleHasher(false) {
}

BundleHasherImpl::~BundleHasherImpl() {
}

void BundleHasherImpl::ComputeSequentialHash(
    const vector<const vector<byte>*>& sequential_data,
    string* sha1_digest, Callback callback) {
  HashData(sequential_data, sha1_digest);
  callback();
}

void BundleHasherImpl::ValidateHash(
    const vector<byte>& data, const string& sha1_digest, bool* is_valid,
    Callback callback) {
  string tmp_sha1_digest;
  HashData({ &data }, &tmp_sha1_digest);
  *CHECK_NOTNULL(is_valid) = (sha1_digest == tmp_sha1_digest);
  callback();
}

void BundleHasherImpl::HashData(
    const vector<const vector<byte>*>& sequential_data,
    string* sha1_digest) const {
  CryptoPP::SHA1 sha1_engine;
  for (const auto* data : sequential_data) {
    sha1_engine.Update(data->data(), data->size());
  }

  unsigned char raw_digest[CryptoPP::SHA1::DIGESTSIZE];
  sha1_engine.Final(raw_digest);
  WriteHashToString(raw_digest, CHECK_NOTNULL(sha1_digest));
}

}  // namespace polar_express
