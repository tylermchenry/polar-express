#include "metadata-db.h"

#include "metadata-db-impl.h"

namespace polar_express {

MetadataDb::MetadataDb()
    : impl_(new MetadataDbImpl),
      strand_dispatcher_(
          AsioDispatcher::GetInstance()->NewStrandDispatcherDiskBound()) {
}

MetadataDb::MetadataDb(bool create_impl)
    : impl_(create_impl ? new MetadataDbImpl : nullptr),
      strand_dispatcher_(
          create_impl
              ? AsioDispatcher::GetInstance()->NewStrandDispatcherDiskBound()
              : boost::shared_ptr<AsioDispatcher::StrandDispatcher>()) {}

MetadataDb::~MetadataDb() {
}

void MetadataDb::GetLatestSnapshot(
    const File& file, boost::shared_ptr<Snapshot>* snapshot,
    Callback callback) {
  strand_dispatcher_->Post(
      bind(&MetadataDb::GetLatestSnapshot,
           impl_.get(), boost::cref(file), snapshot, callback));
}

void MetadataDb::RecordNewSnapshot(
    boost::shared_ptr<Snapshot> snapshot, Callback callback) {
  strand_dispatcher_->Post(
      bind(&MetadataDb::RecordNewSnapshot,
           impl_.get(), snapshot, callback));
}

void MetadataDb::RecordNewBundle(
    boost::shared_ptr<AnnotatedBundleData> bundle, Callback callback) {
  strand_dispatcher_->Post(
      bind(&MetadataDb::RecordNewBundle,
           impl_.get(), bundle, callback));
}

void MetadataDb::RecordUploadedBundle(
    int server_id, boost::shared_ptr<AnnotatedBundleData> bundle,
    Callback callback) {
  strand_dispatcher_->Post(
      bind(&MetadataDb::RecordUploadedBundle,
           impl_.get(), server_id, bundle, callback));
}

}  // polar_express
