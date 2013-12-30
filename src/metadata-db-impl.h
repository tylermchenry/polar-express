#ifndef METADATA_DB_IMPL_H
#define METADATA_DB_IMPL_H

#include <memory>
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
class ScopedStatement;
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

  virtual void GetLatestBundleForBlock(
      const Block& block,
      boost::shared_ptr<BundleAnnotations>* bundle_annotations,
      Callback callback);

  virtual void RecordNewBundle(
      boost::shared_ptr<AnnotatedBundleData> bundle, Callback callback);

  virtual void RecordUploadedBundle(
      int server_id, boost::shared_ptr<AnnotatedBundleData> bundle,
      Callback callback);

 private:
  void PrepareStatements();

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

  // Prepared statements
  std::unique_ptr<ScopedStatement> snapshots_select_latest_stmt_;
  std::unique_ptr<ScopedStatement> snapshots_select_latest_id_stmt_;
  std::unique_ptr<ScopedStatement> snapshots_insert_stmt_;
  std::unique_ptr<ScopedStatement> files_select_id_stmt_;
  std::unique_ptr<ScopedStatement> files_insert_stmt_;
  std::unique_ptr<ScopedStatement> attributes_select_id_stmt_;
  std::unique_ptr<ScopedStatement> attributes_insert_stmt_;
  std::unique_ptr<ScopedStatement> blocks_select_id_stmt_;
  std::unique_ptr<ScopedStatement> blocks_insert_stmt_;
  std::unique_ptr<ScopedStatement> bundles_select_latest_by_block_id_stmt_;
  std::unique_ptr<ScopedStatement> bundles_insert_stmt_;
  std::unique_ptr<ScopedStatement> files_to_blocks_mapping_select_stmt_;
  std::unique_ptr<ScopedStatement> files_to_blocks_mapping_insert_stmt_;
  std::unique_ptr<ScopedStatement>
      snapshots_to_files_to_blocks_mapping_insert_stmt_;
  std::unique_ptr<ScopedStatement>
      snapshots_to_files_to_blocks_mapping_delete_stmt_;
  std::unique_ptr<ScopedStatement> blocks_to_bundles_mapping_insert_stmt_;
  std::unique_ptr<ScopedStatement> bundles_to_servers_mapping_insert_stmt_;

  static sqlite3* db_;

  static sqlite3* db();
  static void InitDb();

  DISALLOW_COPY_AND_ASSIGN(MetadataDbImpl);
};

}  // namespace polar_express

#endif  // METADATA_DB_IMPL_H
