#include "bundle-hasher-impl.h"

#include "crypto++/hex.h"
#include "crypto++/sha.h"

namespace polar_express {
namespace {

// Defined by Amazon AWS API
const size_t kTreeHashIntermediateDigestDataSize = 1024 * 1024;  // 1 MiB

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

void BundleHasherImpl::ComputeSequentialHashes(
    const vector<const vector<byte>*>& sequential_data,
    string* sha256_linear_digest, string* sha256_tree_digest,
    Callback callback) {
  HashData(sequential_data, sha256_linear_digest, sha256_tree_digest);
  callback();
}

void BundleHasherImpl::ValidateHashes(
    const vector<byte>& data, const string& sha256_linear_digest,
    const string& sha256_tree_digest, bool* is_valid, Callback callback) {
  string tmp_sha256_linear_digest;
  string tmp_sha256_tree_digest;
  HashData({ &data }, &tmp_sha256_linear_digest, &tmp_sha256_tree_digest);
  *CHECK_NOTNULL(is_valid) =
      (sha256_linear_digest == tmp_sha256_linear_digest) &&
      (sha256_tree_digest == tmp_sha256_tree_digest);
  callback();
}

void BundleHasherImpl::HashData(
    const vector<const vector<byte>*>& sequential_data,
    string* sha256_linear_digest, string* sha256_tree_digest) const {
  CryptoPP::SHA256 sha256_linear_engine;
  CryptoPP::SHA256 sha256_tree_engine;
  vector<string> sha256_tree_intermediate_digests;
  size_t bytes_in_current_intermediate_digest = 0;
  unsigned char raw_digest[CryptoPP::SHA1::DIGESTSIZE];

  for (const auto* data : sequential_data) {
    if (data->empty()) {
      continue;
    }

    sha256_linear_engine.Update(data->data(), data->size());

    // TODO(tylermchenry): Gah, there has to be a cleaner way to write this!
    size_t offset = 0;
    if (bytes_in_current_intermediate_digest > 0) {
      offset = std::min(data->size(), kTreeHashIntermediateDigestDataSize);
      sha256_tree_engine.Update(&(*data)[0], offset);
      bytes_in_current_intermediate_digest += offset;
    }
    if (bytes_in_current_intermediate_digest ==
        kTreeHashIntermediateDigestDataSize) {
      string intermediate_digest;
      WriteHashToString(raw_digest, &intermediate_digest);
      sha256_tree_intermediate_digests.push_back(intermediate_digest);
      bytes_in_current_intermediate_digest = 0;
    }
    for (; offset <= data->size() - kTreeHashIntermediateDigestDataSize;
         offset += kTreeHashIntermediateDigestDataSize) {
      sha256_tree_engine.CalculateDigest(
          raw_digest, &(*data)[offset], kTreeHashIntermediateDigestDataSize);
      string intermediate_digest;
      WriteHashToString(raw_digest, &intermediate_digest);
      sha256_tree_intermediate_digests.push_back(intermediate_digest);
    }
    if (offset != data->size()) {
      sha256_tree_engine.Update(&(*data)[offset], data->size() - offset);
      bytes_in_current_intermediate_digest = data->size() - offset;
    }
  }

  if (bytes_in_current_intermediate_digest != 0) {
    string intermediate_digest;
    WriteHashToString(raw_digest, &intermediate_digest);
    sha256_tree_intermediate_digests.push_back(intermediate_digest);
  }

  sha256_linear_engine.Final(raw_digest);
  WriteHashToString(raw_digest, CHECK_NOTNULL(sha256_linear_digest));

  ComputeFinalTreeHash(
      sha256_tree_intermediate_digests.begin(),
      sha256_tree_intermediate_digests.end(),
      sha256_tree_digest);
}

// TODO(tylermchenry): Probably some performance improvements to be made here.
void BundleHasherImpl::ComputeFinalTreeHash(
    const vector<string>::const_iterator sha256_intermediate_digests_begin,
    const vector<string>::const_iterator sha256_intermediate_digests_end,
    string* sha256_tree_digest) const {
  const size_t num_intermediate_digests = std::distance(
      sha256_intermediate_digests_begin, sha256_intermediate_digests_end);

  string sha256_left_intermediate_digest;
  string sha256_right_intermediate_digest;
  if (num_intermediate_digests == 2) {
    sha256_left_intermediate_digest = *sha256_intermediate_digests_begin;
    sha256_right_intermediate_digest = *(sha256_intermediate_digests_end - 1);
  } else if (num_intermediate_digests % 2 != 0) {
    ComputeFinalTreeHash(
        sha256_intermediate_digests_begin,
        sha256_intermediate_digests_end - 1,
        &sha256_left_intermediate_digest);
    sha256_right_intermediate_digest = *(sha256_intermediate_digests_end - 1);
  } else {
    ComputeFinalTreeHash(
        sha256_intermediate_digests_begin,
        sha256_intermediate_digests_begin + (num_intermediate_digests / 2),
        &sha256_left_intermediate_digest);
    ComputeFinalTreeHash(
        sha256_intermediate_digests_begin + (num_intermediate_digests / 2),
        sha256_intermediate_digests_end,
        &sha256_right_intermediate_digest);
  }

  CryptoPP::SHA256 sha256_engine;
  sha256_engine.Update(
      reinterpret_cast<const byte*>(sha256_left_intermediate_digest.data()),
      sha256_left_intermediate_digest.size());
  sha256_engine.Update(
      reinterpret_cast<const byte*>(sha256_right_intermediate_digest.data()),
      sha256_right_intermediate_digest.size());

  unsigned char raw_digest[CryptoPP::SHA1::DIGESTSIZE];
  sha256_engine.Final(raw_digest);
  WriteHashToString(raw_digest, CHECK_NOTNULL(sha256_tree_digest));
}

}  // namespace polar_express
