#ifndef FILESYSTEM_SCANNER_H
#define FILESYSTEM_SCANNER_H

#include <vector>
#include <string>

#include "boost/filesystem.hpp"
#include "boost/function.hpp"
#include "boost/scoped_ptr.hpp"

#include "callback.h"
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

  // Begins a scan of the filesystem hierarchy starting at root, invoking the
  // given callback every time one is found.
  virtual void Scan(const string& root, Callback callback);

  virtual bool GetNextPath(boost::filesystem::path* path);
  
 protected:
  explicit FilesystemScanner(bool create_impl);
  
 private:
  scoped_ptr<FilesystemScannerImpl> impl_;
  
  DISALLOW_COPY_AND_ASSIGN(FilesystemScanner);
};
  
}  // namespace polar_express

#endif  // FILESYSTEM_SCANNER_H
