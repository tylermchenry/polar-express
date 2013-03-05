#include "snapshot-state-machine.h"

#include <iostream>

namespace polar_express {

SnapshotStateMachine::SnapshotStateMachine() {
}

SnapshotStateMachine::~SnapshotStateMachine() {
}

void SnapshotStateMachine::RequestGenerateCandidateSnapshot(
    const NewFilePathReady& event) {
}

void SnapshotStateMachine::PrintCandidateSnapshot(
    const CandidateSnapshotReady& event) {
}


}  // namespace polar_express
