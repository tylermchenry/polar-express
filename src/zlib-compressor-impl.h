#ifndef ZLIB_COMPRESSOR_IMPL_H
#define ZLIB_COMPRESSOR_IMPL_H

#include <vector>

#include "callback.h"
#include "compressor.h"
#include "macros.h"
#include "proto/bundle-manifest.pb.h"

namespace polar_express {

// Compressor implementation that doesn't actually do any compression
// (just copies input to output).
class ZlibCompressorImpl : public Compressor {
 public:
  ZlibCompressorImpl();
  virtual ~ZlibCompressorImpl();

  virtual void CompressData(
      const vector<char>& data, vector<char>* compressed_data,
      Callback callback);

  virtual BundlePayload::CompressionType compression_type() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ZlibCompressorImpl);
};

}  // namespace polar_express

#endif  // ZLIB_COMPRESSOR_IMPL_H
