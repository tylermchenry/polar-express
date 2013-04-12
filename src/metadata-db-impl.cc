#include "metadata-db-impl.h"

#include "boost/thread/once.hpp"
#include "sqlite3.h"

#include "proto/file.pb.h"
#include "proto/snapshot.pb.h"
#include "sqlite3-helpers.h"

#define HAS_FIELD(field_name) has_ ## field_name

#define BIND_TYPE(type) Bind ## type

#define COL_NAME(field_name) ":" #field_name

#define BIND_IF_PRESENT(stmt, type, proto_ptr, field_name)    \
  do {                                                               \
    if ((proto_ptr)->HAS_FIELD(field_name)()) {                      \
      (stmt).BIND_TYPE(type)(COL_NAME(field_name),                   \
                             (proto_ptr)->field_name());             \
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
  
  ScopedStatement snapshot_select_stmt(db());
  snapshot_select_stmt.Prepare(
      "select * from snapshots join attributes on "
      "snapshots.attributes_id = attributes.id "
      "where file_id = :file_id "
      "order by observation_time desc limit 1;");

  snapshot_select_stmt.BindInt64(":file_id", file.id());

  if (snapshot_select_stmt.StepUntilNotBusy() == SQLITE_ROW) {
    (*snapshot)->set_id(snapshot_select_stmt.GetColumnInt64("snapshots.id"));

    (*snapshot)->mutable_file()->CopyFrom(file);
    
    Attributes* attributes = (*snapshot)->mutable_attributes();
    attributes->set_id(snapshot_select_stmt.GetColumnInt64("attributes.id"));
    attributes->set_owner_user(
        snapshot_select_stmt.GetColumnText("attributes.owner_user"));
    attributes->set_owner_group(
        snapshot_select_stmt.GetColumnText("attributes.owner_group"));
    attributes->set_uid(
        snapshot_select_stmt.GetColumnInt64("attributes.uid"));
    attributes->set_gid(
        snapshot_select_stmt.GetColumnInt64("attributes.gid"));
    attributes->set_mode(
        snapshot_select_stmt.GetColumnInt64("attributes.mode"));
    
    (*snapshot)->set_creation_time(
        snapshot_select_stmt.GetColumnInt64("snapshots.creation_time"));
    (*snapshot)->set_modification_time(
        snapshot_select_stmt.GetColumnInt64("snapshots.modification_time"));
    (*snapshot)->set_access_time(
        snapshot_select_stmt.GetColumnInt64("snapshots.access_time"));
    // TODO: Extra attributes
    (*snapshot)->set_is_regular(
        snapshot_select_stmt.GetColumnBool("snapshots.is_regular"));
    (*snapshot)->set_is_deleted(
        snapshot_select_stmt.GetColumnBool("snapshots.is_deleted"));
    (*snapshot)->set_sha1_digest(
        snapshot_select_stmt.GetColumnText("snapshots.sha1_digest"));
    (*snapshot)->set_length(
        snapshot_select_stmt.GetColumnInt64("snapshots.length"));
    (*snapshot)->set_observation_time(
        snapshot_select_stmt.GetColumnInt64("snapshots.observation_time"));
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

  WriteNewSnapshot(snapshot);

  // TODO: Also write block-to-file mappings.
  
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
}

void MetadataDbImpl::FindExistingFileId(File* file) const {
  assert(file != nullptr);
  assert(!file->has_id());

  ScopedStatement file_select_stmt(db());
  file_select_stmt.Prepare("select id from files where path = :path;");
  file_select_stmt.BindText(":path", file->path());

  if (file_select_stmt.StepUntilNotBusy() == SQLITE_ROW) {
    file->set_id(file_select_stmt.GetColumnInt64("id"));
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
