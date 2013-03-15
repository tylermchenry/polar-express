#include "metadata-db-impl.h"

#include "boost/thread/once.hpp"
#include "sqlite3.h"

#include "proto/file.pb.h"
#include "proto/snapshot.pb.h"

namespace polar_express {

sqlite3* MetadataDbImpl::db_ = nullptr;

MetadataDbImpl::MetadataDbImpl()
    : MetadataDb(false) {
}

MetadataDbImpl::~MetadataDbImpl() {
}

void MetadataDbImpl::ReadLatestSnapshot(
    const File& file, boost::shared_ptr<Snapshot> snapshot,
    Callback callback) {
  // TODO: Implement
  callback();
}

void MetadataDbImpl::RecordNewSnapshot(
    boost::shared_ptr<Snapshot> snapshot, Callback callback) {
  sqlite3_exec(db(), "begin transaction;",
               nullptr, nullptr, nullptr);

  WriteNewBlocks(snapshot);
  // TODO: Also write (if necessary): the file, the snapshot itself, and
  // block-to-file mappings.
  
  sqlite3_exec(db(), "commit;", nullptr, nullptr, nullptr);
}

void MetadataDbImpl::WriteNewBlocks(
    boost::shared_ptr<Snapshot> snapshot) const {
  sqlite3_stmt* block_insert_stmt = nullptr;
  
  sqlite3_prepare_v2(
      db(),
      "insert into blocks ('sha1_digest', 'length') "
      "values (:sha1_digest, :length);", -1,
      &block_insert_stmt, nullptr);

  for (const Chunk& chunk : snapshot->chunks()) {
    const Block& block = chunk.block();
    if (block.has_id()) {
      continue;
    }
    
    sqlite3_reset(block_insert_stmt);
    sqlite3_bind_text(
        block_insert_stmt,
        sqlite3_bind_parameter_index(block_insert_stmt, ":sha1_digest"),
        block.sha1_digest().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(
        block_insert_stmt,
        sqlite3_bind_parameter_index(block_insert_stmt, ":length"),
        block.length());
    int code;
    do {
      code = sqlite3_step(block_insert_stmt);
    } while (code == SQLITE_BUSY);

    if (code != SQLITE_DONE) {
      std::cerr << sqlite3_errmsg(db()) << std::endl;
      std::cerr << sqlite3_sql(block_insert_stmt) << std::endl;
      std::cerr << block.DebugString() << std::endl;
    }
  }
  
  sqlite3_finalize(block_insert_stmt);
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
