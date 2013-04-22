#ifndef BUNDLE_STATE_MACHINE_H
#define BUNDLE_STATE_MACHINE_H

#include "macros.h"
#include "state-machine.h"

namespace polar_express {

class BundleStateMachine;

// A state machine which goes through the process of generating new bundles as
// new snapshots are provided to it.
//
// The state machine generally operates like this:
//
//  - Waits for a new snapshot.
//  - When a snapshot has arrived, queue its blocks for processing.
//  - For the next block in the queue:
//    - Check to see if it is in any bundles already, if so skip.
//    - Read block contents into memory, compare to hash. If mismatch, skip.
//    - Compress block contents, add to current bundle.
//    - If current bundle is under max size, process next block (loop).
//  - Once current bundle exceeds max size:
//    - Encrypt the bundle.
//    - Record the bundle to metadata DB.
//    - Write the bundle to temp storage on disk.
//    - Start a new bundle, and continue processing blocks (previous loop).
//
// Once the block queue is emptied, it returns to waiting for the next
// snapshot. The only way this state machine exits is if an external object
// instructs it to. This instruction is only respected when in the HaveBlocks
// state with no blocks remaining (so it will not pre-empt the processing of any
// blocks that are already queued). When the instruction is noticed, the current
// bundle is immediately finalized (as if it had exceeded max size).
//
// If the current bundle is empty when finalized, the control flow
// exits. Therefore, the exit mechanism does the following:
//
// - Notice that there are no blocks and that exit has been requested.
// - Force finalize current bundle.
// - Encrypt, record, write the current bundle.
// - Return to block processing.
// - Notice that there are no blocks and that exit has been requested.
// - Force finalize the current bundle.
// - Notice that the curren bundle is empty, exit.
//
// TODO(tylermchenry): Add states to handle the case that the block has changed
// in the file that the snapshot refers to, but exists elsewhere on disk.
class BundleStateMachineImpl
    : public StateMachine<BundleStateMachineImpl, BundleStateMachine> {
 public:
  BundleStateMachineImpl();
  virtual ~BundleStateMachineImpl();

  PE_STATE_MACHINE_DEFINE_INITAL_STATE(WaitForNewSnapshot);
  PE_STATE_MACHINE_DEFINE_STATE(HaveBlocks);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForExistingBundleInfo);
  PE_STATE_MACHINE_DEFINE_STATE(HaveExistingBundleInfo);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForBlockContents);
  PE_STATE_MACHINE_DEFINE_STATE(HaveBlockContents);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForCompression);
  PE_STATE_MACHINE_DEFINE_STATE(BlockFinished);
  PE_STATE_MACHINE_DEFINE_STATE(HaveBundle);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForEncryption);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForBundleToRecord);
  PE_STATE_MACHINE_DEFINE_STATE(WaitForBundleToWrite);
  PE_STATE_MACHINE_DEFINE_STATE(BundleFinished);
  PE_STATE_MACHINE_DEFINE_STATE(Done);

 protected:
  PE_STATE_MACHINE_DEFINE_EVENT(NewSnapshotReady);
  PE_STATE_MACHINE_DEFINE_EVENT(NoBlocksRemaining);
  PE_STATE_MACHINE_DEFINE_EVENT(NewBlockReady);
  PE_STATE_MACHINE_DEFINE_EVENT(ExistingBundleInfoReady);
  PE_STATE_MACHINE_DEFINE_EVENT(BlockAlreadyInBundle);
  PE_STATE_MACHINE_DEFINE_EVENT(BlockNotYetInBundle);
  PE_STATE_MACHINE_DEFINE_EVENT(BlockContentsReady);
  PE_STATE_MACHINE_DEFINE_EVENT(BlockContentsHashMismatch);
  PE_STATE_MACHINE_DEFINE_EVENT(BlockContentsHashMatch);
  PE_STATE_MACHINE_DEFINE_EVENT(CompressionDone);
  PE_STATE_MACHINE_DEFINE_EVENT(MaxBundleSizeNotReached);
  PE_STATE_MACHINE_DEFINE_EVENT(MaxBundleSizeReached);
  PE_STATE_MACHINE_DEFINE_EVENT(BundleEmpty);
  PE_STATE_MACHINE_DEFINE_EVENT(BundleReady);
  PE_STATE_MACHINE_DEFINE_EVENT(EncryptionDone);
  PE_STATE_MACHINE_DEFINE_EVENT(BundleRecorded);
  PE_STATE_MACHINE_DEFINE_EVENT(BundleWritten);
  PE_STATE_MACHINE_DEFINE_EVENT(FlushForced);

  PE_STATE_MACHINE_DEFINE_ACTION(StartNewSnapshot);
  PE_STATE_MACHINE_DEFINE_ACTION(ResetForNextSnapshot);
  PE_STATE_MACHINE_DEFINE_ACTION(GetExistingBundleInfo);
  PE_STATE_MACHINE_DEFINE_ACTION(InspectExistingBundleInfo);
  PE_STATE_MACHINE_DEFINE_ACTION(DiscardBlock);
  PE_STATE_MACHINE_DEFINE_ACTION(ReadBlockContents);
  PE_STATE_MACHINE_DEFINE_ACTION(InspectBlockContents);
  PE_STATE_MACHINE_DEFINE_ACTION(CompressBlockContents);
  PE_STATE_MACHINE_DEFINE_ACTION(FinishBlock);
  PE_STATE_MACHINE_DEFINE_ACTION(FinalizeBundle);
  PE_STATE_MACHINE_DEFINE_ACTION(EncryptBundle);
  PE_STATE_MACHINE_DEFINE_ACTION(RecordBundle);
  PE_STATE_MACHINE_DEFINE_ACTION(WriteBundle);
  PE_STATE_MACHINE_DEFINE_ACTION(StartNewBundle);
  PE_STATE_MACHINE_DEFINE_ACTION(CleanUp);

  PE_STATE_MACHINE_TRANSITION_TABLE(
      PE_STATE_MACHINE_TRANSITION(
          WaitForNewSnapshot,
          NewSnapshotReady,
          StartNewSnapshot,
          HaveBlocks),
      PE_STATE_MACHINE_TRANSITION(
          HaveBlocks,
          FlushForced,
          FinalizeBundle,
          HaveBundle),
      PE_STATE_MACHINE_TRANSITION(
          HaveBlocks,
          NoBlocksRemaining,
          ResetForNextSnapshot,
          WaitForNewSnapshot),
      PE_STATE_MACHINE_TRANSITION(
          HaveBlocks,
          NewBlockReady,
          GetExistingBundleInfo,
          WaitForExistingBundleInfo),
      PE_STATE_MACHINE_TRANSITION(
          WaitForExistingBundleInfo,
          ExistingBundleInfoReady,
          InspectExistingBundleInfo,
          HaveExistingBundleInfo),
      PE_STATE_MACHINE_TRANSITION(
          HaveExistingBundleInfo,
          BlockAlreadyInBundle,
          DiscardBlock,
          HaveBlocks),
      PE_STATE_MACHINE_TRANSITION(
          HaveExistingBundleInfo,
          BlockNotYetInBundle,
          ReadBlockContents,
          WaitForBlockContents),
      PE_STATE_MACHINE_TRANSITION(
          WaitForBlockContents,
          BlockContentsReady,
          InspectBlockContents,
          HaveBlockContents),
      PE_STATE_MACHINE_TRANSITION(
          HaveBlockContents,
          BlockContentsHashMismatch,
          DiscardBlock,
          HaveBlocks),
      PE_STATE_MACHINE_TRANSITION(
          HaveBlockContents,
          BlockContentsHashMatch,
          CompressBlockContents,
          WaitForCompression),
      PE_STATE_MACHINE_TRANSITION(
          WaitForCompression,
          CompressionDone,
          FinishBlock,
          BlockFinished),
      PE_STATE_MACHINE_TRANSITION(
          BlockFinished,
          MaxBundleSizeNotReached,
          DiscardBlock,
          HaveBlocks),
      PE_STATE_MACHINE_TRANSITION(
          BlockFinished,
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
          StartNewBundle,
          HaveBlocks));

 private:
  DISALLOW_COPY_AND_ASSIGN(BundleStateMachineImpl);
};

class BundleStateMachine : public BundleStateMachineImpl::BackEnd {
 public:
  BundleStateMachine() {}

  virtual void Start();

 protected:
  virtual BundleStateMachineImpl::BackEnd* GetBackEnd();

 private:
  DISALLOW_COPY_AND_ASSIGN(BundleStateMachine);
};

}  // namespace polar_express

#endif  // BUNDLE_STATE_MACHINE_H
