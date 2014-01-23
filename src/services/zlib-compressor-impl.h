#ifndef ZLIB_COMPRESSOR_IMPL_H
#define ZLIB_COMPRESSOR_IMPL_H

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "proto/bundle-manifest.pb.h"
#include "services/compressor.h"

struct z_stream_s;

namespace polar_express {

// Compressor that uses zlib to compress data. Note that this produces
// a raw DEFLATE stream, NOT gzip data.
class ZlibCompressorImpl : public Compressor {
 public:
  ZlibCompressorImpl();
  virtual ~ZlibCompressorImpl();

  virtual BundlePayload::CompressionType compression_type() const;

  virtual void InitializeCompression(size_t max_buffer_size);

  virtual void CompressData(
      const vector<byte>& data, vector<byte>* compressed_data,
      Callback callback);

  virtual void FinalizeCompression(vector<byte>* compressed_data);

 private:
  void DeflateStream(vector<byte>* compressed_data, bool flush);

  unique_ptr<z_stream_s> stream_;

  DISALLOW_COPY_AND_ASSIGN(ZlibCompressorImpl);
};

}  // namespace polar_express

#endif  // ZLIB_COMPRESSOR_IMPL_H
