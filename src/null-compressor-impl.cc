#include "null-compressor-impl.h"

#include <algorithm>
#include <iterator>

namespace polar_express {

NullCompressorImpl::NullCompressorImpl() {
}

NullCompressorImpl::~NullCompressorImpl() {
}

BundlePayload::CompressionType NullCompressorImpl::compression_type() const {
  return BundlePayload::COMPRESSION_TYPE_NONE;
}

void NullCompressorImpl::InitializeCompression(size_t max_buffer_size) {
  // No-op.
}

void NullCompressorImpl::CompressData(
    const vector<char>& data, vector<char>* compressed_data,
    Callback callback) {
  std::copy(data.begin(), data.end(),
            std::back_inserter(*CHECK_NOTNULL(compressed_data)));
  callback();
}

void NullCompressorImpl::FinalizeCompression(vector<char>* compressed_data) {
  // No-op.
}

}  // namespace polar_express
