#include "chunk-hasher.h"

#include "asio-dispatcher.h"
#include "chunk-hasher-impl.h"

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
    const boost::filesystem::path& path, Callback callback) {
  AsioDispatcher::GetInstance()->PostDiskBound(
      bind(&ChunkHasher::GenerateAndHashChunks,
           impl_.get(), path, callback));
}

void ChunkHasher::GetGeneratedAndHashedChunks(
    vector<boost::shared_ptr<Chunk> >* chunks) const {
  impl_->GetGeneratedAndHashedChunks(chunks);
}
 
}  // namespace polar_express

