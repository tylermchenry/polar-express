#include "chunk-hasher-impl.h"

#include <ctime>

#include "crypto++/hex.h"
#include "crypto++/sha.h"

#include "proto/snapshot.pb.h"

namespace polar_express {

const size_t ChunkHasherImpl::kBlockSizeBytes = 1024 * 1024;  // 1 MiB

ChunkHasherImpl::ChunkHasherImpl()
  : ChunkHasher(false),
    whole_file_sha1_engine_(new CryptoPP::SHA1) {
}

ChunkHasherImpl::~ChunkHasherImpl() {
}

void ChunkHasherImpl::GenerateAndHashChunks(
    const boost::filesystem::path& path,
    boost::shared_ptr<Snapshot> snapshot, Callback callback) {
  CHECK_NOTNULL(snapshot.get());
  try {
    boost::iostreams::mapped_file mapped_file(path.string(), ios_base::in);
    if (mapped_file.is_open()) {
      size_t offset = 0;
      while (offset < mapped_file.size()) {
        Chunk* chunk = snapshot->add_chunks();
        chunk->set_offset(offset);
        FillBlock(mapped_file, offset, chunk->mutable_block());
        chunk->set_observation_time(time(NULL));
        offset += kBlockSizeBytes;
      }
    }
  } catch (...) {
    // TODO: Do something sane here.
  }

  WriteWholeFileHash(snapshot->mutable_sha1_digest());
  
  callback();
}

void ChunkHasherImpl::FillBlock(
    const boost::iostreams::mapped_file& mapped_file, size_t offset,
    Block* block) {
  block->set_length(min(kBlockSizeBytes, mapped_file.size() - offset));
  HashData(mapped_file.const_data() + offset, block->length(),
           block->mutable_sha1_digest());
  UpdateWholeFileHash(mapped_file.const_data() + offset, block->length());
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

void ChunkHasherImpl::UpdateWholeFileHash(
    const char* data_start, size_t data_length) {
  whole_file_sha1_engine_->Update(
      reinterpret_cast<const unsigned char*>(data_start),
      data_length);
}

void ChunkHasherImpl::WriteWholeFileHash(string* sha1_digest) const {
  unsigned char raw_digest[CryptoPP::SHA1::DIGESTSIZE];
  whole_file_sha1_engine_->Final(raw_digest);
  
  CryptoPP::HexEncoder encoder;
  encoder.Attach(new CryptoPP::StringSink(*sha1_digest));
  encoder.Put(raw_digest, sizeof(raw_digest));
  encoder.MessageEnd();
}

}  // namespace polar_express
