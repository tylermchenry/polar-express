#include <string>
#include <vector>

#include "boost/bind.hpp"
#include "boost/filesystem.hpp"
#include "boost/shared_ptr.hpp"

#include "asio-dispatcher.h"
#include "filesystem-scanner.h"
#include "macros.h"
#include "snapshot-state-machine.h"

using namespace polar_express;

void DeleteSnapshotStateMachine(
    boost::shared_ptr<SnapshotStateMachine> snapshot_state_machine) {
  snapshot_state_machine.reset();
}

void PostDeleteSnapshotStateMachine(
    boost::shared_ptr<SnapshotStateMachine> snapshot_state_machine) {
  AsioDispatcher::GetInstance()->PostStateMachine(
      bind(&DeleteSnapshotStateMachine, snapshot_state_machine));
}

void StartSnapshotStateMachines(
    const string& root,
    const vector<filesystem::path>& paths) {
  for (const auto& path : paths) {
    boost::shared_ptr<SnapshotStateMachine> snapshot_state_machine(
        new SnapshotStateMachine);
    snapshot_state_machine->SetDoneCallback(
        bind(&PostDeleteSnapshotStateMachine, snapshot_state_machine));
    snapshot_state_machine->Start(root, path);
  }
}

int main(int argc, char** argv) {
  if (argc > 1) {
    string root = argv[1];
    FilesystemScanner fs_scanner;
    AsioDispatcher::GetInstance()->Start();
    fs_scanner.Scan(root, bind(StartSnapshotStateMachines, root, _1));
    AsioDispatcher::GetInstance()->WaitForFinish();
  }
  return 0;
}
