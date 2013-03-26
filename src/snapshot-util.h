#ifndef SNAPSHOT_UTIL_H
#define SNAPSHOT_UTIL_H

#include "macros.h"

namespace polar_express {

class Snapshot;

class SnapshotUtil {
 public:
  SnapshotUtil();
  virtual ~SnapshotUtil();

  // Assumes snapshots refer to the same file. Only taks SHA1 digests into
  // account if they exist in both snapshots.
  // TODO: Paranoia levels.
  virtual bool FileContentsEqual(
      const Snapshot& lhs, const Snapshot& rhs) const;

  // Assumes snapshots refer to the same file. Checks that metadata is equal
  // between the two snapshots. This compares all fields except ID fields and
  // observation time. Only compares SHA1 digests if they exist in both
  // snapshots. This is a superset of the equality checks in FileContentsEqual.
  virtual bool AllMetadataEqual(
      const Snapshot& lhs, const Snapshot& rhs) const;
  
 private:
  DISALLOW_COPY_AND_ASSIGN(SnapshotUtil);
};

}  // namespace polar_express

#endif  // SNAPSHOT_UTIL_H
