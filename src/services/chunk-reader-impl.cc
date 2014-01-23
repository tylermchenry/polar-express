#include "services/chunk-reader-impl.h"

#include <boost/iostreams/device/mapped_file.hpp>

#include <crypto++/hex.h>
#include <crypto++/sha.h>

#include "proto/snapshot.pb.h"

namespace polar_express {

ChunkReaderImpl::ChunkReaderImpl(const boost::filesystem::path& path)
  : ChunkReader(path, false),
    mapped_file_(
        new boost::iostreams::mapped_file(path.string(), ios_base::in)) {
}

ChunkReaderImpl::~ChunkReaderImpl() {
}

void ChunkReaderImpl::ReadBlockDataForChunk(
    const Chunk& chunk, vector<byte>* block_data_for_chunk,
    Callback callback) {
  CHECK_NOTNULL(block_data_for_chunk)->clear();
  try {
    if (mapped_file_->is_open() &&
        chunk.offset() < mapped_file_->size() &&
        chunk.block().length() > 0) {
      block_data_for_chunk->assign(
          mapped_file_->const_data() + chunk.offset(),
          mapped_file_->const_data() + min<size_t>(
              chunk.offset() + chunk.block().length(), mapped_file_->size()));
    }
  } catch (...) {
    // TODO: Do something sane here.
  }

  callback();
}

}  // namespace polar_express
