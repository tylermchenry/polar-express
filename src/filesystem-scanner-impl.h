#ifndef FILESYSTEM_SCANNER_IMPL_H
#define FILESYSTEM_SCANNER_IMPL_H

#include <vector>
#include <string>

#include "boost/filesystem.hpp"
#include "boost/function.hpp"

#include "macros.h"
#include "filesystem-scanner.h"

namespace polar_express {

class FilesystemScannerImpl : public FilesystemScanner {
 public:
  FilesystemScannerImpl();
  virtual ~FilesystemScannerImpl();

  using FilesystemScanner::FilePathsCallback;
  
  virtual void Scan(
      const string& root,
      FilePathsCallback callback,
      int callback_interval = 100) const;
  
 private:
  DISALLOW_COPY_AND_ASSIGN(FilesystemScannerImpl);
};
  
}  // namespace polar_express

#endif  // FILESYSTEM_SCANNER_IMPL_H
