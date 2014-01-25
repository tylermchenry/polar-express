#include "services/zlib-compressor-impl.h"

#include <zlib.h>

#include "base/options.h"

DEFINE_OPTION(zlib_compression_level, int, Z_BEST_COMPRESSION,
              "Compression level when using zlib for compression.");

namespace polar_express {

ZlibCompressorImpl::ZlibCompressorImpl() {
}

ZlibCompressorImpl::~ZlibCompressorImpl() {
}

BundlePayload::CompressionType ZlibCompressorImpl::compression_type() const {
  return BundlePayload::COMPRESSION_TYPE_ZLIB;
}

void ZlibCompressorImpl::InitializeCompression(size_t max_buffer_size) {
  // TODO(tylermchenry): Intelligently adjust window bits and memory
  // level to respect max_buffer_size.

  stream_.reset(new z_stream);
  stream_->zalloc = nullptr;
  stream_->zfree = nullptr;
  stream_->opaque = nullptr;

  if (deflateInit(stream_.get(), options::zlib_compression_level) != Z_OK) {
    // TODO(tylermchenry): Reasonable error handling.
    assert(false);
  }
}

void ZlibCompressorImpl::CompressData(
    const vector<byte>& data, vector<byte>* compressed_data,
    Callback callback) {
  assert(stream_ != nullptr);

  stream_->next_in = reinterpret_cast<const Bytef*>(data.data());
  stream_->avail_in = data.size();
  DeflateStream(compressed_data, false /* do not flush */);

  callback();
}

void ZlibCompressorImpl::FinalizeCompression(vector<byte>* compressed_data) {
  assert(stream_ != nullptr);

  stream_->next_in = nullptr;
  stream_->avail_in = 0;
  DeflateStream(compressed_data, true /* flush everything */);

  stream_.reset();
}

void ZlibCompressorImpl::DeflateStream(
    vector<byte>* compressed_data, bool flush) {
  assert(stream_ != nullptr);
  assert(compressed_data != nullptr);

  do {
    size_t next_out_offset = compressed_data->size();
    size_t deflate_bound = deflateBound(stream_.get(), stream_->avail_in);
    compressed_data->resize(next_out_offset + deflate_bound);
    stream_->next_out =
        reinterpret_cast<Bytef*>(compressed_data->data() + next_out_offset);
    stream_->avail_out = deflate_bound;
    deflate(stream_.get(), flush ? Z_FULL_FLUSH : Z_NO_FLUSH);
    compressed_data->resize(compressed_data->size() - stream_->avail_out);
  } while (stream_->avail_in > 0);
}

}  // namespace polar_express
