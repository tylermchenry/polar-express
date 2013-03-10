#ifndef FILESYSTEM_SCANNER_IMPL_H
#define FILESYSTEM_SCANNER_IMPL_H

#include <queue>
#include <string>

#include "boost/filesystem.hpp"
#include "boost/thread/mutex.hpp"

#include "callback.h"
#include "macros.h"
#include "filesystem-scanner.h"

namespace polar_express {

class FilesystemScannerImpl : public FilesystemScanner {
 public:
  FilesystemScannerImpl();
  virtual ~FilesystemScannerImpl();

  virtual void Scan(const string& root, Callback callback);

  virtual bool GetNextPath(boost::filesystem::path* path);
  
 private:
  void AddPath(const boost::filesystem::path& path);
  
  mutable boost::mutex mu_;
  queue<boost::filesystem::path> paths_ GUARDED_BY(mu_);
  
  DISALLOW_COPY_AND_ASSIGN(FilesystemScannerImpl);
};
  
}  // namespace polar_express

#endif  // FILESYSTEM_SCANNER_IMPL_H
