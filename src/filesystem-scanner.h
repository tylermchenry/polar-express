#ifndef FILESYSTEM_SCANNER_H
#define FILESYSTEM_SCANNER_H

#include <vector>
#include <string>

#include "boost/function.hpp"

#include "macros.h"

namespace polar_express {

class FilesystemScanner {
 public:
  FilesystemScanner();
  virtual ~FilesystemScanner();

  typedef boost::function<void(const vector<string>&)> FilePathsCallback;
  
  virtual void Scan(
      const string& root,
      FilePathsCallback callback,
      int callback_interval = 100);
  
 private:
  DISALLOW_COPY_AND_ASSIGN(FilesystemScanner);
};
  
}  // namespace polar_express

#endif  // FILESYSTEM_SCANNER_H
