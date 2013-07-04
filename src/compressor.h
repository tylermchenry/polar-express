#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <vector>

#include "callback.h"
#include "macros.h"
#include "proto/bundle-manifest.pb.h"

namespace polar_express {

// Base class for asynchronous data compressors (which use various
// algorithms).
class Compressor {
 public:
  static unique_ptr<Compressor> CreateCompressor(
      BundlePayload::CompressionType compression_type);

  virtual ~Compressor();

  virtual void CompressData(
      const vector<char>& data, vector<char>* compressed_data,
      Callback callback);

  virtual BundlePayload::CompressionType compression_type() const;

  // TODO(tylermchenry): Add decompression.

 protected:
  Compressor();
  explicit Compressor(unique_ptr<Compressor>&& impl);

 private:
  template<typename CompressorImplT>
  static unique_ptr<Compressor> CreateCompressorWithImpl();

  unique_ptr<Compressor> impl_;

  DISALLOW_COPY_AND_ASSIGN(Compressor);
};

}  // namespace polar_express

#endif  // COMPRESSOR_H
