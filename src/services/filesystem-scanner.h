#ifndef FILESYSTEM_SCANNER_H
#define FILESYSTEM_SCANNER_H

#include <memory>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/function.hpp>

#include "base/callback.h"
#include "base/macros.h"

namespace polar_express {

class FilesystemScannerImpl;

// A class that asynchronously performs a recursive scan of a filesystem
// hierarchy from a specified root directory and collects all of the file paths
// found below the root.
//
// The scan is performed in sections, with each section collecting at most a
// certain number of file paths. When a section completes, the caller is
// expected to take some action with the paths discovered, and then ask the scan
// to continue.
//
// This class is the asynchronous stub. Calls to its asynchronous methods post a
// task which will invoke the equivalent method on its implementation
// class. Calls to other methods are forwarded directly to the implementation.
class FilesystemScanner {
 public:
  FilesystemScanner();
  virtual ~FilesystemScanner();

  // Asynchronously begins a new scan starting at root, which will collect at
  // most max_paths paths, and will invoke callback when done. Clears any
  // existing paths.
  virtual void StartScan(
      const string& root, int max_paths, Callback callback);

  // Asynchronously continues the current scan, collecting at most max_paths
  // additional paths, and will invoke callback when done.
  virtual void ContinueScan(int max_paths, Callback callback);

  // Returns all paths obtained since the last call to StartScan or ClearPaths.
  virtual bool GetPaths(vector<boost::filesystem::path>* paths) const;

  // Returns all paths obtained since the last call to StartScan or ClearPaths,
  // along with their size.
  virtual bool GetPathsWithFilesize(
      vector<pair<boost::filesystem::path, size_t> >* paths_with_size) const;

  // Clears all existing discovered paths.
  virtual void ClearPaths();

 protected:
  explicit FilesystemScanner(bool create_impl);

 private:
  unique_ptr<FilesystemScannerImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(FilesystemScanner);
};

}  // namespace polar_express

#endif  // FILESYSTEM_SCANNER_H
