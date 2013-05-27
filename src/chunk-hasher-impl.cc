#include "chunk-hasher-impl.h"

#include <ctime>
#include <cstdlib>

#include "crypto++/hex.h"
#include "crypto++/sha.h"

#include "chunk-reader.h"
#include "proto/snapshot.pb.h"

namespace polar_express {
namespace {

// TODO: Should be configurable.
const size_t kBlockSizeBytes = 1024 * 1024;  // 1 MiB

template <int N>
void WriteHashToString(unsigned char (&raw_digest)[N], string* str) {
  CryptoPP::HexEncoder encoder;
  encoder.Attach(new CryptoPP::StringSink(*str));
  encoder.Put(raw_digest, N);
  encoder.MessageEnd();
}

}  // namespace

ChunkHasherImpl::ChunkHasherImpl()
  : ChunkHasher(false),
    whole_file_sha1_engine_(new CryptoPP::SHA1) {
}

ChunkHasherImpl::~ChunkHasherImpl() {
}

void ChunkHasherImpl::GenerateAndHashChunks(
    const boost::filesystem::path& path,
    boost::shared_ptr<Snapshot> snapshot, Callback callback) {
  ContinueGeneratingAndHashingChunks(
      boost::shared_ptr<Context>(new Context(path, snapshot, callback)));
}

void ChunkHasherImpl::ValidateHash(
    const Chunk& chunk, const vector<char>& block_data_for_chunk,
    bool* is_valid, Callback callback) {
  string block_data_sha1_digest;
  HashData(block_data_for_chunk, &block_data_sha1_digest);
  *CHECK_NOTNULL(is_valid) =
      (block_data_sha1_digest == chunk.block().sha1_digest());
  callback();
}

void ChunkHasherImpl::ContinueGeneratingAndHashingChunks(
    boost::shared_ptr<Context> context) {
  int64_t offset = 0;
  if (context->current_chunk_ != nullptr) {
    offset = context->current_chunk_->offset() +
        context->current_chunk_->block().length();
  }

  context->current_chunk_ = context->snapshot_->add_chunks();
  context->current_chunk_->set_offset(offset);

  // Ask for a block of the default size. If EOF is reached, then the
  // reader will return less than this many bytes.
  context->current_chunk_->mutable_block()->set_length(kBlockSizeBytes);

  context->block_data_buffer_.clear();
  context->chunk_reader_->ReadBlockDataForChunk(
      *context->current_chunk_, &context->block_data_buffer_,
      bind(&ChunkHasherImpl::UpdateHashesFromBlockData, this, context));
}

void ChunkHasherImpl::UpdateHashesFromBlockData(
    boost::shared_ptr<Context> context) {
  // If the chunk reader returned less data than we requested, this
  // means that it hit EOF.
  size_t expected_data_length = context->current_chunk_->block().length();

  if (context->block_data_buffer_.empty()) {
    // Do not generate chunks for empty blocks.
    context->snapshot_->mutable_chunks()->RemoveLast();
  } else {
    context->current_chunk_->set_observation_time(time(nullptr));

    Block* current_block = context->current_chunk_->mutable_block();
    current_block->set_length(context->block_data_buffer_.size());

    HashData(context->block_data_buffer_, current_block->mutable_sha1_digest());
    UpdateWholeFileHash(context->block_data_buffer_);
  }

  if (context->block_data_buffer_.size() < expected_data_length) {
    WriteWholeFileHash(context->snapshot_->mutable_sha1_digest());
    context->callback_();
  } else {
    ContinueGeneratingAndHashingChunks(context);
  }
}

void ChunkHasherImpl::HashData(
    const vector<char>& data, string* sha1_digest) const {
  CryptoPP::SHA1 sha1_engine;
  unsigned char raw_digest[CryptoPP::SHA1::DIGESTSIZE];
  sha1_engine.CalculateDigest(
      raw_digest,
      reinterpret_cast<const unsigned char*>(data.data()),
      data.size());
  WriteHashToString(raw_digest, sha1_digest);
}

void ChunkHasherImpl::UpdateWholeFileHash(const vector<char>& data) {
  whole_file_sha1_engine_->Update(
      reinterpret_cast<const unsigned char*>(data.data()), data.size());
}

void ChunkHasherImpl::WriteWholeFileHash(string* sha1_digest) const {
  unsigned char raw_digest[CryptoPP::SHA1::DIGESTSIZE];
  whole_file_sha1_engine_->Final(raw_digest);
  WriteHashToString(raw_digest, sha1_digest);
}

ChunkHasherImpl::Context::Context(
    const boost::filesystem::path& path, boost::shared_ptr<Snapshot> snapshot,
    Callback callback)
    : path_(path),
      snapshot_(snapshot),
      chunk_reader_(ChunkReader::CreateChunkReaderForPath(path).release()),
      current_chunk_(nullptr),
      callback_(callback) {
}

}  // namespace polar_express
