#ifndef FILESYSTEM_SCANNER_H
#define FILESYSTEM_SCANNER_H

#include <vector>
#include <string>

#include <boost/scoped_ptr.hpp>
#include <boost/thread/shared_mutex.hpp>
#include "gtest/gtest_prod.h"

#include "macros.h"
#include "overrideable-scoped-ptr.h"
#include "thread-launcher.h"

namespace boost {
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
  virtual bool Scan(const string& root);

  // Stops the current scan.
  virtual void StopScan();
  
  // Returns true iff the scanner is currently in the process of scanning.
  virtual bool is_scanning() const;
  
  // Returns all filepaths retrieved so far during the current scan (or previous
  // scan if none is currently running).
  virtual void GetFilePaths(vector<string>* paths) const;

  // Returns all filepaths retrieved so far during the current scan (or previous
  // scan if none is currently running) and clears the list of retrieved file
  // paths. By calling GetAllFilePathsAndClear you can repeatedly retrieve lists
  // of file paths found since the last time you called it.
  virtual void GetFilePathsAndClear(vector<string>* paths);

  void SetThreadLauncherForTesting(ThreadLauncher* thread_launcher);
  
 private:
  // Called by the thread functor to add newly discovered paths.
  void AddFilePaths(const vector<string>& paths);

  // Called by the thread functor to indicate that it is finished.
  void ScanFinished();

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

  // All members below are guarded by the mutex.
  mutable shared_mutex mu_;
  bool is_scanning_;
  scoped_ptr<thread> scan_thread_;
  vector<string> paths_;

  FRIEND_TEST(FilesystemScannerTest, GetFilePaths);
  FRIEND_TEST(FilesystemScannerTest, GetFilePathsAndClear);

  DISALLOW_COPY_AND_ASSIGN(FilesystemScanner);
};
  
}  // namespace polar_express

#endif  // FILESYSTEM_SCANNER_H
