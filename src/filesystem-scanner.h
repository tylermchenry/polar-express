#ifndef FILESYSTEM_SCANNER_H
#define FILESYSTEM_SCANNER_H

#include <vector>
#include <string>

#include "boost/scoped_ptr.hpp"
#include "boost/thread/shared_mutex.hpp"
#include "gtest/gtest_prod.h"

#include "macros.h"
#include "overrideable-scoped-ptr.h"
#include "thread-launcher.h"

namespace boost {
class condition_variable;
class thread;
}  // namespace boost

namespace polar_express {

// Asynchronously traverses a filesystem rooted at a particular directory and
// appends each regular file to a queue.
class FilesystemScanner {
 public:
  FilesystemScanner();
  virtual ~FilesystemScanner();

  // Begins an asynchronous scan rooted at the given root path. Returns true if
  // the was started successful, false otherwise. You cannot start a new scan
  // while a scan is already running. Starting a new scan clears the list of
  // filepaths found.
  virtual bool Scan(const string& root) LOCKS_EXCLUDED(mu_);

  // Version of Scan that simultaneously registers a new condition variable to
  // be notified when results are available (see NotifyOnNewFilePaths below).
  virtual bool Scan(
      const string& root,
      condition_variable* condition) LOCKS_EXCLUDED(mu_);
  
  // Stops the current scan.
  virtual void StopScan() LOCKS_EXCLUDED(mu_);
  
  // Returns true iff the scanner is currently in the process of scanning.
  virtual bool is_scanning() const LOCKS_EXCLUDED(mu_);
  
  // Returns all filepaths retrieved so far during the current scan (or previous
  // scan if none is currently running).
  virtual void GetFilePaths(vector<string>* paths) const LOCKS_EXCLUDED(mu_);

  // Returns all filepaths retrieved so far during the current scan (or previous
  // scan if none is currently running) and clears the list of retrieved file
  // paths. By calling GetAllFilePathsAndClear you can repeatedly retrieve lists
  // of file paths found since the last time you called it.
  virtual void GetFilePathsAndClear(vector<string>* paths) LOCKS_EXCLUDED(mu_);

  // Registers a condition variable to be notified when there is a new path or
  // set of paths available to retrieve, or when the scan finishes and there are
  // no more paths to retrieve.
  //
  // This condition is notified only once per call to NotifyOnNewFilePaths, so
  // clients wishing to receive another notification must call this method
  // again. Note that the condition variable will be notified immediately if
  // there are currently paths available.
  //
  // Does not assume owenership of the condition variable. The condition
  // variable must not be null and must not be destroyed before either it is
  // notified.
  virtual void NotifyOnNewFilePaths(
      condition_variable* condition) LOCKS_EXCLUDED(mu_);
  
  void SetThreadLauncherForTesting(ThreadLauncher* thread_launcher);
  
 private:
  // Called by the thread functor to add newly discovered paths.
  void AddFilePaths(const vector<string>& paths) LOCKS_EXCLUDED(mu_);

  // Called by the thread functor to indicate that it is finished.
  void ScanFinished() LOCKS_EXCLUDED(mu_);

  // Notifies all conditions registered for the "new file paths" (or end of
  // scan) event, then removes them from the conditions set.
  void NotifyForNewFilePaths() EXCLUSIVE_LOCKS_REQUIRED(mu_);
  
  // A functor class that recursively traverses the filesystem from the given
  // root path and adds groups of discovered paths to the given
  // FilesystemScanner object.
  class ScannerThread {
   public:
    ScannerThread(FilesystemScanner* fs_scanner, const string& root);
    void operator()();

   private:
    FilesystemScanner* fs_scanner_;
    string root_;
  };

  OverrideableScopedPtr<ThreadLauncher> thread_launcher_;

  mutable shared_mutex mu_;
  bool is_scanning_ GUARDED_BY(mu_);
  scoped_ptr<thread> scan_thread_ GUARDED_BY(mu_);
  vector<string> paths_ GUARDED_BY(mu_);
  vector<condition_variable*> new_paths_conditions_ GUARDED_BY(mu_);

  FRIEND_TEST(FilesystemScannerTest, GetFilePaths);
  FRIEND_TEST(FilesystemScannerTest, GetFilePathsAndClear);

  DISALLOW_COPY_AND_ASSIGN(FilesystemScanner);
};
  
}  // namespace polar_express

#endif  // FILESYSTEM_SCANNER_H
