#include "bundle-state-machine.h"

#include "bundle.h"
#include "bundle-hasher.h"
#include "chunk-hasher.h"
#include "chunk-reader.h"
#include "compressor.h"
#include "file-writer.h"
#include "metadata-db.h"
#include "proto/block.pb.h"
#include "proto/file.pb.h"
#include "proto/snapshot.pb.h"

namespace polar_express {
namespace {

// TODO: These should be configurable.
const size_t kMaxBundleSize = 20 * (1 << 20);  // 20 MiB
const size_t kMaxCompressionBufferSize = 2 * (1 << 20);  // 2 MiB

}  // namespace

void BundleStateMachine::Start(
    const string& root,
    Cryptor::EncryptionType encryption_type,
    boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data) {
  InternalStart(root, encryption_type, encryption_keying_data);
}

BundleStateMachineImpl::BackEnd* BundleStateMachine::GetBackEnd() {
  return this;
}

BundleStateMachineImpl::BundleStateMachineImpl()
    : exit_requested_(false),
      active_chunk_(nullptr),
      active_chunk_hash_is_valid_(false),
      active_bundle_(new Bundle),
      chunk_hasher_(new ChunkHasher),
      compressor_(
          // TODO(tylermchenry): Compression type should be configurable.
          Compressor::CreateCompressor(BundlePayload::COMPRESSION_TYPE_ZLIB)),
      bundle_hasher_(new BundleHasher),
      metadata_db_(new MetadataDb),
      file_writer_(new FileWriter) {
  compressor_->InitializeCompression(kMaxCompressionBufferSize);
}

BundleStateMachineImpl::~BundleStateMachineImpl() {
}

void BundleStateMachineImpl::SetSnapshotDoneCallback(Callback callback) {
  snapshot_done_callback_ = callback;
}

void BundleStateMachineImpl::SetBundleReadyCallback(Callback callback) {
  bundle_ready_callback_ = callback;
}

void BundleStateMachineImpl::BundleSnapshot(
    boost::shared_ptr<Snapshot> snapshot) {
  assert(pending_snapshot_ == nullptr);
  pending_snapshot_ = snapshot;
  PostEvent<NewSnapshotReady>();
}

void BundleStateMachineImpl::FinishAndExit() {
  exit_requested_ = true;
  PostEvent<NewSnapshotReady>();
}

boost::shared_ptr<AnnotatedBundleData>
BundleStateMachineImpl::RetrieveGeneratedBundle() const {
  assert(generated_bundle_ != nullptr);
  return generated_bundle_;
}

void BundleStateMachineImpl::Continue() {
  assert(generated_bundle_ != nullptr);
  generated_bundle_.reset();
  PostEvent<ContinueAfterBundleRetrieved>();
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, StartNewSnapshot) {
  chunk_reader_.reset();
  if (pending_snapshot_ != nullptr) {
    PushPendingChunksForSnapshot(pending_snapshot_);
    chunk_reader_ = ChunkReader::CreateChunkReaderForPath(
        root_ + pending_snapshot_->file().path());
  }
  NextChunk();
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, ResetForNextSnapshot) {
  assert(pending_chunks_.empty());
  pending_snapshot_.reset();
  if (snapshot_done_callback_) {
    snapshot_done_callback_();
  }
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, GetExistingBundleInfo) {
  // TODO: Retrieve existing bundle info for this block from metadata DB.
  PostEvent<ExistingBundleInfoReady>();
}

PE_STATE_MACHINE_ACTION_HANDLER(
    BundleStateMachineImpl, InspectExistingBundleInfo) {
  if (existing_bundle_for_active_chunk_ != nullptr &&
      existing_bundle_for_active_chunk_->annotations().id() >= 0) {
    PostEvent<ChunkAlreadyInBundle>();
  } else {
    PostEvent<ChunkNotYetInBundle>();
  }
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, DiscardChunk) {
  NextChunk();
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, ReadChunkContents) {
  block_data_for_active_chunk_.clear();
  chunk_reader_->ReadBlockDataForChunk(
      *active_chunk_, &block_data_for_active_chunk_,
      CreateExternalEventCallback<ChunkContentsReady>());
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, HashChunkContents) {
  active_chunk_hash_is_valid_ = false;
  chunk_hasher_->ValidateHash(
      *active_chunk_, block_data_for_active_chunk_,
      &active_chunk_hash_is_valid_,
      CreateExternalEventCallback<ChunkContentsHashReady>());
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, InspectChunkContents) {
  if (active_chunk_hash_is_valid_) {
    PostEvent<ChunkContentsHashMatch>();
  } else {
    // TODO: Some way to signal back to the executor that this file
    // needs to be snapshotted again.
    PostEvent<ChunkContentsHashMismatch>();
  }
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, CompressChunkContents) {
  compressor_->CompressData(
      block_data_for_active_chunk_, &compressed_block_data_for_active_chunk_,
      CreateExternalEventCallback<CompressionDone>());
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, FinishChunk) {
  assert(active_chunk_ != nullptr);
  assert(active_bundle_ != nullptr);
  assert(!active_bundle_->is_finalized());

  // Compression is finished; get rid of uncompressed data to free memory.
  block_data_for_active_chunk_.clear();

  if (active_bundle_->manifest().payloads_size() == 0) {
    active_bundle_->StartNewPayload(compressor_->compression_type());
  }

  active_bundle_->AddBlockMetadata(active_chunk_->block());
  active_bundle_->AppendBlockContents(compressed_block_data_for_active_chunk_);

  compressed_block_data_for_active_chunk_.clear();

  if (active_bundle_->size() >= kMaxBundleSize) {
    PostEvent<MaxBundleSizeReached>();
  } else {
    PostEvent<MaxBundleSizeNotReached>();
  }
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, FinalizeBundle) {
  assert(active_bundle_ != nullptr);
  assert(!active_bundle_->is_finalized());
  assert(generated_bundle_ == nullptr);

  // May have been flushed here without adding any data to the bundle.
  if (active_bundle_->manifest().payloads_size() == 0) {
    PostEvent<BundleEmpty>();
    return;
  }

  compressor_->FinalizeCompression(&compressed_block_data_for_active_chunk_);
  active_bundle_->AppendBlockContents(compressed_block_data_for_active_chunk_);
  compressed_block_data_for_active_chunk_.clear();

  if (active_bundle_->size() > 0) {
    // Capture the raw data from the active bundle in
    // generated_bundle_ and then reset the active bundle.
    active_bundle_->Finalize();
    generated_bundle_.reset(new AnnotatedBundleData(active_bundle_));
    active_bundle_.reset();
    PostEvent<BundleReady>();
  } else {
    PostEvent<BundleEmpty>();
  }
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, EncryptBundle) {
  assert(generated_bundle_ != nullptr);
  assert(cryptor_ != nullptr);

  cryptor_->InitializeEncryption(*CHECK_NOTNULL(encryption_keying_data_));
  cryptor_->EncryptData(
      generated_bundle_->mutable_data(),
      CreateExternalEventCallback<EncryptionDone>());
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, HashBundle) {
  assert(generated_bundle_ != nullptr);
  assert(cryptor_ != nullptr);

  cryptor_->FinalizeEncryption(
      generated_bundle_->mutable_encryption_headers().get(),
      generated_bundle_->mutable_message_authentication_code().get());

  bundle_hasher_->ComputeSequentialHashes(
      generated_bundle_->file_contents(),
      generated_bundle_->mutable_annotations()->mutable_sha256_linear_digest(),
      generated_bundle_->mutable_annotations()->mutable_sha256_tree_digest(),
      CreateExternalEventCallback<BundleHashed>());
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, RecordBundle) {
  assert(generated_bundle_ != nullptr);
  metadata_db_->RecordNewBundle(
      generated_bundle_,
      CreateExternalEventCallback<BundleRecorded>());
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, WriteBundle) {
  assert(generated_bundle_ != nullptr);
  file_writer_->WriteSequentialDataToTemporaryFile(
      generated_bundle_->file_contents(),
      generated_bundle_->unique_filename() + "_",
      generated_bundle_->mutable_annotations()->mutable_persistence_file_path(),
      CreateExternalEventCallback<BundleWritten>());
}

PE_STATE_MACHINE_ACTION_HANDLER(
    BundleStateMachineImpl, ExecuteBundleReadyCallback) {
  if (bundle_ready_callback_) {
    bundle_ready_callback_();
  }
  SetIdle();
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, ResetForNextBundle) {
  assert(active_bundle_ == nullptr);
  generated_bundle_.reset();
  active_bundle_.reset(new Bundle);
  compressor_->InitializeCompression(kMaxCompressionBufferSize);
  NextChunk();
}

PE_STATE_MACHINE_ACTION_HANDLER(BundleStateMachineImpl, CleanUp) {
  // Nothing to do in current implementation.
}

void BundleStateMachineImpl::InternalStart(
    const string& root,
    Cryptor::EncryptionType encryption_type,
    boost::shared_ptr<const Cryptor::KeyingData> encryption_keying_data) {
  root_ = root;
  encryption_keying_data_ = CHECK_NOTNULL(encryption_keying_data);
  cryptor_ = Cryptor::CreateCryptor(encryption_type);
}

void BundleStateMachineImpl::PushPendingChunksForSnapshot(
    boost::shared_ptr<Snapshot> snapshot) {
  const Chunk* chunk_ptr = nullptr;
  for (const auto& chunk : snapshot->chunks()) {
    chunk_ptr = &chunk;
    pending_chunks_.push(chunk_ptr);
  }
}

bool BundleStateMachineImpl::PopPendingChunk(const Chunk** chunk) {
  if (pending_chunks_.empty()) {
    return false;
  }
  *CHECK_NOTNULL(chunk) = pending_chunks_.front();
  pending_chunks_.pop();
  return true;
}

void BundleStateMachineImpl::NextChunk() {
  if (PopPendingChunk(&active_chunk_)) {
    PostEvent<NewChunkReady>();
  } else if (exit_requested_) {
    PostEvent<FlushForced>();
  } else {
    PostEvent<NoChunksRemaining>();
    SetIdle();
  }
}

}  // namespace polar_express
