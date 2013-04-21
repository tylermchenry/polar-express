#include "metadata-db-impl.h"

#include <iostream>

#include "boost/thread/once.hpp"
#include "sqlite3.h"

#include "proto/file.pb.h"
#include "proto/snapshot.pb.h"
#include "sqlite3-helpers.h"

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
    : MetadataDb(false) {
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

  ScopedStatement snapshot_select_stmt(db());
  snapshot_select_stmt.Prepare(
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

  snapshot_select_stmt.BindInt64(":file_id", (*snapshot)->file().id());

  if (snapshot_select_stmt.StepUntilNotBusy() == SQLITE_ROW) {
    SET_IF_PRESENT(snapshot_select_stmt, Int64, *snapshot, snapshots, id);
    
    Attributes* attributes = (*snapshot)->mutable_attributes();
    SET_IF_PRESENT(snapshot_select_stmt, Int64, attributes, attributes, id);
    SET_IF_PRESENT(snapshot_select_stmt, Text, attributes,
                   attributes, owner_user);
    SET_IF_PRESENT(snapshot_select_stmt, Text, attributes,
                   attributes, owner_group);
    SET_IF_PRESENT(snapshot_select_stmt, Int64, attributes, attributes, uid);
    SET_IF_PRESENT(snapshot_select_stmt, Int64, attributes, attributes, gid);
    SET_IF_PRESENT(snapshot_select_stmt, Int64, attributes, attributes, mode);

    SET_IF_PRESENT(snapshot_select_stmt, Int64, *snapshot,
                   snapshots, creation_time);
    SET_IF_PRESENT(snapshot_select_stmt, Int64, *snapshot,
                   snapshots, modification_time);
    SET_IF_PRESENT(snapshot_select_stmt, Int64, *snapshot,
                   snapshots, access_time);
    // TODO: Extra attributes
    SET_IF_PRESENT(snapshot_select_stmt, Bool, *snapshot,
                   snapshots, is_regular);
    SET_IF_PRESENT(snapshot_select_stmt, Bool, *snapshot,
                   snapshots, is_deleted);
    SET_IF_PRESENT(snapshot_select_stmt, Text, *snapshot,
                   snapshots, sha1_digest);
    SET_IF_PRESENT(snapshot_select_stmt, Int64, *snapshot, snapshots, length);
    SET_IF_PRESENT(snapshot_select_stmt, Int64, *snapshot,
                   snapshots, observation_time);
  }
  
  callback();
}

void MetadataDbImpl::RecordNewSnapshot(
    boost::shared_ptr<Snapshot> snapshot, Callback callback) {
  assert(!snapshot->has_id());

  FindExistingIds(snapshot);
  
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
  
  sqlite3_exec(db(), "commit;", nullptr, nullptr, nullptr);

  callback();
}

void MetadataDbImpl::FindExistingIds(
    boost::shared_ptr<Snapshot> snapshot) const {
  if (!snapshot->file().has_id()) {
    FindExistingFileId(snapshot->mutable_file());
  }
  if (!snapshot->attributes().has_id()) {
    FindExistingAttributesId(snapshot->mutable_attributes());
  }
  FindExistingBlockIds(snapshot);
  FindExistingChunkIds(snapshot);
}

void MetadataDbImpl::FindExistingFileId(File* file) const {
  assert(file != nullptr);
  assert(!file->has_id());

  ScopedStatement file_select_stmt(db());
  file_select_stmt.Prepare(
      "select files.id as files_id from files where path = :path;");
  file_select_stmt.BindText(":path", file->path());

  if (file_select_stmt.StepUntilNotBusy() == SQLITE_ROW) {
    SET_IF_PRESENT(file_select_stmt, Int64, file, files, id);
  }
}

void MetadataDbImpl::FindExistingAttributesId(Attributes* attributes) const {
  assert(attributes != nullptr);
  assert(!attributes->has_id());

  ScopedStatement attributes_select_stmt(db());
  attributes_select_stmt.Prepare(
      "select id from attributes where owner_user = :owner_user and "
      "owner_group = :owner_group and uid = :uid and gid = :gid and "
      "mode = :mode;");

  BIND_IF_PRESENT(attributes_select_stmt, Text, attributes, owner_user);
  BIND_IF_PRESENT(attributes_select_stmt, Text, attributes, owner_group);
  BIND_IF_PRESENT(attributes_select_stmt, Int, attributes, uid);
  BIND_IF_PRESENT(attributes_select_stmt, Int, attributes, gid);
  BIND_IF_PRESENT(attributes_select_stmt, Int, attributes, mode);

  if (attributes_select_stmt.StepUntilNotBusy() == SQLITE_ROW) {
    attributes->set_id(attributes_select_stmt.GetColumnInt64("id"));
  }
}

void MetadataDbImpl::FindExistingBlockIds(
    boost::shared_ptr<Snapshot> snapshot) const {
  ScopedStatement block_select_stmt(db());
  block_select_stmt.Prepare(
      "select id from blocks where sha1_digest = :sha1_digest and "
      "length = :length;");

  for (Chunk& chunk : *(snapshot->mutable_chunks())) {
    Block* block = chunk.mutable_block();
    if (block->has_id()) {
      continue;
    }

    block_select_stmt.Reset();
    block_select_stmt.BindText(":sha1_digest", block->sha1_digest());
    block_select_stmt.BindInt64(":length", block->length());

    if (block_select_stmt.StepUntilNotBusy() == SQLITE_ROW) {
      block->set_id(block_select_stmt.GetColumnInt64("id"));
    }
  }
}

void MetadataDbImpl::FindExistingChunkIds(
    boost::shared_ptr<Snapshot> snapshot) const {
  // This finds the most recent file->block mappings for this file. If the most
  // recent file->block mapping for a chunk's offset references the same block
  // ID, then that chunk ID is re-used.
  ScopedStatement mapping_select_stmt(db());
  mapping_select_stmt.Prepare(
      "select distinct id, block_id, offset, "
      "                max(observation_time) as max_observation_time "
      "from files_to_blocks where file_id = :file_id "
      "group by offset;");

  mapping_select_stmt.BindInt64(":file_id", snapshot->file().id());
  
  map<int64_t, boost::shared_ptr<Chunk> > offsets_to_latest_chunks;
  while (mapping_select_stmt.StepUntilNotBusy() == SQLITE_ROW) {
    boost::shared_ptr<Chunk> chunk(new Chunk);
    chunk->set_id(mapping_select_stmt.GetColumnInt64("id"));
    chunk->set_offset(mapping_select_stmt.GetColumnInt64("offset"));
    chunk->mutable_block()->set_id(
        mapping_select_stmt.GetColumnInt64("block_id"));
    chunk->set_observation_time(
        mapping_select_stmt.GetColumnInt64("max_observation_time"));
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

  ScopedStatement snapshot_insert_stmt(db());

  // TODO: Extra attributes.
  snapshot_insert_stmt.Prepare(
      "insert into snapshots ('file_id', 'attributes_id', 'creation_time', "
      "'modification_time', 'access_time', 'is_regular', 'is_deleted', "
      "'sha1_digest', 'length', 'observation_time') "
      "values (:file_id, :attributes_id, :creation_time, "
      ":modification_time, :access_time, :is_regular, :is_deleted, "
      ":sha1_digest, :length, :observation_time);");

  snapshot_insert_stmt.BindInt64(":file_id", snapshot->file().id());
  snapshot_insert_stmt.BindInt64(":attributes_id", snapshot->attributes().id());
  BIND_IF_PRESENT(snapshot_insert_stmt, Int64, snapshot, creation_time);
  snapshot_insert_stmt.BindInt64(":modification_time",
                                 snapshot->modification_time());
  BIND_IF_PRESENT(snapshot_insert_stmt, Int64, snapshot, access_time);
  snapshot_insert_stmt.BindBool(":is_regular", snapshot->is_regular());
  snapshot_insert_stmt.BindBool(":is_deleted", snapshot->is_deleted());
  snapshot_insert_stmt.BindText(":sha1_digest", snapshot->sha1_digest());
  snapshot_insert_stmt.BindInt64(":length", snapshot->length());
  snapshot_insert_stmt.BindInt64(":observation_time",
                                 snapshot->observation_time());

  int code = snapshot_insert_stmt.StepUntilNotBusy();

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

  ScopedStatement file_insert_stmt(db());

  file_insert_stmt.Prepare(
      "insert into files ('path') "
      "values (:path);");

  file_insert_stmt.BindText(":path", file->path());

  int code = file_insert_stmt.StepUntilNotBusy();
    
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

  ScopedStatement attributes_insert_stmt(db());

  attributes_insert_stmt.Prepare(
      "insert into attributes ('owner_user', 'owner_group', 'uid', "
      "'gid', 'mode') "
      "values (:owner_user, :owner_group, :uid, :gid, :mode);");

  BIND_IF_PRESENT(attributes_insert_stmt, Text, attributes, owner_user);
  BIND_IF_PRESENT(attributes_insert_stmt, Text, attributes, owner_group);
  BIND_IF_PRESENT(attributes_insert_stmt, Int, attributes, uid);
  BIND_IF_PRESENT(attributes_insert_stmt, Int, attributes, gid);
  BIND_IF_PRESENT(attributes_insert_stmt, Int, attributes, mode);
  
  int code = attributes_insert_stmt.StepUntilNotBusy();

  if (code == SQLITE_DONE) {
    attributes->set_id(sqlite3_last_insert_rowid(db()));
  } else {
    std::cerr << sqlite3_errmsg(db()) << std::endl;
    std::cerr << attributes->DebugString() << std::endl;
  }
}

void MetadataDbImpl::WriteNewBlocks(
    boost::shared_ptr<Snapshot> snapshot) const {
  ScopedStatement block_insert_stmt(db());

  block_insert_stmt.Prepare(
      "insert into blocks ('sha1_digest', 'length') "
      "values (:sha1_digest, :length);");

  for (Chunk& chunk : *(snapshot->mutable_chunks())) {
    Block* block = chunk.mutable_block();
    if (block->has_id()) {
      continue;
    }

    block_insert_stmt.Reset();
    block_insert_stmt.BindText(":sha1_digest", block->sha1_digest());
    block_insert_stmt.BindInt64(":length", block->length());
    
    int code = block_insert_stmt.StepUntilNotBusy();

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
  ScopedStatement mapping_insert_stmt(db());

  mapping_insert_stmt.Prepare(
      "insert into files_to_blocks "
      "('file_id', 'block_id', 'offset', 'observation_time') "
      "values (:file_id, :block_id, :offset, :observation_time);");

  for (Chunk& chunk : *(snapshot->mutable_chunks())) {
    const Block* block = chunk.mutable_block();
    if (chunk.has_id()) {
      continue;
    }
    
    mapping_insert_stmt.Reset();
    mapping_insert_stmt.BindInt64(":file_id", snapshot->file().id());
    mapping_insert_stmt.BindInt64(":block_id", block->id());
    mapping_insert_stmt.BindInt64(":offset", chunk.offset());
    mapping_insert_stmt.BindInt64(
        ":observation_time", chunk.observation_time());

    int code = mapping_insert_stmt.StepUntilNotBusy();

    if (code == SQLITE_DONE) {
      chunk.set_id(sqlite3_last_insert_rowid(db()));
    } else {
      std::cerr << sqlite3_errmsg(db()) << std::endl;
      std::cerr << chunk.DebugString() << std::endl;
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
