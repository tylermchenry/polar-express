#ifndef FILESYSTEM_SCANNER_H
#define FILESYSTEM_SCANNER_H

#include <vector>
#include <string>

#include <boost/thread/shared_mutex.hpp>

#include "macros.h"

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
  
  mutable shared_mutex mu_;
  bool is_scanning_;
  vector<string> paths_;

  DISALLOW_COPY_AND_ASSIGN(FilesystemScanner);
};
  
}  // namespace polar_express

#endif  // FILESYSTEM_SCANNER_H
