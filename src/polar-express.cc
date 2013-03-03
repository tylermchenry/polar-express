#include <iostream>
#include <string>
#include <vector>

#include "boost/thread/condition_variable.hpp"
#include "boost/thread/mutex.hpp"

#include "filesystem-scanner.h"
#include "macros.h"

using namespace polar_express;

namespace {

void PrintPathsAndClear(FilesystemScanner* scanner) {
  vector<string> paths;
  CHECK_NOTNULL(scanner)->GetFilePathsAndClear(&paths);
  for (const auto& path : paths) {
    cout << path << endl;
  }
}

}  // namespace

int main(int argc, char** argv) {
  mutex new_data_mutex;
  condition_variable new_data_condition;
  FilesystemScanner scanner;
  if (argc > 1) {
    if (scanner.Scan(argv[1], &new_data_condition)) {
      while (scanner.is_scanning()) {
        unique_lock<mutex> new_data_lock(new_data_mutex);
        new_data_condition.wait(new_data_lock);
        PrintPathsAndClear(&scanner);
        scanner.NotifyOnNewFilePaths(&new_data_condition);
      } 
      PrintPathsAndClear(&scanner);
    }
  }
  
  return 0;
}
