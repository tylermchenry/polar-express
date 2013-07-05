#ifndef ZLIB_COMPRESSOR_IMPL_H
#define ZLIB_COMPRESSOR_IMPL_H

#include <memory>
#include <vector>

#include "callback.h"
#include "compressor.h"
#include "macros.h"
#include "proto/bundle-manifest.pb.h"

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
      const vector<char>& data, vector<char>* compressed_data,
      Callback callback);

  virtual void FinalizeCompression(vector<char>* compressed_data);

 private:
  void DeflateStream(vector<char>* compressed_data, bool flush);

  unique_ptr<z_stream_s> stream_;

  DISALLOW_COPY_AND_ASSIGN(ZlibCompressorImpl);
};

}  // namespace polar_express

#endif  // ZLIB_COMPRESSOR_IMPL_H
