#ifndef SQLITE3_HELPERS_H
#define SQLITE3_HELPERS_H

#include <string>
#include <unordered_map>

#include "base/macros.h"

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
  int BindInt(const string& param_name, int value);
  int BindInt64(const string& param_name, int64_t value);
  int BindBool(const string& param_name, bool value);
  template <typename EnumT>
  int BindEnum(const string& param_name, const EnumT value);

  int Step();
  int StepUntilNotBusy();

  bool IsColumnNull(const string& col_name);

  string GetColumnText(const string& col_name);
  int GetColumnInt(const string& col_name);
  int64_t GetColumnInt64(const string& col_name);
  bool GetColumnBool(const string& col_name);
  template <typename EnumT> EnumT GetColumnEnum(const string& col_name);

  int Reset();

 private:
  void GenerateColumnIdxs();
  int GetColumnIdx(const string& col_name);

  sqlite3* db_;
  sqlite3_stmt* stmt_;

  unordered_map<string, int> column_idxs;

  DISALLOW_COPY_AND_ASSIGN(ScopedStatement);
};

template <typename EnumT>
int ScopedStatement::BindEnum(const string& param_name, const EnumT value) {
  return BindInt(param_name, static_cast<int>(value));
}

template <typename EnumT>
EnumT ScopedStatement::GetColumnEnum(const string& col_name) {
  return static_cast<EnumT>(GetColumnInt(col_name));
}

}  // namespace polar_express

#endif  // SQLITE3_HELPERS_H
