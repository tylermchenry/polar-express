#ifndef NULL_COMPRESSOR_IMPL_H
#define NULL_COMPRESSOR_IMPL_H

#include <vector>

#include "callback.h"
#include "compressor.h"
#include "macros.h"
#include "proto/bundle-manifest.pb.h"

namespace polar_express {

// Compressor implementation that doesn't actually do any compression
// (just copies input to output).
class NullCompressorImpl : public Compressor {
 public:
  NullCompressorImpl();
  virtual ~NullCompressorImpl();

  virtual void CompressData(
      const vector<char>& data, vector<char>* compressed_data,
      Callback callback);

  virtual BundlePayload::CompressionType compression_type() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCompressorImpl);
};

}  // namespace polar_express

#endif  // NULL_COMPRESSOR_IMPL_H
