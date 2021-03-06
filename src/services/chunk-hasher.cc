#include "services/chunk-hasher.h"

#include "base/asio-dispatcher.h"
#include "proto/block.pb.h"
#include "services/chunk-hasher-impl.h"

namespace polar_express {

ChunkHasher::ChunkHasher()
    : impl_(new ChunkHasherImpl) {
}

ChunkHasher::ChunkHasher(bool create_impl)
    : impl_(create_impl ? new ChunkHasherImpl : nullptr) {
}

ChunkHasher::~ChunkHasher() {
}

void ChunkHasher::GenerateAndHashChunks(
    const boost::filesystem::path& path,
    boost::shared_ptr<Snapshot> snapshot, Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&ChunkHasher::GenerateAndHashChunks,
           impl_.get(), path, snapshot, callback));
}

void ChunkHasher::ValidateHash(
    const Chunk& chunk, const vector<byte>& block_data_for_chunk,
    bool* is_valid, Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&ChunkHasher::ValidateHash,
           impl_.get(), chunk, block_data_for_chunk, is_valid, callback));
}

}  // namespace polar_express

