#ifndef CHUNK_READER_IMPL_H
#define CHUNK_READER_IMPL_H

#include <cstdlib>
#include <memory>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "chunk-reader.h"
#include "macros.h"

namespace boost {
namespace iostreams {
class mapped_file;
}  // namespace iostreams
}  // namespace boost

namespace polar_express {

class Block;
class Chunk;

class ChunkReaderImpl : public ChunkReader {
 public:
  explicit ChunkReaderImpl(const boost::filesystem::path& path);
  virtual ~ChunkReaderImpl();

  virtual void ReadBlockDataForChunk(
      const Chunk& chunk, vector<char>* block_data_for_chunk,
      Callback callback);

 private:
  const unique_ptr<boost::iostreams::mapped_file> mapped_file_;

  DISALLOW_COPY_AND_ASSIGN(ChunkReaderImpl);
};

}   // namespace chunk_reader

#endif  // CHUNK_READER_IMPL_H
