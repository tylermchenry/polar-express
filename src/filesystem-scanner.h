#ifndef FILESYSTEM_SCANNER_H
#define FILESYSTEM_SCANNER_H

#include <vector>
#include <string>

#include <boost/scoped_ptr.hpp>
#include <boost/thread/shared_mutex.hpp>

#include "macros.h"

namespace boost {
class thread;
}  // namespace boost

namespace polar_express {

// Traverses a filesystem rooted at a particular directory and appends each
// regular file to a queue.
class FilesystemScanner {
 public:
  FilesystemScanner();
  virtual ~FilesystemScanner();
  
  virtual bool Scan(const string& root);

  virtual bool is_scanning() const;

  virtual void GetFilePaths(vector<string>* paths) const;

  virtual void GetFilePathsAndClear(vector<string>* paths);

 private:
  bool StartScan();

  void AddFilePaths(const vector<string>& paths);

  void StopScan();
  
  class ScannerThread {
   public:
    ScannerThread(FilesystemScanner* fs_scanner, const string& root);
    void operator()();

   private:
    FilesystemScanner* fs_scanner_;
    string root_;
  };
  
  mutable shared_mutex mu_;
  bool is_scanning_;
  scoped_ptr<thread> scan_thread_;
  vector<string> paths_;

  DISALLOW_COPY_AND_ASSIGN(FilesystemScanner);
};
  
}  // namespace polar_express

#endif  // FILESYSTEM_SCANNER_H
