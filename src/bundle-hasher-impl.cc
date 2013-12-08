#include "bundle-hasher-impl.h"

#include "crypto++/hex.h"
#include "crypto++/sha.h"

namespace polar_express {
namespace {

// Defined by Amazon AWS API
const size_t kTreeHashIntermediateDigestDataSize = 1024 * 1024;  // 1 MiB

void WriteHashToString(const byte* raw_digest, size_t n, string* str) {
  CryptoPP::HexEncoder encoder;
  encoder.Attach(new CryptoPP::StringSink(*CHECK_NOTNULL(str)));
  encoder.Put(raw_digest, n);
  encoder.MessageEnd();
}

// TODO(tylermchenry): Duplicated in chunk-hasher-impl.cc; consolidate.
template <int N>
void WriteHashToString(const byte (&raw_digest)[N], string* str) {
  WriteHashToString(raw_digest, N, str);
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
    const vector<byte>* data, const string& sha256_linear_digest,
    const string& sha256_tree_digest, bool* is_valid, Callback callback) {
  string tmp_sha256_linear_digest;
  string tmp_sha256_tree_digest;
  HashData({ data }, &tmp_sha256_linear_digest, &tmp_sha256_tree_digest);
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
  vector<vector<byte> > sha256_tree_intermediate_digests;
  size_t bytes_in_current_intermediate_digest = 0;

  for (const auto* data : sequential_data) {
    if (data->empty()) {
      continue;
    }

    sha256_linear_engine.Update(data->data(), data->size());

    // For the tree digest we need to compute a digest of each 1 MiB
    // peice of the data. But here the data is spread out over
    // multiple byte vectors which may not each be a multiple of 1
    // MiB. So do this:
    //
    // 1. Check if there is some leftover data from the previous
    // vector in the tree engine. If so, add to it, up to a complete
    // peice (or the end of the current vector).
    //
    // 2. If we made a complete peice this way, compute its digest and
    // clear the engine.
    //
    // 3. For the remaining data in the current vector, compute
    // digests of each 1 MiB peice, until there is less than 1 MiB
    // remaining.
    //
    // 4. Put whatever data is remaining into the engine, but do not
    // finalize.
    //
    // TODO(tylermchenry): Gah, there has to be a cleaner way to write this!
    size_t offset = 0;
    if (bytes_in_current_intermediate_digest > 0) {
      offset = std::min(data->size(), kTreeHashIntermediateDigestDataSize);
      sha256_tree_engine.Update(&(*data)[0], offset);
      bytes_in_current_intermediate_digest += offset;
    }
    if (bytes_in_current_intermediate_digest ==
        kTreeHashIntermediateDigestDataSize) {
      vector<byte> intermediate_digest(CryptoPP::SHA256::DIGESTSIZE);
      sha256_tree_engine.Final(intermediate_digest.data());
      sha256_tree_intermediate_digests.push_back(intermediate_digest);
      bytes_in_current_intermediate_digest = 0;
    }
    // Mind the unsigned arithmetic!
    if (data->size() >= kTreeHashIntermediateDigestDataSize) {
      for (; offset <= data->size() - kTreeHashIntermediateDigestDataSize;
           offset += kTreeHashIntermediateDigestDataSize) {
        vector<byte> intermediate_digest(CryptoPP::SHA256::DIGESTSIZE);
        sha256_tree_engine.CalculateDigest(
            intermediate_digest.data(), &(*data)[offset],
            kTreeHashIntermediateDigestDataSize);
        sha256_tree_intermediate_digests.push_back(intermediate_digest);
      }
    }
    if (offset != data->size()) {
      sha256_tree_engine.Update(&(*data)[offset], data->size() - offset);
      bytes_in_current_intermediate_digest = data->size() - offset;
    }
  }

  // If there is any leftover data which does not add up to a complete
  // 1 MiB peice, compute its digest. The Glacier tree hash algorithm
  // allows the final peice (but only the final peice!) to be short.
  if (bytes_in_current_intermediate_digest != 0) {
    vector<byte> intermediate_digest(CryptoPP::SHA256::DIGESTSIZE);
    sha256_tree_engine.Final(intermediate_digest.data());
    sha256_tree_intermediate_digests.push_back(intermediate_digest);
  }

  byte raw_digest[CryptoPP::SHA256::DIGESTSIZE];
  sha256_linear_engine.Final(raw_digest);
  WriteHashToString(raw_digest, CHECK_NOTNULL(sha256_linear_digest));

  vector<byte> sha256_tree_digest_vector(CryptoPP::SHA256::DIGESTSIZE);
  ComputeFinalTreeHash(
      sha256_tree_intermediate_digests.begin(),
      sha256_tree_intermediate_digests.end(),
      &sha256_tree_digest_vector);
  WriteHashToString(
      sha256_tree_digest_vector.data(),
      sha256_tree_digest_vector.size(),
      sha256_tree_digest);
}

// The tree digest is an Amazon-specific thing. See here for description:
// http://docs.aws.amazon.com/amazonglacier/latest/dev/checksum-calculations.html
//
// TODO(tylermchenry): Probably some performance improvements to be made here.
void BundleHasherImpl::ComputeFinalTreeHash(
    const vector<vector<byte> >::const_iterator
      sha256_intermediate_digests_begin,
    const vector<vector<byte> >::const_iterator
      sha256_intermediate_digests_end,
    vector<byte>* sha256_tree_digest) const {
  const size_t num_intermediate_digests = std::distance(
      sha256_intermediate_digests_begin, sha256_intermediate_digests_end);

  if (num_intermediate_digests == 0) {
    // Should never happen.
    return;
  } else if (num_intermediate_digests == 1) {
    // Base case of only one intermediate digest left.
    *sha256_tree_digest = *sha256_intermediate_digests_begin;
    return;
  }

  vector<vector<byte> > next_level_sha256_intermediate_digests;
  for (auto left_digest_itr = sha256_intermediate_digests_begin;
       left_digest_itr < sha256_intermediate_digests_end - 1;
       std::advance(left_digest_itr, 2)) {
    auto right_digest_itr = left_digest_itr;
    std::advance(right_digest_itr, 1);

    CryptoPP::SHA256 sha256_engine;
    sha256_engine.Update(left_digest_itr->data(), left_digest_itr->size());
    sha256_engine.Update(right_digest_itr->data(), right_digest_itr->size());

    vector<byte> next_level_sha256_intermediate_digest(
        CryptoPP::SHA256::DIGESTSIZE);
    sha256_engine.Final(next_level_sha256_intermediate_digest.data());

    next_level_sha256_intermediate_digests.push_back(
        next_level_sha256_intermediate_digest);
  }

  if (num_intermediate_digests % 2 != 0) {
    auto last_digest_itr = sha256_intermediate_digests_end - 1;
    next_level_sha256_intermediate_digests.push_back(*last_digest_itr);
  }

  ComputeFinalTreeHash(
      next_level_sha256_intermediate_digests.begin(),
      next_level_sha256_intermediate_digests.end(),
      sha256_tree_digest);
}

}  // namespace polar_express
