#ifndef SNAPSHOT_STATE_MACHINE_H
#define SNAPSHOT_STATE_MACHINE_H

#include <iostream>
#include <string>

#include "boost/filesystem.hpp"
#include "boost/msm/back/state_machine.hpp"
#include "boost/msm/front/state_machine_def.hpp"
#include "boost/shared_ptr.hpp"

#include "macros.h"
#include "overrideable-scoped-ptr.h"

namespace boost {
namespace asio {
class io_service;
}  // namespace asio
}  // namespace boost

namespace polar_express {

class CandidateSnapshotGenerator;
class Snapshot;

class SnapshotStateMachine
    : public msm::front::state_machine_def<SnapshotStateMachine>
{
 public: 
  typedef msm::back::state_machine<
   SnapshotStateMachine,
   boost::shared_ptr<asio::io_service> > BackEnd;

  typedef boost::function<void(BackEnd*)> DoneCallback;
  
  explicit SnapshotStateMachine(
      boost::shared_ptr<asio::io_service> io_service);
  virtual ~SnapshotStateMachine();

  // Kind of a hack. What is the idiomatic way to do this?
  // Takes ownership of the given pointer.
  void Initialize(BackEnd* back_end,
                  DoneCallback done_callback);
  
  // Events
  struct NewFilePathReady {
    string root_;
    filesystem::path filepath_;
  };
  struct CandidateSnapshotReady {
    boost::shared_ptr<Snapshot> candidate_snapshot_;
  };
  struct CleanUp {};
  
  // States
  struct WaitForNewFilePath : public msm::front::state<> {};
  struct WaitForCandidateSnapshot : public msm::front::state<> {};
  struct WaitForCleanUp : public msm::front::state<> {};
  struct Done : public msm::front::state<> {};
  typedef WaitForNewFilePath initial_state;
  
  // Transition actions
  virtual void RequestGenerateCandidateSnapshot(
      const NewFilePathReady& event);
  virtual void PrintCandidateSnapshot(
      const CandidateSnapshotReady& event);
  virtual void ExecuteDoneCallback(const CleanUp& event);
  
  // Transition table
  struct transition_table : mpl::vector<
    a_row <WaitForNewFilePath,
           NewFilePathReady,
           WaitForCandidateSnapshot,
           &SnapshotStateMachine::RequestGenerateCandidateSnapshot>,
    a_row <WaitForCandidateSnapshot,
           CandidateSnapshotReady,
           WaitForCleanUp,
           &SnapshotStateMachine::PrintCandidateSnapshot>,
    a_row <WaitForCleanUp,
           CleanUp,
           Done,
           &SnapshotStateMachine::ExecuteDoneCallback>
    > {};

  // Post this callback to an io_service this callback after enequing events.
  static void ExecuteEventsCallback(SnapshotStateMachine::BackEnd* back_end);

  template <class FSM,class Event>
  void no_transition(Event const& e, FSM&,int state)
  {
    std::cerr << "no transition from state " << state
              << " on event " << typeid(e).name() << std::endl;
  }

 private:
  auto_ptr<BackEnd> back_end_;
  DoneCallback done_callback_;

  boost::shared_ptr<asio::io_service> io_service_;

  OverrideableScopedPtr<CandidateSnapshotGenerator>
  candidate_snapshot_generator_;
  
  DISALLOW_COPY_AND_ASSIGN(SnapshotStateMachine);
};

}  // namespace polar_express

#endif  // SNAPSHOT_STATE_MACHINE_H
