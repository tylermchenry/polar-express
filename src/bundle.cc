#include "bundle.h"

#include <ctime>

#include <unistd.h>

#include "crypto++/hex.h"
#include "crypto++/sha.h"

#include "proto/block.pb.h"
#include "tar-header-block.h"

namespace polar_express {
namespace {

const char kPayloadFilenamePrefix[] = "payload_";
const char kPayloadFilenameSuffix[] = ".dat";
const char kManifestFilename[] = "manifest.pbuf";
const char kManifestDigestFilename[] = "manifest_digest.sha1";

}  // namespace

Bundle::Bundle()
    : is_finalized_(false),
      current_payload_(nullptr),
      next_payload_id_(0),
      data_(new vector<char>) {
}

Bundle::~Bundle() {
}

const BundleManifest& Bundle::manifest() const {
  return manifest_;
}

size_t Bundle::size() const {
  return data_->size();
}

const vector<char>& Bundle::data() const {
  return *data_;
}

bool Bundle::is_finalized() const {
  return is_finalized_;
}

void Bundle::StartNewPayload(BundlePayload::CompressionType compression_type) {
  assert(!is_finalized_);
  EndCurrentPayload();
  current_payload_ = manifest_.add_payloads();
  current_payload_->set_id(next_payload_id_++);
  current_payload_->set_offset(size());
  current_payload_->set_compression_type(compression_type);
  StartNewFile(kPayloadFilenamePrefix + to_string(current_payload_->id()) +
               kPayloadFilenameSuffix);
}

void Bundle::AddBlockMetadata(const Block& block) {
  assert(!is_finalized_);
  assert(current_payload_ != nullptr);
  current_payload_->add_blocks()->CopyFrom(block);
}

void Bundle::AppendBlockContents(const vector<char>& compressed_contents) {
  assert(!is_finalized_);
  assert(current_payload_ != nullptr);
  data_->insert(
      data_->end(), compressed_contents.begin(), compressed_contents.end());
}

void Bundle::Finalize() {
  assert(!is_finalized_);
  EndCurrentPayload();
  AppendSerializedManifest();
  is_finalized_ = true;
}

void Bundle::StartNewFile(const string& filename) {
  assert(current_tar_header_block_ == nullptr);
  size_t offset = size();

  data_->insert(data_->begin() + current_payload_->offset(),
                TarHeaderBlock::kTarHeaderBlockLength, '\0');
  current_tar_header_block_.reset(
      new TarHeaderBlock(make_offset_ptr(data_, offset)));

  current_tar_header_block_->set_filename(filename);
  current_tar_header_block_->set_mode(0400);  // Read-only by owner only.
  current_tar_header_block_->set_owner_uid(getuid());
  current_tar_header_block_->set_owner_gid(getgid());
}

void Bundle::EndCurrentFile(size_t final_size) {
  if (current_tar_header_block_ == nullptr) {
    return;
  }

  current_tar_header_block_->set_size(final_size);
  current_tar_header_block_->set_modification_time(time(nullptr));
  current_tar_header_block_->ComputeAndSetChecksum();

  current_tar_header_block_.reset();
}

void Bundle::EndCurrentPayload() {
  if (current_payload_ != nullptr) {
    EndCurrentFile(size() - current_payload_->offset());
  }
  current_payload_ = nullptr;
}

void Bundle::AppendSerializedManifest() {
  string serialized_manifest;
  manifest_.SerializeToString(&serialized_manifest);

  StartNewFile(kManifestFilename);
  data_->insert(data_->end(), serialized_manifest.begin(),
               serialized_manifest.end());
  EndCurrentFile(serialized_manifest.length());

  string serialized_manifest_sha1_digest;
  CryptoPP::SHA1 sha1_engine;
  unsigned char raw_digest[CryptoPP::SHA1::DIGESTSIZE];
  sha1_engine.CalculateDigest(
      raw_digest,
      reinterpret_cast<const unsigned char*>(serialized_manifest.c_str()),
      serialized_manifest.length());

  CryptoPP::HexEncoder encoder;
  encoder.Attach(new CryptoPP::StringSink(serialized_manifest));
  encoder.Put(raw_digest, sizeof(raw_digest));
  encoder.MessageEnd();

  StartNewFile(kManifestDigestFilename);
  data_->insert(data_->end(), serialized_manifest_sha1_digest.begin(),
               serialized_manifest_sha1_digest.end());
  EndCurrentFile(serialized_manifest_sha1_digest.length());
}

}  // namespace polar_express
