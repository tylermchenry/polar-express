#ifndef FILESYSTEM_SCANNER_H
#define FILESYSTEM_SCANNER_H

#include <vector>
#include <string>

#include "boost/filesystem.hpp"
#include "boost/function.hpp"
#include "boost/scoped_ptr.hpp"

#include "macros.h"

namespace polar_express {

class FilesystemScannerImpl;
  
// A class that recursively scans a filesystem hierarchy from a specified root
// directory, and repeatedly invokes a callback with groups of paths found under
// that root.
class FilesystemScanner {
 public:
  FilesystemScanner();
  virtual ~FilesystemScanner();

  typedef boost::function<void(const vector<filesystem::path>&)>
  FilePathsCallback;

  // Begins a scan of the filesystem hierarchy starting at root. Every time
  // callback_interval distinct paths are found, the callback is invoked with a
  // vector of those paths. This method blocks until the scan is complete.
  virtual void Scan(
      const string& root,
      FilePathsCallback callback,
      int callback_interval = 100) const;

 protected:
  explicit FilesystemScanner(bool create_impl);
  
 private:
  scoped_ptr<FilesystemScannerImpl> impl_;
  
  DISALLOW_COPY_AND_ASSIGN(FilesystemScanner);
};
  
}  // namespace polar_express

#endif  // FILESYSTEM_SCANNER_H
