#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "proto/bundle-manifest.pb.h"

namespace polar_express {

// Base class for asynchronous data compressors (which use various
// algorithms).
class Compressor {
 public:
  static unique_ptr<Compressor> CreateCompressor(
      BundlePayload::CompressionType compression_type);

  virtual ~Compressor();

  virtual BundlePayload::CompressionType compression_type() const;

  // This must be called once before the first call to CompressData
  // and again after FinalizeCompression before CompressData may be
  // called again. Calling InitializeCompression discards any
  // buffered compressed data that has not yet been finalized.
  //
  // This call is lightweight and synchronous.
  virtual void InitializeCompression(size_t max_buffer_size);

  // Compresses (more) data. Some or all of the compressed data may be
  // appeneded to compressed_data. It is possible that nothing will be
  // appended to compressed_data if it all remains buffered.
  virtual void CompressData(
      const vector<byte>& data, vector<byte>* compressed_data,
      Callback callback);

  // Outputs any remaining buffered compressed data to compressed_data
  // and clears all state. InitializeCompression must have been called
  // at least once before each call to FinalizeCompression.
  //
  // This call is lightweight and synchronous.
  virtual void FinalizeCompression(vector<byte>* compressed_data);

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
