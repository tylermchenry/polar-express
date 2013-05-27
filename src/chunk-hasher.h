#ifndef CHUNK_HASHER_H
#define CHUNK_HASHER_H

#include <memory>
#include <string>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "macros.h"

namespace polar_express {

class Chunk;
class ChunkHasherImpl;
class Snapshot;

class ChunkHasher {
 public:
  ChunkHasher();
  virtual ~ChunkHasher();

  virtual void GenerateAndHashChunks(
      const boost::filesystem::path& path,
      boost::shared_ptr<Snapshot> snapshot, Callback callback);

  virtual void ValidateHash(
      const Chunk& chunk, const vector<char>& block_data_for_chunk,
      bool* is_valid, Callback callback);

 protected:
  explicit ChunkHasher(bool create_impl);

 private:
  unique_ptr<ChunkHasherImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ChunkHasher);
};

}   // namespace chunk_hasher

#endif  // CHUNK_HASHER_H
