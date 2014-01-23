#ifndef NULL_COMPRESSOR_IMPL_H
#define NULL_COMPRESSOR_IMPL_H

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "proto/bundle-manifest.pb.h"
#include "services/compressor.h"

namespace polar_express {

// Compressor implementation that doesn't actually do any compression
// (just copies input to output).
class NullCompressorImpl : public Compressor {
 public:
  NullCompressorImpl();
  virtual ~NullCompressorImpl();

  virtual BundlePayload::CompressionType compression_type() const;

  virtual void InitializeCompression(size_t max_buffer_size);

  virtual void CompressData(
      const vector<byte>& data, vector<byte>* compressed_data,
      Callback callback);

  virtual void FinalizeCompression(vector<byte>* compressed_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCompressorImpl);
};

}  // namespace polar_express

#endif  // NULL_COMPRESSOR_IMPL_H
