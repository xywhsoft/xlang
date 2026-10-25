#ifndef XLANG_SQLITE_NATIVE_H
#define XLANG_SQLITE_NATIVE_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* SQLite 薄绑定的物理 ABI。句柄必须来自同一份 SQLite 库，释放后不得复用。 */
/* 文件名和 SQL 的 NULL 指针无效；空字符串仍遵循 SQLite 自身语义。 */
int xsqlite3_bind_blob_native(void* stmt, int index, void* data, size_t size);
int xsqlite3_bind_text_bytes_native(void* stmt, int index, void* data, size_t size);
void* xsqlite3_column_blob_native(void* stmt, int column);
void* xsqlite3_column_text_bytes_native(void* stmt, int column);
const char* xsqlite3_stmt_errmsg_native(void* stmt);
char* xsqlite3_stmt_errmsg_owned_native(void* stmt);
void* xsqlite3_stmt_errmsg_point_native(void* stmt);
const char* xsqlite3_last_error(void);
const char* xsqlite_last_error_native(void);
int xsqlite_available_native(void);
void* xsqlite3_open_native(const char* filename);
void* xsqlite3_open_v2_native(const char* filename, int flags, const char* vfs);
int xsqlite3_exec_native(void* db_handle, const char* sql);
void* xsqlite3_prepare_v2_native(void* db_handle, const char* sql);
int xsqlite3_bind_text_native(void* stmt_handle, int index, const char* value);
int xsqlite3_bind_blob_text_native(void* stmt_handle, int index, const char* value);
const char* xsqlite3_column_text_native(void* stmt_handle, int column);
char* xsqlite3_column_text_owned_native(void* stmt_handle, int column);
void* xsqlite3_column_text_point_native(void* stmt_handle, int column);
const char* xsqlite3_column_name_native(void* stmt_handle, int column);
char* xsqlite3_column_name_owned_native(void* stmt_handle, int column);
void* xsqlite3_column_name_point_native(void* stmt_handle, int column);
const char* xsqlite3_last_insert_rowid_text_native(void* db_handle);
char* xsqlite3_last_insert_rowid_text_owned_native(void* db_handle);
void* xsqlite3_last_insert_rowid_text_point_native(void* db_handle);
const char* xsqlite3_errstr_native(int rc);
const char* xsqlite3_libversion_native(void);
const char* xsqlite_version_native(void);
int xsqlite3_libversion_number_native(void);
int xsqlite3_close_native(void* db);
int xsqlite3_close_v2_native(void* db);
const char* xsqlite3_errmsg_native(void* db);
char* xsqlite3_errmsg_owned_native(void* db);
void* xsqlite3_errmsg_point_native(void* db);
int xsqlite3_extended_errcode_native(void* db);
int64_t xsqlite3_changes_native(void* db);
int64_t xsqlite3_total_changes_native(void* db);
int xsqlite3_step_native(void* stmt);
int xsqlite3_finalize_native(void* stmt);
int xsqlite3_reset_native(void* stmt);
int xsqlite3_clear_bindings_native(void* stmt);
int xsqlite3_bind_int_native(void* stmt, int index, int value);
int xsqlite3_bind_int64_native(void* stmt, int index, int64_t value);
int xsqlite3_bind_double_native(void* stmt, int index, double value);
int xsqlite3_bind_null_native(void* stmt, int index);
int xsqlite3_bind_parameter_count_native(void* stmt);
int xsqlite3_column_count_native(void* stmt);
int xsqlite3_column_type_native(void* stmt, int column);
int xsqlite3_column_int_native(void* stmt, int column);
int64_t xsqlite3_column_int64_native(void* stmt, int column);
int64_t xsqlite3_last_insert_rowid_native(void* db);
int xsqlite3_busy_timeout_native(void* db, int milliseconds);
double xsqlite3_column_double_native(void* stmt, int column);
int xsqlite3_column_bytes_native(void* stmt, int column);


#ifdef __cplusplus
}
#endif
#endif
