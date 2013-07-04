#include "compressor.h"

#include "asio-dispatcher.h"
#include "null-compressor-impl.h"
#include "zlib-compressor-impl.h"

namespace polar_express {

// static
template<typename CompressorImplT>
unique_ptr<Compressor> Compressor::CreateCompressorWithImpl() {
  return unique_ptr<Compressor>(new Compressor(
      unique_ptr<Compressor>(new CompressorImplT)));
}

// static
unique_ptr<Compressor> Compressor::CreateCompressor(
    BundlePayload::CompressionType compression_type) {
  // TODO(tylermchenry): If in there future there are a lot of
  // different compression types, it may make sense to auto-register
  // compressor implementation classes into a map based on the result
  // of their compression_type() accessor.
  switch (compression_type) {
    case BundlePayload::COMPRESSION_TYPE_NONE:
      return CreateCompressorWithImpl<NullCompressorImpl>();
    case BundlePayload::COMPRESSION_TYPE_ZLIB:
      return CreateCompressorWithImpl<ZlibCompressorImpl>();
    default:
      assert(false);
      return nullptr;
  }
}

Compressor::Compressor() {
}

Compressor::Compressor(unique_ptr<Compressor>&& impl)
    : impl_(std::move(CHECK_NOTNULL(impl))) {
}

Compressor::~Compressor() {
}

void Compressor::CompressData(
    const vector<char>& data, vector<char>* compressed_data,
    Callback callback) {
  AsioDispatcher::GetInstance()->PostCpuBound(
      bind(&Compressor::CompressData,
           impl_.get(), data, compressed_data, callback));
}

BundlePayload::CompressionType Compressor::compression_type() const {
  return impl_->compression_type();
}

}  // namespace polar_express
