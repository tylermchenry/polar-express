-- SQLite schema for local metadata of the backup system.

PRAGMA foreign_keys = ON;

-- A block is a series of bytes that is (or is to be) backed up.
create table blocks (
  'id'                  INTEGER PRIMARY KEY NOT NULL,
  'sha1_digest'         TEXT    NOT NULL,
  'length'              INTEGER NOT NULL
);
create index idx_blocks_sha1_digest_length on blocks('sha1_digest', 'length');

-- A file is a file at exists (or previously existed) on the local
-- filesystem.
create table files (
  'id'                  INTEGER PRIMARY KEY NOT NULL,
  'path'                TEXT    UNIQUE NOT NULL
);
create unique index idx_files_path on files('path');

-- Attributes are a set of basic POSIX attributes for a file (or
-- files). Having a table of attributes is purely a space optimization
-- since it is expected that many files will share the same set of
-- POSIX attributes. Note that owner/group names and uids/gids are
-- stored seperately. During a restore (potentially to a different
-- machine) the user may choose which one to use when recreating the
-- files.
create table attributes (
  'id'                  INTEGER PRIMARY KEY NOT NULL,
  'owner_user'          TEXT,
  'owner_group'         TEXT,
  'uid'                 INTEGER,
  'gid'                 INTEGER,
  'mode'                INTEGER
);
create unique index idx_attributes_all on
  attributes('owner_user', 'owner_group', 'uid', 'gid', 'mode');

-- A snapshot is a record of having observed a file on the local
-- filesystem at some particular time. This records the length,
-- sha1_digest, and attributes of that file at that time.
create table snapshots (
  'id'                  INTEGER PRIMARY KEY NOT NULL,
  'file_id'             INTEGER NOT NULL REFERENCES files('id')
                                  ON DELETE CASCADE,
  'attributes_id'       INTEGER NOT NULL REFERENCES attributes('id')
                                  ON DELETE CASCADE,
  'creation_time'       INTEGER,
  'modification_time'   INTEGER NOT NULL,
  'access_time'         INTEGER,
  'extra_attributes'    TEXT,
  'is_regular'          INTEGER NOT NULL DEFAULT true,
  'is_deleted'          INTEGER NOT NULL DEFAULT false,
  'sha1_digest'         TEXT    NOT NULL,
  'length'              INTEGER NOT NULL,
  'observation_time'    INTEGER NOT NULL
);
create index idx_snapshots_file_id on snapshots('file_id');
create index idx_snapshots_attributes_id on snapshots('attributes_id');
create index idx_snapshots_observation_time on snapshots('observation_time');

-- Maps files to blocks that they contain (or used to contain). The
-- observation time gives the time at which the file was observed to
-- contain the specified block at the specified offset.
create table files_to_blocks (
  'id'                  INTEGER PRIMARY KEY NOT NULL,
  'file_id'             INTEGER NOT NULL REFERENCES files('id')
                                  ON DELETE CASCADE,
  'block_id'            INTEGER NOT NULL REFERENCES blocks('id')
                                  ON DELETE CASCADE,
  'offset'              INTEGER NOT NULL,
  'observation_time'    INTEGER NOT NULL
);
create index idx_files_to_blocks_file_id on files_to_blocks('file_id');
create index idx_files_to_blocks_block_id on files_to_blocks('block_id');
create index idx_files_to_blocks_file_offset on
  files_to_blocks('file_id', 'offset');
create index idx_files_to_blocks_observation_time on
  files_to_blocks('observation_time');

-- ALL TABLES BELOW THIS POINT (prefixed with local_) ARE NOT BACKED
-- UP. They can be inferred and reconstructed during a full restore.

-- A bundle is a collection of blocks, plus a manifest, that has been
-- (or will be) uploaded to the server.
create table local_bundles (
  'id'                  INTEGER PRIMARY KEY NOT NULL,
  'sha1_digest'         TEXT    NOT NULL,
  'length'              INTEGER NOT NULL
);

-- This table holds information about the remote endpoint('s') being
-- backed up to, and how to connect to them.
create table local_servers (
  'id'                  INTEGER PRIMARY KEY NOT NULL,
  'name'                TEXT    NOT NULL,
  'connection_info'     TEXT
);

-- This table provides a quick lookup for which blocks corresponded to
-- a file (at which offsets) at the time that a particular snapshot
-- was taken. This will only hold data for the most recent snapshot ID
-- for each file; older data is routinely deleted. This mapping can be
-- reconstructed for older snapshots by joining the files,
-- files_to_blocks, and snapshots tables and analyzing offsets,
-- lengths, and observation timestamps.
create table local_snapshots_to_files_to_blocks (
  'snapshot_id'         INTEGER NOT NULL REFERENCES snapshots('id')
                                  ON DELETE CASCADE,
  'files_to_blocks_id'  INTEGER NOT NULL REFERENCES files_to_blocks('id')
                                  ON DELETE CASCADE
);
create index idx_local_snapshots_to_files_to_blocks_snapshot_id on
  local_snapshots_to_files_to_blocks('snapshot_id');
create index idx_local_snapshots_to_files_to_blocks_files_to_blocks_id on
  local_snapshots_to_files_to_blocks('files_to_blocks_id');

-- Records which blocks were included in which bundles.
create table local_blocks_to_bundles (
  'block_id'            INTEGER NOT NULL REFERENCES blocks('id')
                                  ON DELETE CASCADE,
  'bundle_id'           INTEGER NOT NULL REFERENCES bundles('id')
                                  ON DELETE CASCADE
);
create index idx_local_blocks_to_bundles_block_id on
  local_blocks_to_bundles('block_id');
create index idx_local_blocks_to_bundles_bundle_id on
  local_blocks_to_bundles('bundle_id');

-- Records which bundles were destined for which servers, what the
-- status of getting them there is, and when the status was last
-- updated.
create table local_bundles_to_servers (
  'bundle_id'         INTEGER NOT NULL REFERENCES bundles('id')
                                ON DELETE CASCADE,
  'server_id'         INTEGER NOT NULL REFERENCES servers('id')
                                ON DELETE CASCADE,
  'status'            INTEGER NOT NULL,
  'status_timestamp'  INTEGER NOT NULL
);
create index idx_local_bundles_to_servers_bundle_id on
  local_bundles_to_servers('bundle_id');
create index idx_local_bundles_to_servers_server_id on
  local_bundles_to_servers('server_id');

-- Records which rows of the local_attributes table were backed up in
-- which bundles' manifests.
create table local_attributes_to_bundle_manifest (
  'attributes_id'       INTEGER NOT NULL REFERENCES attributes('id')
                                ON DELETE CASCADE,
  'bundle_id'           INTEGER NOT NULL REFERENCES bundles('id')
                                ON DELETE CASCADE
);
create index idx_local_attributes_to_bundle_manifest_attributes_id on
  local_attributes_to_bundle_manifest('attributes_id');
create index idx_local_attributes_to_bundle_manifest_bundle_id on
  local_attributes_to_bundle_manifest('bundle_id');

-- Records which rows of the snapshots table were backed up in which
-- bundles' manifests.
create table local_snapshots_to_bundle_manifest (
  'snapshot_id'         INTEGER NOT NULL REFERENCES snapshots('id')
                                  ON DELETE CASCADE,
  'bundle_id'           INTEGER NOT NULL REFERENCES bundles('id')
                                  ON DELETE CASCADE
);
create index idx_local_snapshots_to_bundle_manifest_snapshot_id on
  local_snapshots_to_bundle_manifest('snapshot_id');
create index idx_local_snapshots_to_bundle_manifest_bundle_id on
  local_snapshots_to_bundle_manifest('bundle_id');

-- Records which rows of the files_to_blocks table were backed up in
-- which bundles' manifests.
create table local_files_to_blocks_to_bundle_manifest (
  'files_to_blocks_id'  INTEGER NOT NULL REFERENCES files_to_blocks('id')
                                  ON DELETE CASCADE,
  'bundle_id'           INTEGER NOT NULL REFERENCES bundles('id')
                                  ON DELETE CASCADE
);
create index idx_local_files_to_blocks_to_bundle_manifest_files_to_blocks_id on
  local_files_to_blocks_to_bundle_manifest('files_to_blocks_id');
create index idx_local_files_to_blocks_to_bundle_manifest_bundle_id on
  local_files_to_blocks_to_bundle_manifest('bundle_id');