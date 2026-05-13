#include "sqlite3.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XSQLITE_EXPORT __declspec(dllexport)
#else
#define XSQLITE_EXPORT
#endif

#if defined(__GNUC__)
#define XSQLITE_THREAD __thread
#elif defined(_MSC_VER)
#define XSQLITE_THREAD __declspec(thread)
#else
#define XSQLITE_THREAD
#endif

static XSQLITE_THREAD char g_xsqlite_error[1024];
static XSQLITE_THREAD char g_xsqlite_int64[64];

static void xsqlite_set_error(const char* text)
{
	if ( text == NULL ) text = "";
	strncpy(g_xsqlite_error, text, sizeof(g_xsqlite_error) - 1u);
	g_xsqlite_error[sizeof(g_xsqlite_error) - 1u] = '\0';
}

static void xsqlite_set_db_error(sqlite3* db)
{
	if ( db == NULL ) {
		xsqlite_set_error("null sqlite3 handle");
		return;
	}
	xsqlite_set_error(sqlite3_errmsg(db));
}

XSQLITE_EXPORT const char* xsqlite3_last_error(void)
{
	return g_xsqlite_error;
}

XSQLITE_EXPORT void* xsqlite3_open_native(const char* filename)
{
	sqlite3* db = NULL;
	int rc = sqlite3_open(filename == NULL ? "" : filename, &db);
	if ( rc != SQLITE_OK ) {
		xsqlite_set_db_error(db);
		if ( db != NULL ) sqlite3_close(db);
		return NULL;
	}
	xsqlite_set_error("");
	return db;
}

XSQLITE_EXPORT void* xsqlite3_open_v2_native(const char* filename, int flags, const char* vfs)
{
	sqlite3* db = NULL;
	int rc = sqlite3_open_v2(filename == NULL ? "" : filename, &db, flags, vfs);
	if ( rc != SQLITE_OK ) {
		xsqlite_set_db_error(db);
		if ( db != NULL ) sqlite3_close(db);
		return NULL;
	}
	xsqlite_set_error("");
	return db;
}

XSQLITE_EXPORT int xsqlite3_exec_native(void* db_handle, const char* sql)
{
	sqlite3* db = (sqlite3*)db_handle;
	char* errmsg = NULL;
	int rc;
	if ( db == NULL ) {
		xsqlite_set_error("null sqlite3 handle");
		return SQLITE_MISUSE;
	}
	rc = sqlite3_exec(db, sql == NULL ? "" : sql, NULL, NULL, &errmsg);
	if ( rc != SQLITE_OK ) {
		xsqlite_set_error(errmsg != NULL ? errmsg : sqlite3_errmsg(db));
		sqlite3_free(errmsg);
		return rc;
	}
	xsqlite_set_error("");
	return rc;
}

XSQLITE_EXPORT void* xsqlite3_prepare_v2_native(void* db_handle, const char* sql)
{
	sqlite3* db = (sqlite3*)db_handle;
	sqlite3_stmt* stmt = NULL;
	int rc;
	if ( db == NULL ) {
		xsqlite_set_error("null sqlite3 handle");
		return NULL;
	}
	rc = sqlite3_prepare_v2(db, sql == NULL ? "" : sql, -1, &stmt, NULL);
	if ( rc != SQLITE_OK ) {
		xsqlite_set_db_error(db);
		return NULL;
	}
	xsqlite_set_error("");
	return stmt;
}

XSQLITE_EXPORT int xsqlite3_bind_text_native(void* stmt_handle, int index, const char* value)
{
	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_handle;
	int rc;
	if ( stmt == NULL ) {
		xsqlite_set_error("null sqlite3_stmt handle");
		return SQLITE_MISUSE;
	}
	rc = sqlite3_bind_text(stmt, index, value == NULL ? "" : value, -1, SQLITE_TRANSIENT);
	if ( rc != SQLITE_OK ) xsqlite_set_error(sqlite3_errstr(rc));
	else xsqlite_set_error("");
	return rc;
}

XSQLITE_EXPORT int xsqlite3_bind_blob_text_native(void* stmt_handle, int index, const char* value)
{
	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_handle;
	int rc;
	if ( stmt == NULL ) {
		xsqlite_set_error("null sqlite3_stmt handle");
		return SQLITE_MISUSE;
	}
	if ( value == NULL ) value = "";
	rc = sqlite3_bind_blob(stmt, index, value, (int)strlen(value), SQLITE_TRANSIENT);
	if ( rc != SQLITE_OK ) xsqlite_set_error(sqlite3_errstr(rc));
	else xsqlite_set_error("");
	return rc;
}

XSQLITE_EXPORT const char* xsqlite3_column_text_native(void* stmt_handle, int column)
{
	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_handle;
	const unsigned char* text;
	if ( stmt == NULL ) {
		xsqlite_set_error("null sqlite3_stmt handle");
		return "";
	}
	text = sqlite3_column_text(stmt, column);
	xsqlite_set_error("");
	return text == NULL ? "" : (const char*)text;
}

XSQLITE_EXPORT const char* xsqlite3_column_name_native(void* stmt_handle, int column)
{
	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_handle;
	const char* name;
	if ( stmt == NULL ) {
		xsqlite_set_error("null sqlite3_stmt handle");
		return "";
	}
	name = sqlite3_column_name(stmt, column);
	xsqlite_set_error("");
	return name == NULL ? "" : name;
}

XSQLITE_EXPORT const char* xsqlite3_last_insert_rowid_text_native(void* db_handle)
{
	sqlite3* db = (sqlite3*)db_handle;
	if ( db == NULL ) {
		xsqlite_set_error("null sqlite3 handle");
		return "";
	}
	snprintf(g_xsqlite_int64, sizeof(g_xsqlite_int64), "%lld", (long long)sqlite3_last_insert_rowid(db));
	xsqlite_set_error("");
	return g_xsqlite_int64;
}

XSQLITE_EXPORT const char* xsqlite3_errstr_native(int rc)
{
	return sqlite3_errstr(rc);
}

XSQLITE_EXPORT const char* xsqlite3_libversion_native(void)
{
	return sqlite3_libversion();
}

XSQLITE_EXPORT int xsqlite3_libversion_number_native(void)
{
	return sqlite3_libversion_number();
}

XSQLITE_EXPORT int xsqlite3_close_native(void* db)
{
	return sqlite3_close((sqlite3*)db);
}

XSQLITE_EXPORT int xsqlite3_close_v2_native(void* db)
{
	return sqlite3_close_v2((sqlite3*)db);
}

XSQLITE_EXPORT const char* xsqlite3_errmsg_native(void* db)
{
	if ( db == NULL ) return g_xsqlite_error;
	return sqlite3_errmsg((sqlite3*)db);
}

XSQLITE_EXPORT int xsqlite3_extended_errcode_native(void* db)
{
	if ( db == NULL ) return SQLITE_MISUSE;
	return sqlite3_extended_errcode((sqlite3*)db);
}

XSQLITE_EXPORT int xsqlite3_changes_native(void* db)
{
	if ( db == NULL ) return 0;
	return sqlite3_changes((sqlite3*)db);
}

XSQLITE_EXPORT int xsqlite3_total_changes_native(void* db)
{
	if ( db == NULL ) return 0;
	return sqlite3_total_changes((sqlite3*)db);
}

XSQLITE_EXPORT int xsqlite3_step_native(void* stmt)
{
	return sqlite3_step((sqlite3_stmt*)stmt);
}

XSQLITE_EXPORT int xsqlite3_finalize_native(void* stmt)
{
	return sqlite3_finalize((sqlite3_stmt*)stmt);
}

XSQLITE_EXPORT int xsqlite3_reset_native(void* stmt)
{
	return sqlite3_reset((sqlite3_stmt*)stmt);
}

XSQLITE_EXPORT int xsqlite3_clear_bindings_native(void* stmt)
{
	return sqlite3_clear_bindings((sqlite3_stmt*)stmt);
}

XSQLITE_EXPORT int xsqlite3_bind_int_native(void* stmt, int index, int value)
{
	return sqlite3_bind_int((sqlite3_stmt*)stmt, index, value);
}

XSQLITE_EXPORT int xsqlite3_bind_double_native(void* stmt, int index, double value)
{
	return sqlite3_bind_double((sqlite3_stmt*)stmt, index, value);
}

XSQLITE_EXPORT int xsqlite3_bind_null_native(void* stmt, int index)
{
	return sqlite3_bind_null((sqlite3_stmt*)stmt, index);
}

XSQLITE_EXPORT int xsqlite3_bind_parameter_count_native(void* stmt)
{
	return sqlite3_bind_parameter_count((sqlite3_stmt*)stmt);
}

XSQLITE_EXPORT int xsqlite3_column_count_native(void* stmt)
{
	return sqlite3_column_count((sqlite3_stmt*)stmt);
}

XSQLITE_EXPORT int xsqlite3_column_type_native(void* stmt, int column)
{
	return sqlite3_column_type((sqlite3_stmt*)stmt, column);
}

XSQLITE_EXPORT int xsqlite3_column_int_native(void* stmt, int column)
{
	return sqlite3_column_int((sqlite3_stmt*)stmt, column);
}

XSQLITE_EXPORT double xsqlite3_column_double_native(void* stmt, int column)
{
	return sqlite3_column_double((sqlite3_stmt*)stmt, column);
}

XSQLITE_EXPORT int xsqlite3_column_bytes_native(void* stmt, int column)
{
	return sqlite3_column_bytes((sqlite3_stmt*)stmt, column);
}
