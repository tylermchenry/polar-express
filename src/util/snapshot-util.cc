#include "util/snapshot-util.h"

#include "proto/snapshot.pb.h"

namespace polar_express {

SnapshotUtil::SnapshotUtil() {
}

SnapshotUtil::~SnapshotUtil() {
}

bool SnapshotUtil::FileContentsEqual(
      const Snapshot& lhs, const Snapshot& rhs) const {
  return lhs.creation_time() == rhs.creation_time() &&
      lhs.modification_time() == rhs.modification_time() &&
      lhs.is_regular() == rhs.is_regular() &&
      lhs.is_deleted() == rhs.is_deleted() &&
      lhs.length() == rhs.length() &&
      (!lhs.has_sha1_digest() || !rhs.has_sha1_digest() ||
       (lhs.sha1_digest() == rhs.sha1_digest()));
}

bool SnapshotUtil::AllMetadataEqual(
    const Snapshot& lhs, const Snapshot& rhs) const {
  // TODO: Generic reflection-based protobuf equality.
  // TODO: Compare extra attributes (using this reflection-based equality).
  return lhs.attributes().owner_user() == rhs.attributes().owner_user() &&
      lhs.attributes().owner_group() == rhs.attributes().owner_group() &&
      lhs.attributes().uid() == rhs.attributes().uid() &&
      lhs.attributes().gid() == rhs.attributes().gid() &&
      lhs.attributes().mode() == rhs.attributes().mode() &&
      lhs.access_time() == rhs.access_time() &&
      FileContentsEqual(lhs, rhs);
}

}  // namespace polar_express
