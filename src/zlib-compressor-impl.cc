#include "zlib-compressor-impl.h"

#include "zlib.h"

namespace polar_express {

ZlibCompressorImpl::ZlibCompressorImpl() {
}

ZlibCompressorImpl::~ZlibCompressorImpl() {
}

void ZlibCompressorImpl::CompressData(
    const vector<char>& data, vector<char>* compressed_data,
    Callback callback) {
  CHECK_NOTNULL(compressed_data)->clear();
  compressed_data->resize(compressBound(data.size()));

  unsigned long int compressed_size = compressed_data->size();
  if (compress(
          reinterpret_cast<Bytef*>(compressed_data->data()), &compressed_size,
          reinterpret_cast<const Bytef*>(data.data()), data.size()) != Z_OK) {
    // TODO(tylermchenry): Reasonable error handling.
    assert(false);
  }

  compressed_data->resize(compressed_size);
  callback();
}

BundlePayload::CompressionType ZlibCompressorImpl::compression_type() const {
  return BundlePayload::COMPRESSION_TYPE_ZLIB;
}

}  // namespace polar_express
