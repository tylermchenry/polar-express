#ifndef BUNDLE_STATE_MACHINE_H
#define BUNDLE_STATE_MACHINE_H

#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "callback.h"
#include "macros.h"
#include "overrideable-unique-ptr.h"
#include "state-machine.h"

namespace polar_express {

class AnnotatedBundleData;
class Bundle;
class BundleStateMachine;
class Chunk;
class ChunkHasher;
class ChunkReader;
class FileWriter;
class Snapshot;

// A state machine which goes through the process of generating new bundles as
// new snapshots are provided to it.
//
// The state machine generally operates like this:
//
//  - Waits for a new snapshot.
//  - When a snapshot has arrived, queue its chunks for processing.
//  - For the next chunk in the queue:
//    - Check to see if it is in any bundles already, if so skip.
//    - Read chunk contents into memory, compare to hash. If mismatch, skip.
//    - Compress chunk contents, add to current bundle.
//    - If current bundle is under max size, process next chunk (loop).
//  - Once current bundle exceeds max size:
//    - Encrypt the bundle.
//    - Record the bundle to metadata DB.
//    - Write the bundle to temp storage on disk.
//    - Start a new bundle, and continue processing chunks (previous loop).
//
// Once the chunk queue is emptied, it returns to waiting for the next
// snapshot. The only way this state machine exits is if an external object
// instructs it to. This instruction is only respected when in the HaveChunks
// state with no chunks remaining (so it will not pre-empt the processing of any
// chunks that are already queued). When the instruction is noticed, the current
// bundle is immediately finalized (as if it had exceeded max size).
//
// If the current bundle is empty when finalized, the control flow
// exits. Therefore, the exit mechanism does the following:
//
// - Notice that there are no chunks and that exit has been requested.
// - Force finalize current bundle.
// - Encrypt, record, write the current bundle.
// - Return to chunk processing.
// - Notice that there are no chunks and that exit has been requested.
// - Force finalize the current bundle.
// - Notice that the curren bundle is empty, exit.
//
// TODO(tylermchenry): Add states to handle the case that the chunk has changed
// in the file that the snapshot refers to, but exists elsewhere on disk.
class BundleStateMachineImpl
    : public StateMachine<BundleStateMachineImpl, BundleStateMachine> {
 public:
  BundleStateMachineImpl();
  virtual ~BundleStateMachineImpl();

  // Sets a callback to be invoked when the current snapshot is
  // finished being bundled, and a new snapshot may be presented for
  // bundling.
  void SetSnapshotDoneCallback(Callback callback);

  // Sets a callback to be invoked whenever a new bundle is finished
  // and ready to be picked up via a call to GetAndClearGeneratedBundles.
  //
  // After invoking this callback, the state machine will pause until
  // RetrieveGeneratedBundle is called.
  void SetBundleReadyCallback(Callback callback);

  // Provides a new snapshot to be bundled. Chunks that are different
  // between this snapshot and the previous snapshot of the same file will
  // be written into one or more bundles.
  //
  // After the initial call to BundleSnapshot, additional calls may be
  // made only in response to the snapshot done callback being invoked.
  void BundleSnapshot(boost::shared_ptr<Snapshot> snapshot);

  // Returns a protocol buffer describing the bundle that has been
  // generated by the state machine. The bundle will have been given
  // an ID already, which has been recorded to the metadata database.
  //
  // It is only legal to call this method exactly once in response to
  // each invocation of the bundle ready callback.
  boost::shared_ptr<AnnotatedBundleData> RetrieveGeneratedBundle();

  // Instructs the state machine to exit once it finishes processing
  // its queue of snapshots.
  //
  // Once BundleSnapshot has been invoked at least once, it is only
  // legal to call this method in response to an invocation of the
  // snapshot done callback. It is illegal to call BundleSnapshot
  // after calling FinishAndExit, or to call this method twice.
  void FinishAndExit();

  PE_STATE_MACHINE_DEFINE_INITIAL_STATE(WaitForNewSnapshot);
  PE_STATE_MACHINE_DEFINE_STATE(HaveChunks);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForExistingBundleInfo);
  PE_STATE_MACHINE_DEFINE_STATE(HaveExistingBundleInfo);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForChunkContents);
  PE_STATE_MACHINE_DEFINE_STATE(HaveChunkContents);
  PE_STATE_MACHINE_DEFINE_STATE(HaveChunkContentsAndHashValidity);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForCompression);
  PE_STATE_MACHINE_DEFINE_STATE(ChunkFinished);
  PE_STATE_MACHINE_DEFINE_STATE(HaveBundle);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForEncryption);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForBundleToRecord);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForBundleToWrite);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForBundleRetrieval);
  PE_STATE_MACHINE_DEFINE_STATE(Done);

 protected:
  PE_STATE_MACHINE_DEFINE_EVENT(NewSnapshotReady);
  PE_STATE_MACHINE_DEFINE_EVENT(NoChunksRemaining);
  PE_STATE_MACHINE_DEFINE_EVENT(NewChunkReady);
  PE_STATE_MACHINE_DEFINE_EVENT(ExistingBundleInfoReady);
  PE_STATE_MACHINE_DEFINE_EVENT(ChunkAlreadyInBundle);
  PE_STATE_MACHINE_DEFINE_EVENT(ChunkNotYetInBundle);
  PE_STATE_MACHINE_DEFINE_EVENT(ChunkContentsReady);
  PE_STATE_MACHINE_DEFINE_EVENT(ChunkContentsHashReady);
  PE_STATE_MACHINE_DEFINE_EVENT(ChunkContentsHashMismatch);
  PE_STATE_MACHINE_DEFINE_EVENT(ChunkContentsHashMatch);
  PE_STATE_MACHINE_DEFINE_EVENT(CompressionDone);
  PE_STATE_MACHINE_DEFINE_EVENT(MaxBundleSizeNotReached);
  PE_STATE_MACHINE_DEFINE_EVENT(MaxBundleSizeReached);
  PE_STATE_MACHINE_DEFINE_EVENT(BundleEmpty);
  PE_STATE_MACHINE_DEFINE_EVENT(BundleReady);
  PE_STATE_MACHINE_DEFINE_EVENT(EncryptionDone);
  PE_STATE_MACHINE_DEFINE_EVENT(BundleRecorded);
  PE_STATE_MACHINE_DEFINE_EVENT(BundleWritten);
  PE_STATE_MACHINE_DEFINE_EVENT(BundleRetrieved);
  PE_STATE_MACHINE_DEFINE_EVENT(FlushForced);

  PE_STATE_MACHINE_DEFINE_ACTION(StartNewSnapshot);
  PE_STATE_MACHINE_DEFINE_ACTION(ResetForNextSnapshot);
  PE_STATE_MACHINE_DEFINE_ACTION(GetExistingBundleInfo);
  PE_STATE_MACHINE_DEFINE_ACTION(InspectExistingBundleInfo);
  PE_STATE_MACHINE_DEFINE_ACTION(DiscardChunk);
  PE_STATE_MACHINE_DEFINE_ACTION(ReadChunkContents);
  PE_STATE_MACHINE_DEFINE_ACTION(HashChunkContents);
  PE_STATE_MACHINE_DEFINE_ACTION(InspectChunkContents);
  PE_STATE_MACHINE_DEFINE_ACTION(CompressChunkContents);
  PE_STATE_MACHINE_DEFINE_ACTION(FinishChunk);
  PE_STATE_MACHINE_DEFINE_ACTION(FinalizeBundle);
  PE_STATE_MACHINE_DEFINE_ACTION(EncryptBundle);
  PE_STATE_MACHINE_DEFINE_ACTION(RecordBundle);
  PE_STATE_MACHINE_DEFINE_ACTION(WriteBundle);
  PE_STATE_MACHINE_DEFINE_ACTION(ExecuteBundleReadyCallback);
  PE_STATE_MACHINE_DEFINE_ACTION(ResetForNextBundle);
  PE_STATE_MACHINE_DEFINE_ACTION(CleanUp);

  PE_STATE_MACHINE_TRANSITION_TABLE(
      PE_STATE_MACHINE_TRANSITION(
          WaitForNewSnapshot,
          NewSnapshotReady,
          StartNewSnapshot,
          HaveChunks),
      PE_STATE_MACHINE_TRANSITION(
          HaveChunks,
          FlushForced,
          FinalizeBundle,
          HaveBundle),
      PE_STATE_MACHINE_TRANSITION(
          HaveChunks,
          NoChunksRemaining,
          ResetForNextSnapshot,
          WaitForNewSnapshot),
      PE_STATE_MACHINE_TRANSITION(
          HaveChunks,
          NewChunkReady,
          GetExistingBundleInfo,
          WaitForExistingBundleInfo),
      PE_STATE_MACHINE_TRANSITION(
          WaitForExistingBundleInfo,
          ExistingBundleInfoReady,
          InspectExistingBundleInfo,
          HaveExistingBundleInfo),
      PE_STATE_MACHINE_TRANSITION(
          HaveExistingBundleInfo,
          ChunkAlreadyInBundle,
          DiscardChunk,
          HaveChunks),
      PE_STATE_MACHINE_TRANSITION(
          HaveExistingBundleInfo,
          ChunkNotYetInBundle,
          ReadChunkContents,
          WaitForChunkContents),
      PE_STATE_MACHINE_TRANSITION(
          WaitForChunkContents,
          ChunkContentsReady,
          HashChunkContents,
          HaveChunkContents),
      PE_STATE_MACHINE_TRANSITION(
          HaveChunkContents,
          ChunkContentsHashReady,
          InspectChunkContents,
          HaveChunkContentsAndHashValidity),
      PE_STATE_MACHINE_TRANSITION(
          HaveChunkContentsAndHashValidity,
          ChunkContentsHashMismatch,
          DiscardChunk,
          HaveChunks),
      PE_STATE_MACHINE_TRANSITION(
          HaveChunkContentsAndHashValidity,
          ChunkContentsHashMatch,
          CompressChunkContents,
          WaitForCompression),
      PE_STATE_MACHINE_TRANSITION(
          WaitForCompression,
          CompressionDone,
          FinishChunk,
          ChunkFinished),
      PE_STATE_MACHINE_TRANSITION(
          ChunkFinished,
          MaxBundleSizeNotReached,
          DiscardChunk,
          HaveChunks),
      PE_STATE_MACHINE_TRANSITION(
          ChunkFinished,
          MaxBundleSizeReached,
          FinalizeBundle,
          HaveBundle),
      PE_STATE_MACHINE_TRANSITION(
          HaveBundle,
          BundleEmpty,
          CleanUp,
          Done),
      PE_STATE_MACHINE_TRANSITION(
          HaveBundle,
          BundleReady,
          EncryptBundle,
          WaitForEncryption),
      PE_STATE_MACHINE_TRANSITION(
          WaitForEncryption,
          EncryptionDone,
          RecordBundle,
          WaitForBundleToRecord),
      PE_STATE_MACHINE_TRANSITION(
          WaitForBundleToRecord,
          BundleRecorded,
          WriteBundle,
          WaitForBundleToWrite),
      PE_STATE_MACHINE_TRANSITION(
          WaitForBundleToWrite,
          BundleWritten,
          ExecuteBundleReadyCallback,
          WaitForBundleRetrieval),
      PE_STATE_MACHINE_TRANSITION(
          WaitForBundleRetrieval,
          BundleRetrieved,
          ResetForNextBundle,
          HaveChunks));

  void InternalStart(const string& root);

 private:
  void PushPendingChunksForSnapshot(boost::shared_ptr<Snapshot> snapshot);

  // Returns false and does not modify argument if the pending
  // snapshots queue is empty.
  bool PopPendingChunk(const Chunk** chunk);

  // This method should be called whenever transitioning into the
  // HaveChunks state. Discards the current active chunk (if any), and
  // makes the next pending chunk (if any) the active chunk.
  //
  // If the previously active chunk was the last chunk for a snapshot,
  // this relinquishes the shared_ptr being held for this snapshot.
  //
  // This method will always conclude by posting one of the three
  // events with an outbound edge from the HaveChunks state (new chunk
  // ready, no chunks remaining, or flush forced).
  void NextChunk();

  string root_;
  Callback snapshot_done_callback_;
  Callback bundle_ready_callback_;
  bool exit_requested_;

  boost::shared_ptr<Snapshot> pending_snapshot_;

  // Pending chunk pointers are owned by pending_shapshot_.
  queue<const Chunk*> pending_chunks_;
  const Chunk* active_chunk_;
  boost::shared_ptr<AnnotatedBundleData> existing_bundle_for_active_chunk_;
  vector<char> block_data_for_active_chunk_;
  bool active_chunk_hash_is_valid_;
  vector<char> compressed_block_data_for_active_chunk_;

  boost::shared_ptr<Bundle> active_bundle_;
  boost::shared_ptr<AnnotatedBundleData> generated_bundle_;

  unique_ptr<ChunkReader> chunk_reader_;
  OverrideableUniquePtr<ChunkHasher> chunk_hasher_;
  OverrideableUniquePtr<FileWriter> file_writer_;

  DISALLOW_COPY_AND_ASSIGN(BundleStateMachineImpl);
};

class BundleStateMachine : public BundleStateMachineImpl::BackEnd {
 public:
  BundleStateMachine() {}

  virtual void Start(const string& root);

 protected:
  virtual BundleStateMachineImpl::BackEnd* GetBackEnd();

 private:
  DISALLOW_COPY_AND_ASSIGN(BundleStateMachine);
};

}  // namespace polar_express

#endif  // BUNDLE_STATE_MACHINE_H
