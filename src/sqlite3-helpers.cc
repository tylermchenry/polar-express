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

int ScopedStatement::BindInt(const string& param_name, int value) {
  return sqlite3_bind_int(
      stmt_, sqlite3_bind_parameter_index(stmt_, param_name.c_str()), value);
}
  
int ScopedStatement::BindInt64(const string& param_name, int64_t value) {
  return sqlite3_bind_int64(
      stmt_, sqlite3_bind_parameter_index(stmt_, param_name.c_str()), value);
}

int ScopedStatement::BindBool(const string& param_name, bool value) {
  return sqlite3_bind_int(
      stmt_, sqlite3_bind_parameter_index(stmt_, param_name.c_str()),
      (value ? 1 : 0));
}

int ScopedStatement::Step() {
  return sqlite3_step(stmt_);
}

int ScopedStatement::StepUntilNotBusy() {
  int code;
  do {
    code = Step();
  } while (code == SQLITE_BUSY);
  return code;
}

string ScopedStatement::GetColumnText(const string& col_name) {
  const char* value_cstr = reinterpret_cast<const char*>(
      sqlite3_column_text(stmt_, GetColumnIdx(col_name)));
  return (value_cstr != nullptr) ? string(value_cstr) : "";
}

int64_t ScopedStatement::GetColumnInt64(const string& col_name) {
  return sqlite3_column_int64(stmt_, GetColumnIdx(col_name));
}

bool ScopedStatement::GetColumnBool(const string& col_name) {
  return (sqlite3_column_int(stmt_, GetColumnIdx(col_name)) == 0);
}

int ScopedStatement::Reset() {
  return sqlite3_reset(stmt_);
}

void ScopedStatement::GenerateColumnIdxs() {
  for (int i = 0; i < sqlite3_column_count(stmt_); ++i) {
    column_idxs.insert(make_pair(sqlite3_column_name(stmt_, i), i));
  }
}

int ScopedStatement::GetColumnIdx(const string& col_name) {
  if (column_idxs.empty()) {
    GenerateColumnIdxs();
  }
  auto itr = column_idxs.find(col_name);
  if (itr != column_idxs.end()) {
    return itr->second;
  } else {
    return -1;
  }
}

}  // namespace polar_express
