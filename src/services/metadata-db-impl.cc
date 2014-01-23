#include "services/metadata-db-impl.h"

#include <iostream>

#include <boost/thread/once.hpp>
#include <sqlite3.h>

#include "file/bundle.h"
#include "proto/bundle-manifest.pb.h"
#include "proto/file.pb.h"
#include "proto/snapshot.pb.h"
#include "services/sqlite3-helpers.h"

#define HAS_FIELD(field_name) has_ ## field_name

#define BIND_TYPE(type) Bind ## type

#define COL_NAME(field_name) ":" #field_name

#define BIND_IF_PRESENT(stmt, type, proto_ptr, field_name)           \
  do {                                                               \
    if ((proto_ptr)->HAS_FIELD(field_name)()) {                      \
      (stmt).BIND_TYPE(type)(COL_NAME(field_name),                   \
                             (proto_ptr)->field_name());             \
    }                                                                \
  } while(0)

#define GET_TYPE(type) GetColumn ## type

#define QUALIFIED_COL_NAME(tbl_name, field_name)                     \
  #tbl_name "_" #field_name

#define SET_FIELD(field_name) set_ ## field_name

#define SET_IF_PRESENT(stmt, type, proto_ptr, tbl_name, field_name)  \
  do {                                                               \
    if (!(stmt).IsColumnNull(                                        \
            QUALIFIED_COL_NAME(tbl_name, field_name))) {             \
      (proto_ptr)->SET_FIELD(field_name)(                            \
          (stmt).GET_TYPE(type)(                                     \
              QUALIFIED_COL_NAME(tbl_name, field_name)));            \
    }                                                                \
  } while(0)

namespace polar_express {

sqlite3* MetadataDbImpl::db_ = nullptr;

MetadataDbImpl::MetadataDbImpl()
    : MetadataDb(false),
      snapshots_select_latest_stmt_(new ScopedStatement(db())),
      snapshots_select_latest_id_stmt_(new ScopedStatement(db())),
      snapshots_insert_stmt_(new ScopedStatement(db())),
      files_select_id_stmt_(new ScopedStatement(db())),
      files_insert_stmt_(new ScopedStatement(db())),
      attributes_select_id_stmt_(new ScopedStatement(db())),
      attributes_insert_stmt_(new ScopedStatement(db())),
      blocks_select_id_stmt_(new ScopedStatement(db())),
      blocks_insert_stmt_(new ScopedStatement(db())),
      bundles_select_latest_by_block_id_stmt_(new ScopedStatement(db())),
      bundles_insert_stmt_(new ScopedStatement(db())),
      files_to_blocks_mapping_select_stmt_(new ScopedStatement(db())),
      files_to_blocks_mapping_insert_stmt_(new ScopedStatement(db())),
      snapshots_to_files_to_blocks_mapping_insert_stmt_(
          new ScopedStatement(db())),
      snapshots_to_files_to_blocks_mapping_delete_stmt_(
          new ScopedStatement(db())),
      blocks_to_bundles_mapping_insert_stmt_(new ScopedStatement(db())),
      bundles_to_servers_mapping_insert_stmt_(new ScopedStatement(db())) {
  PrepareStatements();
}

MetadataDbImpl::~MetadataDbImpl() {
}

void MetadataDbImpl::GetLatestSnapshot(
    const File& file, boost::shared_ptr<Snapshot>* snapshot,
    Callback callback) {
  CHECK_NOTNULL(snapshot)->reset(new Snapshot);

  (*snapshot)->mutable_file()->CopyFrom(file);

  if (!(*snapshot)->file().has_id()) {
    FindExistingFileId((*snapshot)->mutable_file());
  }
  if (!(*snapshot)->file().has_id()) {
    callback();
    return;
  }
  snapshots_select_latest_stmt_->Reset();

  snapshots_select_latest_stmt_->BindInt64(
      ":file_id", (*snapshot)->file().id());

  if (snapshots_select_latest_stmt_->StepUntilNotBusy() == SQLITE_ROW) {
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Int64, *snapshot, snapshots,
                   id);

    Attributes* attributes = (*snapshot)->mutable_attributes();
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Int64, attributes,
                   attributes, id);
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Text, attributes,
                   attributes, owner_user);
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Text, attributes,
                   attributes, owner_group);
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Int64, attributes,
                   attributes, uid);
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Int64, attributes,
                   attributes, gid);
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Int64, attributes,
                   attributes, mode);

    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Int64, *snapshot,
                   snapshots, creation_time);
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Int64, *snapshot,
                   snapshots, modification_time);
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Int64, *snapshot,
                   snapshots, access_time);
    // TODO: Extra attributes
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Bool, *snapshot,
                   snapshots, is_regular);
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Bool, *snapshot,
                   snapshots, is_deleted);
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Text, *snapshot,
                   snapshots, sha1_digest);
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Int64, *snapshot, snapshots,
                   length);
    SET_IF_PRESENT(*snapshots_select_latest_stmt_, Int64, *snapshot,
                   snapshots, observation_time);
  }

  callback();
}

void MetadataDbImpl::RecordNewSnapshot(
    boost::shared_ptr<Snapshot> snapshot, Callback callback) {
  assert(!snapshot->has_id());
  int64_t previous_snapshot_id = -1;

  FindExistingIds(snapshot, &previous_snapshot_id);

  sqlite3_exec(db(), "begin transaction;",
               nullptr, nullptr, nullptr);

  if (!snapshot->file().has_id()) {
    WriteNewFile(snapshot->mutable_file());
  }

  if (!snapshot->attributes().has_id()) {
    WriteNewAttributes(snapshot->mutable_attributes());
  }

  WriteNewBlocks(snapshot);

  WriteNewChunks(snapshot);

  WriteNewSnapshot(snapshot);

  UpdateLatestChunksCache(previous_snapshot_id, snapshot);

  sqlite3_exec(db(), "commit;", nullptr, nullptr, nullptr);

  callback();
}

void MetadataDbImpl::GetLatestBundleForBlock(
    const Block& block,
    boost::shared_ptr<BundleAnnotations>* bundle_annotations,
    Callback callback) {
  CHECK_NOTNULL(bundle_annotations)->reset();

  bundles_select_latest_by_block_id_stmt_->Reset();
  bundles_select_latest_by_block_id_stmt_->BindInt64(":block_id", block.id());

  if (bundles_select_latest_by_block_id_stmt_->StepUntilNotBusy() ==
      SQLITE_ROW) {
    bundle_annotations->reset(new BundleAnnotations);

    SET_IF_PRESENT(*bundles_select_latest_by_block_id_stmt_, Int64,
                   *bundle_annotations, local_bundles, id);
    SET_IF_PRESENT(*bundles_select_latest_by_block_id_stmt_, Text,
                   *bundle_annotations, local_bundles, sha256_linear_digest);
    SET_IF_PRESENT(*bundles_select_latest_by_block_id_stmt_, Text,
                   *bundle_annotations, local_bundles, sha256_tree_digest);
    SET_IF_PRESENT(*bundles_select_latest_by_block_id_stmt_, Text,
                   *bundle_annotations, local_bundles_to_servers,
                   server_bundle_id);
    SET_IF_PRESENT(*bundles_select_latest_by_block_id_stmt_,
                   Enum<BundleAnnotations::ServerBundleStatus>,
                   *bundle_annotations, local_bundles_to_servers,
                   server_bundle_status);
    SET_IF_PRESENT(*bundles_select_latest_by_block_id_stmt_, Int64,
                   *bundle_annotations, local_bundles_to_servers,
                   server_bundle_status_timestamp);
  }

  callback();
}

void MetadataDbImpl::RecordNewBundle(
    boost::shared_ptr<AnnotatedBundleData> bundle, Callback callback) {
  assert(!bundle->annotations().has_id());

  sqlite3_exec(db(), "begin transaction;",
               nullptr, nullptr, nullptr);

  WriteNewBundle(bundle);

  WriteNewBlockToBundleMappings(bundle);

  // TODO(tylermchenry): Write new mappings for attributes-to-bundle
  // and snapshots-to-bundle when backups of these metadata objects
  // are stored in bundles.

  sqlite3_exec(db(), "commit;", nullptr, nullptr, nullptr);

  callback();
}

void MetadataDbImpl::RecordUploadedBundle(
    int server_id, boost::shared_ptr<AnnotatedBundleData> bundle,
    Callback callback) {
  bundles_to_servers_mapping_insert_stmt_->Reset();

  bundles_to_servers_mapping_insert_stmt_->BindInt64(
      ":bundle_id", bundle->annotations().id());
  bundles_to_servers_mapping_insert_stmt_->BindInt64(
      ":server_id", server_id);
  bundles_to_servers_mapping_insert_stmt_->BindText(
      ":server_bundle_id", bundle->annotations().server_bundle_id());
  bundles_to_servers_mapping_insert_stmt_->BindEnum(
      ":status", bundle->annotations().server_bundle_status());
  bundles_to_servers_mapping_insert_stmt_->BindInt64(
      ":status_timestamp",
      bundle->annotations().server_bundle_status_timestamp());

  int code = bundles_to_servers_mapping_insert_stmt_->StepUntilNotBusy();
  if (code != SQLITE_DONE) {
    std::cerr << sqlite3_errmsg(db()) << std::endl;
    std::cerr << bundle->annotations().DebugString() << std::endl;
  }

  callback();
}

void MetadataDbImpl::PrepareStatements() {
  snapshots_select_latest_stmt_->Prepare(
      "select snapshots.id as snapshots_id, "
      "       snapshots.creation_time as snapshots_creation_time, "
      "       snapshots.modification_time as snapshots_modification_time, "
      "       snapshots.access_time as snapshots_access_time, "
      "       snapshots.is_regular as snapshots_is_regular, "
      "       snapshots.is_deleted as snapshots_is_deleted, "
      "       snapshots.sha1_digest as snapshots_sha1_digest, "
      "       snapshots.length as snapshots_length, "
      "       snapshots.observation_time as snapshots_observation_time, "
      "       attributes.id as attributes_id, "
      "       attributes.owner_user as attributes_owner_user, "
      "       attributes.owner_group as attributes_owner_group,"
      "       attributes.uid as attributes_uid, "
      "       attributes.gid as attributes_gid, "
      "       attributes.mode as attributes_mode "
      "from snapshots join attributes on "
      "  snapshots.attributes_id = attributes.id "
      "where snapshots.file_id = :file_id "
    "order by snapshots.observation_time desc limit 1;");

  snapshots_select_latest_id_stmt_->Prepare(
      "select id from snapshots where file_id = :file_id "
      "order by observation_time desc limit 1;");

  snapshots_insert_stmt_->Prepare(
      "insert into snapshots ('file_id', 'attributes_id', 'creation_time', "
      "'modification_time', 'access_time', 'is_regular', 'is_deleted', "
      "'sha1_digest', 'length', 'observation_time') "
      "values (:file_id, :attributes_id, :creation_time, "
      ":modification_time, :access_time, :is_regular, :is_deleted, "
      ":sha1_digest, :length, :observation_time);");

  files_select_id_stmt_->Prepare(
      "select files.id as files_id from files where path = :path;");

  files_insert_stmt_->Prepare(
      "insert into files ('path') "
      "values (:path);");

  attributes_select_id_stmt_->Prepare(
      "select id from attributes where owner_user = :owner_user and "
      "owner_group = :owner_group and uid = :uid and gid = :gid and "
      "mode = :mode;");

  attributes_insert_stmt_->Prepare(
      "insert into attributes ('owner_user', 'owner_group', 'uid', "
      "'gid', 'mode') "
      "values (:owner_user, :owner_group, :uid, :gid, :mode);");

  blocks_select_id_stmt_->Prepare(
      "select id from blocks where sha1_digest = :sha1_digest and "
      "length = :length;");

  blocks_insert_stmt_->Prepare(
      "insert into blocks ('sha1_digest', 'length') "
      "values (:sha1_digest, :length);");

  bundles_select_latest_by_block_id_stmt_->Prepare(
      "select local_bundles.id as local_bundles_id, "
      "       local_bundles.sha256_linear_digest "
      "         as local_bundles_sha256_linear_digest, "
      "       local_bundles.sha256_tree_digest "
      "         as local_bundles_sha256_tree_digest, "
      "       local_bundles_to_servers.server_bundle_id "
      "         as local_bundles_to_servers_server_bundle_id, "
      "       local_bundles_to_servers.status "
      "         as local_bundles_to_servers_server_bundle_status, "
      "       local_bundles_to_servers.status_timestamp "
      "         as local_bundles_to_servers_server_bundle_status_timestamp "
      "from "
      "  (local_blocks_to_bundles join local_bundles "
      "   on local_blocks_to_bundles.bundle_id = local_bundles.id) "
      "join "
      "   local_bundles_to_servers "
      "   on local_bundles.id = local_bundles_to_servers.bundle_id "
      "where block_id = :block_id "
      "order by local_bundles_to_servers.status_timestamp desc, "
      "         local_bundles.id desc "
      "limit 1;");

  bundles_insert_stmt_->Prepare(
      "insert into local_bundles "
      "('sha256_linear_digest', 'sha256_tree_digest', 'length') "
      "values (:sha256_linear_digest, :sha256_tree_digest, :length);");

  files_to_blocks_mapping_select_stmt_->Prepare(
      "select files_to_blocks.id as files_to_blocks_id, "
      "       files_to_blocks.block_id as files_to_blocks_block_id, "
      "       files_to_blocks.offset as files_to_blocks_offset, "
      "       files_to_blocks.observation_time as "
      "         files_to_blocks_observation_time "
      "from files_to_blocks "
      "  join local_snapshots_to_files_to_blocks "
      "  on files_to_blocks.id = "
      "    local_snapshots_to_files_to_blocks.files_to_blocks_id "
      "where local_snapshots_to_files_to_blocks.snapshot_id = "
      "  :snapshot_id;");

  files_to_blocks_mapping_insert_stmt_->Prepare(
      "insert into files_to_blocks "
      "('file_id', 'block_id', 'offset', 'observation_time') "
      "values (:file_id, :block_id, :offset, :observation_time);");

  snapshots_to_files_to_blocks_mapping_insert_stmt_->Prepare(
      "insert into local_snapshots_to_files_to_blocks "
      "  (snapshot_id, files_to_blocks_id) values "
      "  (:snapshot_id, :files_to_blocks_id);");

  snapshots_to_files_to_blocks_mapping_delete_stmt_->Prepare(
        "delete from local_snapshots_to_files_to_blocks "
        "where snapshot_id = :snapshot_id;");

  blocks_to_bundles_mapping_insert_stmt_->Prepare(
      "insert into local_blocks_to_bundles ('block_id', 'bundle_id') "
      "values (:block_id, :bundle_id);");

  bundles_to_servers_mapping_insert_stmt_->Prepare(
      "insert into local_bundles_to_servers "
      "('bundle_id', 'server_id', 'server_bundle_id', 'status', "
      "'status_timestamp') "
      "values (:bundle_id, :server_id, :server_bundle_id, :status, "
      ":status_timestamp);");

  // This sets the SQLite Database to use Write-Ahead Logging, but to only
  // periodically force-flush the journal to disk. This ensures that the
  // metadata DB will never become corrupted, but in the case of a crash or
  // power outage it may still lose some of the most recent writes when
  // recovered. This is fine for the case of this application, since the worst
  // case is that we redundantly back up the blocks that we forgot that we
  // backed up on account of the lost writes. The gain is a 15x or better
  // speedup over full synchronous mode.
  //
  // TODO: Should be configurable. Users who are storing their metadata DB on a
  // solid-state drive can enable full-synchronous mode without a significant
  // performance penalty.
  //
  // TODO: When in non-full-synchronous mode, the system should force a
  // synchronization when it is otherwise idle (waiting on upstream).
  sqlite3_exec(db(), "pragma synchronous = NORMAL",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db(), "pragma journal_mode = WAL",
               nullptr, nullptr, nullptr);
}

int64_t MetadataDbImpl::GetLatestSnapshotId(const File& file) const {
  ScopedStatement snapshots_select_stmt(db());
  snapshots_select_latest_id_stmt_->Reset();
  snapshots_select_latest_id_stmt_->BindInt64(":file_id", file.id());

  if (snapshots_select_latest_id_stmt_->StepUntilNotBusy() == SQLITE_ROW) {
    return snapshots_select_latest_id_stmt_->GetColumnInt64("id");
  }

  return -1;
}

void MetadataDbImpl::FindExistingIds(
    boost::shared_ptr<Snapshot> snapshot,
    int64_t* previous_snapshot_id) const {
  assert(previous_snapshot_id != nullptr);

  if (!snapshot->file().has_id()) {
    FindExistingFileId(snapshot->mutable_file());
  }
  if (snapshot->file().has_id()) {
    *previous_snapshot_id = GetLatestSnapshotId(snapshot->file());
  } else {
    *previous_snapshot_id = -1;
  }
  if (!snapshot->attributes().has_id()) {
    FindExistingAttributesId(snapshot->mutable_attributes());
  }
  FindExistingBlockIds(snapshot);
  if (previous_snapshot_id > 0) {
    FindExistingChunkIds(*previous_snapshot_id, snapshot);
  }
}

void MetadataDbImpl::FindExistingFileId(File* file) const {
  assert(file != nullptr);
  assert(!file->has_id());

  files_select_id_stmt_->Reset();
  files_select_id_stmt_->BindText(":path", file->path());

  if (files_select_id_stmt_->StepUntilNotBusy() == SQLITE_ROW) {
    SET_IF_PRESENT(*files_select_id_stmt_, Int64, file, files, id);
  }
}

void MetadataDbImpl::FindExistingAttributesId(Attributes* attributes) const {
  assert(attributes != nullptr);
  assert(!attributes->has_id());

  attributes_select_id_stmt_->Reset();

  BIND_IF_PRESENT(*attributes_select_id_stmt_, Text, attributes, owner_user);
  BIND_IF_PRESENT(*attributes_select_id_stmt_, Text, attributes, owner_group);
  BIND_IF_PRESENT(*attributes_select_id_stmt_, Int, attributes, uid);
  BIND_IF_PRESENT(*attributes_select_id_stmt_, Int, attributes, gid);
  BIND_IF_PRESENT(*attributes_select_id_stmt_, Int, attributes, mode);

  if (attributes_select_id_stmt_->StepUntilNotBusy() == SQLITE_ROW) {
    attributes->set_id(attributes_select_id_stmt_->GetColumnInt64("id"));
  }
}

void MetadataDbImpl::FindExistingBlockIds(
    boost::shared_ptr<Snapshot> snapshot) const {
  blocks_select_id_stmt_->Reset();

  for (Chunk& chunk : *(snapshot->mutable_chunks())) {
    Block* block = chunk.mutable_block();
    if (block->has_id()) {
      continue;
    }

    blocks_select_id_stmt_->Reset();
    blocks_select_id_stmt_->BindText(":sha1_digest", block->sha1_digest());
    blocks_select_id_stmt_->BindInt64(":length", block->length());

    if (blocks_select_id_stmt_->StepUntilNotBusy() == SQLITE_ROW) {
      block->set_id(blocks_select_id_stmt_->GetColumnInt64("id"));
    }
  }
}

void MetadataDbImpl::FindExistingChunkIds(
    int64_t previous_snapshot_id,
    boost::shared_ptr<Snapshot> snapshot) const {
  files_to_blocks_mapping_select_stmt_->Reset();
  files_to_blocks_mapping_select_stmt_->BindInt64(":snapshot_id",
                                                 previous_snapshot_id);

  map<int64_t, boost::shared_ptr<Chunk> > offsets_to_latest_chunks;
  while (files_to_blocks_mapping_select_stmt_->StepUntilNotBusy() ==
         SQLITE_ROW) {
    boost::shared_ptr<Chunk> chunk(new Chunk);
    chunk->set_id(files_to_blocks_mapping_select_stmt_->GetColumnInt64(
        "files_to_blocks_id"));
    chunk->set_offset(files_to_blocks_mapping_select_stmt_->GetColumnInt64(
        "files_to_blocks_offset"));
    chunk->mutable_block()
        ->set_id(files_to_blocks_mapping_select_stmt_
                     ->GetColumnInt64("files_to_blocks_block_id"));
    chunk->set_observation_time(
        files_to_blocks_mapping_select_stmt_->GetColumnInt64(
            "files_to_blocks_observation_time"));
    offsets_to_latest_chunks.insert(make_pair(chunk->offset(), chunk));
  }

  for (Chunk& chunk : *(snapshot->mutable_chunks())) {
    if (chunk.has_id()) {
      continue;
    }

    auto latest_chunk_itr = offsets_to_latest_chunks.find(chunk.offset());
    if (latest_chunk_itr == offsets_to_latest_chunks.end()) {
      continue;
    }

    const Chunk& latest_chunk = *latest_chunk_itr->second;
    if (chunk.block().id() == latest_chunk.block().id()) {
      chunk.set_id(latest_chunk.id());
      chunk.set_observation_time(latest_chunk.observation_time());
    }
  }
}

void MetadataDbImpl::WriteNewSnapshot(
    boost::shared_ptr<Snapshot> snapshot) const {
  assert(!snapshot->has_id());

  snapshots_insert_stmt_->Reset();

  // TODO: Extra attributes.
  snapshots_insert_stmt_->BindInt64(":file_id", snapshot->file().id());
  snapshots_insert_stmt_->BindInt64(":attributes_id",
                                  snapshot->attributes().id());
  BIND_IF_PRESENT(*snapshots_insert_stmt_, Int64, snapshot, creation_time);
  snapshots_insert_stmt_->BindInt64(":modification_time",
                                   snapshot->modification_time());
  BIND_IF_PRESENT(*snapshots_insert_stmt_, Int64, snapshot, access_time);
  snapshots_insert_stmt_->BindBool(":is_regular", snapshot->is_regular());
  snapshots_insert_stmt_->BindBool(":is_deleted", snapshot->is_deleted());
  snapshots_insert_stmt_->BindText(":sha1_digest", snapshot->sha1_digest());
  snapshots_insert_stmt_->BindInt64(":length", snapshot->length());
  snapshots_insert_stmt_->BindInt64(":observation_time",
                                   snapshot->observation_time());

  int code = snapshots_insert_stmt_->StepUntilNotBusy();

  if (code == SQLITE_DONE) {
    snapshot->set_id(sqlite3_last_insert_rowid(db()));
  } else {
    std::cerr << sqlite3_errmsg(db()) << std::endl;
    std::cerr << snapshot->DebugString() << std::endl;
  }
}

void MetadataDbImpl::WriteNewFile(File* file) const {
  assert(file != nullptr);
  assert(!file->has_id());

  files_insert_stmt_->Reset();
  files_insert_stmt_->BindText(":path", file->path());

  int code = files_insert_stmt_->StepUntilNotBusy();

  if (code == SQLITE_DONE) {
    file->set_id(sqlite3_last_insert_rowid(db()));
  } else {
    std::cerr << sqlite3_errmsg(db()) << std::endl;
    std::cerr << file->DebugString() << std::endl;
  }
}

void MetadataDbImpl::WriteNewAttributes(Attributes* attributes) const {
  assert(attributes != nullptr);
  assert(!attributes->has_id());

  attributes_insert_stmt_->Reset();

  BIND_IF_PRESENT(*attributes_insert_stmt_, Text, attributes, owner_user);
  BIND_IF_PRESENT(*attributes_insert_stmt_, Text, attributes, owner_group);
  BIND_IF_PRESENT(*attributes_insert_stmt_, Int, attributes, uid);
  BIND_IF_PRESENT(*attributes_insert_stmt_, Int, attributes, gid);
  BIND_IF_PRESENT(*attributes_insert_stmt_, Int, attributes, mode);

  int code = attributes_insert_stmt_->StepUntilNotBusy();

  if (code == SQLITE_DONE) {
    attributes->set_id(sqlite3_last_insert_rowid(db()));
  } else {
    std::cerr << sqlite3_errmsg(db()) << std::endl;
    std::cerr << attributes->DebugString() << std::endl;
  }
}

void MetadataDbImpl::WriteNewBlocks(
    boost::shared_ptr<Snapshot> snapshot) const {
  blocks_insert_stmt_->Reset();

  for (Chunk& chunk : *(snapshot->mutable_chunks())) {
    Block* block = chunk.mutable_block();
    if (block->has_id()) {
      continue;
    }

    blocks_insert_stmt_->Reset();
    blocks_insert_stmt_->BindText(":sha1_digest", block->sha1_digest());
    blocks_insert_stmt_->BindInt64(":length", block->length());

    int code = blocks_insert_stmt_->StepUntilNotBusy();

    if (code == SQLITE_DONE) {
      block->set_id(sqlite3_last_insert_rowid(db()));
    } else {
      std::cerr << sqlite3_errmsg(db()) << std::endl;
      std::cerr << block->DebugString() << std::endl;
    }
  }
}

void MetadataDbImpl::WriteNewChunks(
    boost::shared_ptr<Snapshot> snapshot) const {
  files_to_blocks_mapping_insert_stmt_->Reset();

  for (Chunk& chunk : *(snapshot->mutable_chunks())) {
    const Block* block = chunk.mutable_block();
    if (chunk.has_id()) {
      continue;
    }

    files_to_blocks_mapping_insert_stmt_->Reset();
    files_to_blocks_mapping_insert_stmt_->BindInt64(
        ":file_id", snapshot->file().id());
    files_to_blocks_mapping_insert_stmt_->BindInt64(":block_id", block->id());
    files_to_blocks_mapping_insert_stmt_->BindInt64(":offset", chunk.offset());
    files_to_blocks_mapping_insert_stmt_->BindInt64(
        ":observation_time", chunk.observation_time());

    int code = files_to_blocks_mapping_insert_stmt_->StepUntilNotBusy();

    if (code == SQLITE_DONE) {
      chunk.set_id(sqlite3_last_insert_rowid(db()));
    } else {
      std::cerr << sqlite3_errmsg(db()) << std::endl;
      std::cerr << chunk.DebugString() << std::endl;
    }
  }
}

void MetadataDbImpl::UpdateLatestChunksCache(
    int64_t previous_snapshot_id,
    boost::shared_ptr<Snapshot> snapshot) const {
  if (previous_snapshot_id > 0) {
    snapshots_to_files_to_blocks_mapping_delete_stmt_->Reset();
    snapshots_to_files_to_blocks_mapping_delete_stmt_->BindInt64(
        ":snapshot_id", previous_snapshot_id);
    snapshots_to_files_to_blocks_mapping_delete_stmt_->StepUntilNotBusy();
  }

  for (const Chunk& chunk : *(snapshot->mutable_chunks())) {
    assert(chunk.has_id());
    snapshots_to_files_to_blocks_mapping_insert_stmt_->Reset();
    snapshots_to_files_to_blocks_mapping_insert_stmt_->BindInt64(
        ":snapshot_id", snapshot->id());
    snapshots_to_files_to_blocks_mapping_insert_stmt_->BindInt64(
        ":files_to_blocks_id", chunk.id());

    if (snapshots_to_files_to_blocks_mapping_insert_stmt_->StepUntilNotBusy() !=
        SQLITE_DONE) {
      std::cerr << sqlite3_errmsg(db()) << std::endl;
    }
  }
}

void MetadataDbImpl::WriteNewBundle(
    boost::shared_ptr<AnnotatedBundleData> bundle) const {
  assert(!bundle->annotations().has_id());

  bundles_insert_stmt_->Reset();

  bundles_insert_stmt_->BindText(
      ":sha256_linear_digest", bundle->annotations().sha256_linear_digest());
  bundles_insert_stmt_->BindText(
      ":sha256_tree_digest", bundle->annotations().sha256_tree_digest());
  bundles_insert_stmt_->BindInt64(
      ":length", bundle->file_contents_size());

  int code = bundles_insert_stmt_->StepUntilNotBusy();

  if (code == SQLITE_DONE) {
    bundle->mutable_annotations()->set_id(sqlite3_last_insert_rowid(db()));
  } else {
    std::cerr << sqlite3_errmsg(db()) << std::endl;
    std::cerr << bundle->annotations().DebugString() << std::endl;
  }
}

void MetadataDbImpl::WriteNewBlockToBundleMappings(
    boost::shared_ptr<AnnotatedBundleData> bundle) const {
  for (const auto& payload : bundle->manifest().payloads()) {
    for (const auto& block : payload.blocks()) {
      blocks_to_bundles_mapping_insert_stmt_->Reset();
      blocks_to_bundles_mapping_insert_stmt_->BindInt64(
          ":block_id", block.id());
      blocks_to_bundles_mapping_insert_stmt_->BindInt64(
          ":bundle_id", bundle->annotations().id());

      if (blocks_to_bundles_mapping_insert_stmt_->StepUntilNotBusy() !=
          SQLITE_DONE) {
        std::cerr << sqlite3_errmsg(db()) << std::endl;
        std::cerr << bundle->annotations().DebugString() << std::endl;
      }
    }
  }
}

// static
sqlite3* MetadataDbImpl::db() {
  static once_flag once = BOOST_ONCE_INIT;
  call_once(InitDb, once);
  return db_;
}

// static
void MetadataDbImpl::InitDb() {
  assert(sqlite3_threadsafe());
  int code = sqlite3_open_v2(
      "metadata.db", &db_,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr);
  assert(code == SQLITE_OK);
}

}  // polar_express
