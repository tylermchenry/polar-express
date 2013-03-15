#ifndef SQLITE3_HELPERS_H
#define SQLITE3_HELPERS_H

#include <string>

#include "macros.h"

class sqlite3;
class sqlite3_stmt;

namespace polar_express {

class ScopedStatement {
 public:
  explicit ScopedStatement(sqlite3* db);
  ~ScopedStatement();

  // TODO: Add wrappers around sqlite3_* functions as needed.
  int Prepare(const string& query);

  int BindText(const string& param_name, const string& value);
  int BindInt64(const string& param_name, int64_t value);

  int Step();

  int Reset();
  
 private:
  sqlite3* db_;
  sqlite3_stmt* stmt_;
  
  DISALLOW_COPY_AND_ASSIGN(ScopedStatement);
};

}  // namespace polar_express

#endif  // SQLITE3_HELPERS_H
