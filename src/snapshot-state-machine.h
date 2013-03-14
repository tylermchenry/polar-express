#ifndef SNAPSHOT_STATE_MACHINE_H
#define SNAPSHOT_STATE_MACHINE_H

#include <string>

#include "boost/filesystem.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/condition_variable.hpp"

#include "macros.h"
#include "overrideable-scoped-ptr.h"
#include "state-machine.h"

class sqlite3;

namespace polar_express {

class CandidateSnapshotGenerator;
class Chunk;
class ChunkHasher;
class SnapshotStateMachine;
class Snapshot;

// A state machine which goes through the process of generating a snapshot of a
// single file, comparing it with the previous snapshot (if any), and then
// writing information about any updates to the database.
//
// TODO: Currently this is incomplete; it just reads the filesystem metadata and
// prints it to standard output. Expand documentation as the class becomes more
// complete.
class SnapshotStateMachineImpl
  : public StateMachine<SnapshotStateMachineImpl, SnapshotStateMachine> {
 public:
  SnapshotStateMachineImpl();
  virtual ~SnapshotStateMachineImpl();

  PE_STATE_MACHINE_DEFINE_INITIAL_STATE(WaitForNewFilePath);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForCandidateSnapshot);
  PE_STATE_MACHINE_DEFINE_STATE(HaveCandidateSnapshot);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForChunkHashes);
  PE_STATE_MACHINE_DEFINE_STATE(HaveChunkHashes);
  PE_STATE_MACHINE_DEFINE_STATE(Done);
  
 protected:
  PE_STATE_MACHINE_DEFINE_EVENT(NewFilePathReady);
  PE_STATE_MACHINE_DEFINE_EVENT(CandidateSnapshotReady);
  PE_STATE_MACHINE_DEFINE_EVENT(NeedChunkHashes);
  PE_STATE_MACHINE_DEFINE_EVENT(ChunkHashesReady);
  PE_STATE_MACHINE_DEFINE_EVENT(ReadyToRecord);

  PE_STATE_MACHINE_DEFINE_ACTION(RequestGenerateCandidateSnapshot);
  PE_STATE_MACHINE_DEFINE_ACTION(GetCandidateSnapshot);
  PE_STATE_MACHINE_DEFINE_ACTION(RequestGenerateAndHashChunks);
  PE_STATE_MACHINE_DEFINE_ACTION(GetChunkHashes);
  PE_STATE_MACHINE_DEFINE_ACTION(RecordCandidateSnapshotAndChunks);

  PE_STATE_MACHINE_TRANSITION_TABLE(
      PE_STATE_MACHINE_TRANSITION(
          WaitForNewFilePath,
          NewFilePathReady,
          RequestGenerateCandidateSnapshot,
          WaitForCandidateSnapshot),
      PE_STATE_MACHINE_TRANSITION(
          WaitForCandidateSnapshot,
          CandidateSnapshotReady,
          GetCandidateSnapshot,
          HaveCandidateSnapshot),
      PE_STATE_MACHINE_TRANSITION(
          HaveCandidateSnapshot,
          NeedChunkHashes,
          RequestGenerateAndHashChunks,
          WaitForChunkHashes),
      PE_STATE_MACHINE_TRANSITION(
          WaitForChunkHashes,
          ChunkHashesReady,
          GetChunkHashes,
          HaveChunkHashes),
      PE_STATE_MACHINE_TRANSITION(
          HaveChunkHashes,
          ReadyToRecord,
          RecordCandidateSnapshotAndChunks,
          Done),
      PE_STATE_MACHINE_TRANSITION(
          HaveCandidateSnapshot,
          ReadyToRecord,
          RecordCandidateSnapshotAndChunks,
          Done));

  void InternalStart(
    const string& root, const filesystem::path& filepath);
  
 private:
  OverrideableScopedPtr<CandidateSnapshotGenerator>
  candidate_snapshot_generator_;
  OverrideableScopedPtr<ChunkHasher> chunk_hasher_;

  boost::shared_ptr<Snapshot> candidate_snapshot_;
  vector<boost::shared_ptr<Chunk> > chunks_;
  
  string root_;
  filesystem::path filepath_;

  static sqlite3* metadata_db_;
  
  static sqlite3* GetMetadataDb();
  static void InitMetadataDb();
  
  DISALLOW_COPY_AND_ASSIGN(SnapshotStateMachineImpl);
};

class SnapshotStateMachine : public SnapshotStateMachineImpl::BackEnd {
 public:  
  SnapshotStateMachine() {}
  
  virtual void Start(const string& root, const filesystem::path& filepath);

 protected:
  virtual SnapshotStateMachineImpl::BackEnd* GetBackEnd();
  
 private:
  DISALLOW_COPY_AND_ASSIGN(SnapshotStateMachine);
};

}  // namespace polar_express

#endif  // SNAPSHOT_STATE_MACHINE_H
