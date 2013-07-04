#include "null-compressor-impl.h"

namespace polar_express {

NullCompressorImpl::NullCompressorImpl() {
}

NullCompressorImpl::~NullCompressorImpl() {
}

void NullCompressorImpl::CompressData(
    const vector<char>& data, vector<char>* compressed_data,
    Callback callback) {
  *CHECK_NOTNULL(compressed_data) = data;
  callback();
}

BundlePayload::CompressionType NullCompressorImpl::compression_type() const {
  return BundlePayload::COMPRESSION_TYPE_NONE;
}

}  // namespace polar_express
