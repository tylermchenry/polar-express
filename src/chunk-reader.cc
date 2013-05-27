#include "chunk-reader.h"

#include "asio-dispatcher.h"
#include "chunk-reader-impl.h"
#include "proto/block.pb.h"

namespace polar_express {

ChunkReader::ChunkReader(const boost::filesystem::path& path)
    : impl_(new ChunkReaderImpl(path)) {
}

ChunkReader::ChunkReader(
    const boost::filesystem::path& path, bool create_impl)
    : impl_(create_impl ? new ChunkReaderImpl(path) : nullptr) {
}

ChunkReader::~ChunkReader() {
}

void ChunkReader::ReadBlockDataForChunk(
    const Chunk& chunk, vector<char>* block_data_for_chunk, Callback callback) {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&ChunkReader::ReadBlockDataForChunk,
           impl_.get(), chunk, block_data_for_chunk, callback));
}

}  // namespace polar_express

