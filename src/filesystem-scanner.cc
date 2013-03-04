#include "filesystem-scanner.h"

#include <utility>

#include "boost/foreach.hpp"
#include "boost/filesystem.hpp"
#include "boost/thread/thread.hpp"

namespace polar_express {
namespace {

string PathWithoutPrefix(const filesystem::path& path, const string& prefix) {
  string full_path = path.string();
  if (full_path.find(prefix) == 0) {
    return full_path.substr(prefix.length(),
                            full_path.length() - prefix.length());
  }
  return full_path;
}

}  // namespace

FilesystemScanner::FilesystemScanner() {
}

FilesystemScanner::~FilesystemScanner() {
}

void FilesystemScanner::Scan(
    const string& root,
    FilePathsCallback callback,
    int callback_interval) {
  vector<string> paths;
  filesystem::recursive_directory_iterator itr(root);
  filesystem::recursive_directory_iterator eod;
  BOOST_FOREACH(const filesystem::path& path, make_pair(itr, eod)) {
    paths.push_back(PathWithoutPrefix(path, root));
    if (paths.size() >= callback_interval) {
      this_thread::interruption_point();
      callback(paths);
      paths.clear();
    }
  }

  if (!paths.empty()) {
    callback(paths);
  }
}
  
}  // namespace polar_express
