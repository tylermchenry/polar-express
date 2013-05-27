#ifndef BUNDLE_H
#define BUNDLE_H

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "macros.h"
#include "proto/bundle-manifest.pb.h"

namespace polar_express {

class Block;
class TarHeaderBlock;

// The Bundle class is a wrapper around an in-memory representation of
// a temp file that is used to store the contents of a logical bundle
// in the Polar Express system. A logical bundle is a collection of
// blocks, plus a manifest, that has been (or will be) uploaded to the
// server.
//
// The internal format of a Bundle is a standard TAR file, containing
// a file named "manifest", a file name "manifest.sha1_digest" and one
// or more files named "payload_<N>", where N is an integer. The manifest
// is a serialized BundleManifest protocol buffer describing the
// contents of the payloads. Each payload is a compressed blob of
// data. The compression type is indicated in the manifest. When the
// payload is decompressed, it contains a series of block data
// contents. The block IDs in each payload, beginning of that day
// corresponding to each block is indicated in the manifest.
//
// Notably, the bundle manifest does NOT contain the bundle ID or the
// SHA1 digest of the entire bundle. This is because a bundle is not
// assigned an ID until it is recorded to the metadata database after it
// is finalized, and because clearly it would be difficult for the bundle
// to include a digest in the data to be hashed. Instead, this information
// is stored in the filename given to the bundle by other code. This
// information is convenient, though not critical, because each block,
// and the manifest itself, has an individual digest, and because correct
// restoration of blocks is independent of bundle ID when performing a
// full restore.
//
// The manifest and its digest are the last files in the archive, for
// practicality reasons. This is not terribly efficient for reading
// bundles, but it is in practice not a problem since bundles normally
// have only a small number (often just one) of payloads.
//
// This class is NOT thread-safe!
//
// TODO(tylermchenry): It will probably be useful for this class to be
// mockable later on when there are unit tests. This will require
// replacing the constructor with a factory.
class Bundle {
 public:
  Bundle();
  ~Bundle();

  // Returns the current manifest.
  const BundleManifest& manifest() const;

  // Returns the current data. Not complete until Finalize is called.
  const vector<char>& data() const;

  // Returns the current size of the bundle in bytes. Note that the
  // size will increase when Finalize is called, on account of the
  // serialized manifest being appended to the bundle.
  size_t size() const;

  // Returns true if Finalize has been called.
  bool is_finalized() const;

  // Begins a new payload which subsequent calls to AppendBlock will
  // be added to. It is not necessary to call this before the first call to
  // AppendBlock, since there is always an implicit first
  // payload. All blocks in a payload must be part of the same
  // continuous compression stream.
  //
  // It is not legal to call this method after calling Finalize.
  void StartNewPayload(BundlePayload::CompressionType compression_type);

  // Adds the given block to the manifest in the current payload.
  //
  // It is not legal to call this method before calling
  // StartNewPayload, or after calling Finalize.
  void AddBlockMetadata(const Block& block);

  // Appends the given compressed data to the current payload. The
  // compressed data must be the compressed contents of the blocks,
  // appearing in the same order as the blocks are added with
  // AddBlockMetadata.
  //
  // It is not legal to call this method before calling
  // StartNewPayload, or after calling Finalize.
  void AppendBlockContents(const vector<char>& compressed_contents);

  // Serializes the manifest to the bundle, closes out the TAR file
  // and returns a pointer to the contents of the completed bundle, which
  // is now immutable. After Finalize is called, it is illegal to add
  // any new payload data.
  void Finalize();

 private:
  // Creates space for a new TAR file header in the data_ buffer, and
  // instantiates a new TarHeaderBlock object in current_tar_header_block_
  // to manage it.
  void StartNewFile(const string& name);

  // Closes the current file. Writes the final length and the checksum
  // to the header block and then resets current_tar_header_block_ to null.
  void EndCurrentFile(size_t final_size);

  // Closes the file associated with the current payload (if any) and
  // resets current_payload_ to null.
  void EndCurrentPayload();

  // Writes the serialized manifest and its checksum as two separate
  // files to data_. All payloads must be closed before calling this.
  void AppendSerializedManifest();

  int64_t id_;
  BundleManifest manifest_;
  bool is_finalized_;

  unique_ptr<TarHeaderBlock> current_tar_header_block_;
  BundlePayload* current_payload_;  // not owned
  int64_t next_payload_id_;

  boost::shared_ptr<vector<char> > data_;

  DISALLOW_COPY_AND_ASSIGN(Bundle);
};

// A bundle which has been finalized and is no longer mutable. This
// also contain additional metadata about what ID has been assigned to
// the bundle, and what file on disk contains its persisted contents.
class FinalizedBundle {
 public:
  // The bundle given as the argument must have been finalized.
  explicit FinalizedBundle(boost::shared_ptr<const Bundle> bundle);

  const Bundle& bundle() const;

  int64_t id() const;
  void set_id(int64_t id);

  const string& persistence_file_path() const;
  void set_persistence_file_path(const string& path);

 private:
  const boost::shared_ptr<const Bundle> bundle_;
  int64_t id_;
  string persistence_file_path_;

  DISALLOW_COPY_AND_ASSIGN(FinalizedBundle);
};

}  // namespace polar_express

#endif  // BUNDLE_H
