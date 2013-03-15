#include "metadata-db-impl.h"

#include "boost/thread/once.hpp"
#include "sqlite3.h"

#include "proto/file.pb.h"
#include "proto/snapshot.pb.h"
#include "sqlite3-helpers.h"

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
  ScopedStatement block_insert_stmt(db());

  block_insert_stmt.Prepare(
      "insert into blocks ('sha1_digest', 'length') "
      "values (:sha1_digest, :length);");

  for (const Chunk& chunk : snapshot->chunks()) {
    const Block& block = chunk.block();
    if (block.has_id()) {
      continue;
    }

    block_insert_stmt.Reset();
    block_insert_stmt.BindText(":sha1_digest", block.sha1_digest());
    block_insert_stmt.BindInt64(":length", block.length());
        
    int code;
    do {
      code = block_insert_stmt.Step();
    } while (code == SQLITE_BUSY);

    if (code != SQLITE_DONE) {
      std::cerr << sqlite3_errmsg(db()) << std::endl;
      std::cerr << block.DebugString() << std::endl;
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
