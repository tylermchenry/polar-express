#include "snapshot-state-machine.h"

#include <iostream>

#include "boost/thread/once.hpp"
#include "boost/thread/mutex.hpp"
#include "sqlite3.h"

#include "candidate-snapshot-generator.h"
#include "chunk-hasher.h"
#include "proto/block.pb.h"
#include "proto/snapshot.pb.h"

namespace polar_express {

sqlite3* SnapshotStateMachineImpl::metadata_db_ = nullptr;

void SnapshotStateMachine::Start(
    const string& root, const filesystem::path& filepath) {
  InternalStart(root, filepath);
}

SnapshotStateMachineImpl::BackEnd* SnapshotStateMachine::GetBackEnd() {
  return this;
}

SnapshotStateMachineImpl::SnapshotStateMachineImpl()
    : candidate_snapshot_generator_(new CandidateSnapshotGenerator),
      chunk_hasher_(new ChunkHasher) {
}

SnapshotStateMachineImpl::~SnapshotStateMachineImpl() {
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, RequestGenerateCandidateSnapshot) {
  candidate_snapshot_generator_->GenerateCandidateSnapshot(
      root_, filepath_, &candidate_snapshot_,
      CreateExternalEventCallback<CandidateSnapshotReady>());
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, InspectCandidateSnapshot) {
  if (candidate_snapshot_->is_regular() &&
      !candidate_snapshot_->is_deleted()) {
    PostEvent<NeedChunkHashes>();
  } else {
    PostEvent<ReadyToRecord>();
  }
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, RequestGenerateAndHashChunks) {
  chunk_hasher_->GenerateAndHashChunks(
      filepath_, candidate_snapshot_,
      CreateExternalEventCallback<ChunkHashesReady>());
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, InspectChunkHashes) {
  PostEvent<ReadyToRecord>();
}

PE_STATE_MACHINE_ACTION_HANDLER(
    SnapshotStateMachineImpl, RecordCandidateSnapshotAndChunks) {
  sqlite3_stmt* chunk_insert_stmt = nullptr;
  sqlite3_prepare_v2(
      GetMetadataDb(),
      "insert into blocks ('sha1_digest', 'length') "
      "values (:sha1_digest, :length);", -1,
      &chunk_insert_stmt, nullptr);

  sqlite3_exec(GetMetadataDb(), "begin transaction;",
               nullptr, nullptr, nullptr);
  
  for (const Chunk& chunk : candidate_snapshot_->chunks()) {
    const Block& block = chunk.block();
    sqlite3_reset(chunk_insert_stmt);
    sqlite3_bind_text(
        chunk_insert_stmt,
        sqlite3_bind_parameter_index(chunk_insert_stmt, ":sha1_digest"),
        block.sha1_digest().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(
        chunk_insert_stmt,
        sqlite3_bind_parameter_index(chunk_insert_stmt, ":length"),
        block.length());
    int code;
    do {
      code = sqlite3_step(chunk_insert_stmt);
    } while (code == SQLITE_BUSY);

    if (code != SQLITE_DONE) {
      std::cerr << sqlite3_errmsg(GetMetadataDb()) << std::endl;
      std::cerr << sqlite3_sql(chunk_insert_stmt) << std::endl;
      std::cerr << block.DebugString() << std::endl;
    }
  }

  sqlite3_exec(GetMetadataDb(), "commit;", nullptr, nullptr, nullptr);

  sqlite3_finalize(chunk_insert_stmt);
}

void SnapshotStateMachineImpl::InternalStart(
    const string& root, const filesystem::path& filepath) {
  root_ = root;
  filepath_ = filepath;
  PostEvent<NewFilePathReady>();
}

// static
sqlite3* SnapshotStateMachineImpl::GetMetadataDb() {
  static once_flag once = BOOST_ONCE_INIT;
  call_once(InitMetadataDb, once);
  return metadata_db_;
}

// static
void SnapshotStateMachineImpl::InitMetadataDb() {
  assert(sqlite3_threadsafe());
  int code = sqlite3_open_v2(
      "metadata.db", &metadata_db_,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr);
  assert(code == SQLITE_OK);
}


}  // namespace polar_express
