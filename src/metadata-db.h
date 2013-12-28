#ifndef METADATA_DB_H
#define METADATA_DB_H

#include <memory>

#include "boost/shared_ptr.hpp"

#include "asio-dispatcher.h"
#include "callback.h"
#include "macros.h"

namespace polar_express {

class AnnotatedBundleData;
class File;
class MetadataDbImpl;
class Snapshot;

class MetadataDb {
 public:
  MetadataDb();
  virtual ~MetadataDb();

  virtual void GetLatestSnapshot(
      const File& file, boost::shared_ptr<Snapshot>* snapshot,
      Callback callback);

  // This also modifies the snapshot to add IDs for the snapshot itself as well
  // as any blocks that do not already have IDs.
  virtual void RecordNewSnapshot(
      boost::shared_ptr<Snapshot> snapshot, Callback callback);

  // This also modifies the bundle to add an ID for the bundle itself.
  virtual void RecordNewBundle(
      boost::shared_ptr<AnnotatedBundleData> bundle, Callback callback);

  // Creates a local-only mapping from (bundle_id, server_id) to the server-side
  // bundle ID with status and timestamp.
  //
  // TODO: There should be some kind of server object, instead of taking server
  // ID directly.
  virtual void RecordUploadedBundle(
      int server_id, boost::shared_ptr<AnnotatedBundleData> bundle,
      Callback callback);

 protected:
  explicit MetadataDb(bool create_impl);

 private:
  unique_ptr<MetadataDbImpl> impl_;
  boost::shared_ptr<AsioDispatcher::StrandDispatcher> strand_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(MetadataDb);
};

}  // namespace polar_express

#endif  // METADATA_DB_H
