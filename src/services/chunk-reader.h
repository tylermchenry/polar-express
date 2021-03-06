#ifndef CHUNK_READER_H
#define CHUNK_READER_H

#include <memory>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>

#include "base/callback.h"
#include "base/macros.h"

namespace polar_express {

class Chunk;
class ChunkReaderImpl;

class ChunkReader {
 public:
  // Factory method so that we can mock these later.
  static unique_ptr<ChunkReader> CreateChunkReaderForPath(
      const boost::filesystem::path& path);
  virtual ~ChunkReader();

  virtual void ReadBlockDataForChunk(
      const Chunk& chunk, vector<byte>* block_data_for_chunk,
      Callback callback);

 protected:
  explicit ChunkReader(const boost::filesystem::path& path);
  ChunkReader(const boost::filesystem::path& path, bool create_impl);

 private:
  unique_ptr<ChunkReaderImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ChunkReader);
};

}   // namespace chunk_reader

#endif  // CHUNK_READER_H
