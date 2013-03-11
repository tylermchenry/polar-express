#include "chunk-hasher-impl.h"

#include <ctime>

#include "crypto++/hex.h"
#include "crypto++/sha.h"

#include "proto/block.pb.h"

namespace polar_express {

const size_t ChunkHasherImpl::kBlockSizeBytes = 1024 * 1024;  // 1 MiB

ChunkHasherImpl::ChunkHasherImpl()
  : ChunkHasher(false) {
}

ChunkHasherImpl::~ChunkHasherImpl() {
}

void ChunkHasherImpl::GenerateAndHashChunks(
    const boost::filesystem::path& path, Callback callback) {
  try {
    boost::iostreams::mapped_file mapped_file(path.string(), ios_base::in);
    if (mapped_file.is_open()) {
      size_t offset = 0;
      while (offset < mapped_file.size()) {
        boost::shared_ptr<Chunk> chunk(new Chunk);
        chunk->set_offset(offset);
        FillCandidateBlock(
            mapped_file, offset, chunk->mutable_candidate_block());
        chunk->set_observation_time(time(NULL));
        chunks_.push_back(chunk);
        offset += kBlockSizeBytes;
      }
    }
  } catch (...) {
    // TODO: Do something sane here.
  }
  callback();
}

void ChunkHasherImpl::GetGeneratedAndHashedChunks(
    vector<boost::shared_ptr<Chunk> >* chunks) const {
  CHECK_NOTNULL(chunks)->insert(chunks->end(), chunks_.begin(), chunks_.end());
}

void ChunkHasherImpl::FillCandidateBlock(
    const boost::iostreams::mapped_file& mapped_file, size_t offset,
    Block* candidate_block) const {
  candidate_block->set_length(
      min(kBlockSizeBytes, mapped_file.size() - offset));
  HashData(
      mapped_file.const_data() + offset, candidate_block->length(),
      candidate_block->mutable_sha1_digest());
}

void ChunkHasherImpl::HashData(
    const char* data_start, size_t data_length,
    string* sha1_digest) const {
  CryptoPP::SHA1 sha1_engine;
  unsigned char raw_digest[CryptoPP::SHA1::DIGESTSIZE];
  sha1_engine.CalculateDigest(
      raw_digest,
      reinterpret_cast<const unsigned char*>(data_start),
      data_length);

  CryptoPP::HexEncoder encoder;
  encoder.Attach(new CryptoPP::StringSink(*sha1_digest));
  encoder.Put(raw_digest, sizeof(raw_digest));
  encoder.MessageEnd();
}

}  // namespace polar_express
