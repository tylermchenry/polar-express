#ifndef FILESYSTEM_SCANNER_IMPL_H
#define FILESYSTEM_SCANNER_IMPL_H

#include <queue>
#include <string>

#include "boost/filesystem.hpp"

#include "callback.h"
#include "macros.h"
#include "filesystem-scanner.h"

namespace polar_express {

// This class is the synchronous implementation of the asynchronous stub class
// FilesystemScanner. Do not use it directly; use the stub instead. See the stub
// class header for documentation on the behavior of the public methods.
class FilesystemScannerImpl : public FilesystemScanner {
 public:
  FilesystemScannerImpl();
  virtual ~FilesystemScannerImpl();

  virtual void StartScan(
      const string& root, int max_paths, Callback callback);

  virtual void ContinueScan(int max_paths, Callback callback);

  virtual bool GetPaths(vector<boost::filesystem::path>* paths) const;

  virtual void ClearPaths();

 private:
  void AddPath(const boost::filesystem::path& path);

  filesystem::recursive_directory_iterator itr_;
  vector<boost::filesystem::path> paths_;

  DISALLOW_COPY_AND_ASSIGN(FilesystemScannerImpl);
};

}  // namespace polar_express

#endif  // FILESYSTEM_SCANNER_IMPL_H
