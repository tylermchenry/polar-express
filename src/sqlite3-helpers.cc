#include "sqlite3-helpers.h"

#include "sqlite3.h"

namespace polar_express {

ScopedStatement::ScopedStatement(sqlite3* db)
    : db_(CHECK_NOTNULL(db)),
      stmt_(nullptr) {
}

ScopedStatement::~ScopedStatement() {
  if (stmt_) {
    sqlite3_finalize(stmt_);
  }
}

int ScopedStatement::Prepare(const string& query) {
  return sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt_, nullptr);
}

int ScopedStatement::BindText(const string& param_name, const string& value) {
  return sqlite3_bind_text(
      stmt_, sqlite3_bind_parameter_index(stmt_, param_name.c_str()),
      value.c_str(), -1, SQLITE_TRANSIENT);
}

int ScopedStatement::BindInt64(const string& param_name, int64_t value) {
  return sqlite3_bind_int64(
      stmt_, sqlite3_bind_parameter_index(stmt_, param_name.c_str()), value);
}

int ScopedStatement::Step() {
  return sqlite3_step(stmt_);
}

int ScopedStatement::Reset() {
  return sqlite3_reset(stmt_);
}

}  // namespace polar_express
