#ifndef METADATA_DB_IMPL_H
#define METADATA_DB_IMPL_H

#include <string>

#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "macros.h"
#include "metadata-db.h"

class sqlite3;

namespace polar_express {

class AnnotatedBundleData;
class Attributes;
class Chunk;
class File;
class Snapshot;

class MetadataDbImpl : public MetadataDb {
 public:
  MetadataDbImpl();
  virtual ~MetadataDbImpl();

  virtual void GetLatestSnapshot(
      const File& file, boost::shared_ptr<Snapshot>* snapshot,
      Callback callback);

  virtual void RecordNewSnapshot(
      boost::shared_ptr<Snapshot> snapshot, Callback callback);

  virtual void RecordNewBundle(
      boost::shared_ptr<AnnotatedBundleData> bundle, Callback callback);

 private:
  // TODO(tylermchenry): Might be useful for this to be public later.
  int64_t GetLatestSnapshotId(const File& file) const;

  void FindExistingIds(
      boost::shared_ptr<Snapshot> snapshot,
      int64_t* previous_snapshot_id) const;
  void FindExistingFileId(File* file) const;
  void FindExistingAttributesId(Attributes* attributes) const;
  void FindExistingBlockIds(boost::shared_ptr<Snapshot> snapshot) const;
  void FindExistingChunkIds(
      int64_t previous_snapshot_id,
      boost::shared_ptr<Snapshot> snapshot) const;

  void WriteNewSnapshot(boost::shared_ptr<Snapshot> snapshot) const;
  void WriteNewFile(File* file) const;
  void WriteNewAttributes(Attributes* attributes) const;
  void WriteNewBlocks(boost::shared_ptr<Snapshot> snapshot) const;
  void WriteNewChunks(boost::shared_ptr<Snapshot> snapshot) const;

  void UpdateLatestChunksCache(
      int64_t previous_snapshot_id,
      boost::shared_ptr<Snapshot> snapshot) const;

  void WriteNewBundle(boost::shared_ptr<AnnotatedBundleData> bundle) const;
  void WriteNewBlockToBundleMappings(
      boost::shared_ptr<AnnotatedBundleData> bundle) const;

  static sqlite3* db_;

  static sqlite3* db();
  static void InitDb();

  DISALLOW_COPY_AND_ASSIGN(MetadataDbImpl);
};

}  // namespace polar_express

#endif  // METADATA_DB_IMPL_H
