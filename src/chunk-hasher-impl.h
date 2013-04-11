#ifndef CHUNK_HASHER_IMPL_H
#define CHUNK_HASHER_IMPL_H

#include <cstdlib>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/iostreams/device/mapped_file.hpp"
#include "boost/scoped_ptr.hpp"
#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "chunk-hasher.h"
#include "macros.h"

namespace polar_express {

class Block;
class Chunk;

class ChunkHasherImpl : public ChunkHasher {
 public:
  ChunkHasherImpl();
  virtual ~ChunkHasherImpl();

  virtual void GenerateAndHashChunks(
      const boost::filesystem::path& path,
      boost::shared_ptr<Snapshot> snapshot, Callback callback);
  
 private:
  void FillBlock(
      const boost::iostreams::mapped_file& mapped_file, size_t offset,
      Block* block) const;

  void HashData(
      const char* data_start, size_t data_length,
      string* sha1_digest) const;

  static const size_t kBlockSizeBytes;
  
  DISALLOW_COPY_AND_ASSIGN(ChunkHasherImpl);
};

}   // namespace chunk_hasher

#endif  // CHUNK_HASHER_IMPL_H