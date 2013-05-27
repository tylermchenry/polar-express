#ifndef CHUNK_HASHER_IMPL_H
#define CHUNK_HASHER_IMPL_H

#include <memory>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "chunk-hasher.h"
#include "macros.h"

namespace CryptoPP {
class SHA1;
}  // namespace CryptoPP

namespace polar_express {

class Block;
class Chunk;
class ChunkReader;

class ChunkHasherImpl : public ChunkHasher {
 public:
  ChunkHasherImpl();
  virtual ~ChunkHasherImpl();

  virtual void GenerateAndHashChunks(
      const boost::filesystem::path& path,
      boost::shared_ptr<Snapshot> snapshot, Callback callback);

  virtual void ValidateHash(
      const Chunk& chunk, const vector<char>& block_data_for_chunk,
      bool* is_valid, Callback callback);

 private:
  struct Context {
    Context(const boost::filesystem::path& path,
            boost::shared_ptr<Snapshot> snapshot,
            Callback callback);

    boost::filesystem::path path_;
    boost::shared_ptr<Snapshot> snapshot_;
    boost::shared_ptr<ChunkReader> chunk_reader_;
    Chunk* current_chunk_;
    vector<char> block_data_buffer_;
    Callback callback_;
  };

  void ContinueGeneratingAndHashingChunks(
      boost::shared_ptr<Context> context);

  void UpdateHashesFromBlockData(
      boost::shared_ptr<Context> context);

  void HashData(const vector<char>& data, string* sha1_digest) const;

  void UpdateWholeFileHash(const vector<char>& data);

  void WriteWholeFileHash(string* sha1_digest) const;

  unique_ptr<CryptoPP::SHA1> whole_file_sha1_engine_;

  DISALLOW_COPY_AND_ASSIGN(ChunkHasherImpl);
};

}   // namespace chunk_hasher

#endif  // CHUNK_HASHER_IMPL_H
