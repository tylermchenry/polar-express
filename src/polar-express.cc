#include <string>
#include <vector>

#include "boost/bind.hpp"
#include "boost/filesystem.hpp"

#include "asio-dispatcher.h"
#include "filesystem-scanner.h"
#include "macros.h"
#include "snapshot-state-machine.h"

using namespace polar_express;

void DeleteSnapshotStateMachine(
    SnapshotStateMachine* snapshot_state_machine) {
  delete snapshot_state_machine;
}

void StartSnapshotStateMachines(
    const string& root,
    const vector<filesystem::path>& paths) {
  for (const auto& path : paths) {
    (new SnapshotStateMachine(&DeleteSnapshotStateMachine))->Start(root, path);
  }
}

int main(int argc, char** argv) {
  if (argc > 1) {
    string root = argv[1];
    FilesystemScanner fs_scanner;
    AsioDispatcher::GetInstance()->Start();
    fs_scanner.Scan(root, bind(StartSnapshotStateMachines, root, _1));
    // WRONG: This stops the non-disk-bound ASIO services immediately. FIXME.
    AsioDispatcher::GetInstance()->Finish();
  }
  return 0;
}
