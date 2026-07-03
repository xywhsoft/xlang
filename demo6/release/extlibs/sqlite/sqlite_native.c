#include "third_party/sqlite3.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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
static XSQLITE_THREAD char* g_xsqlite_returns[8];
static XSQLITE_THREAD size_t g_xsqlite_return_slot;

static void xsqlite_set_error(const char* text)
{
	size_t len;
	if ( text == NULL ) text = "";
	len = strlen(text);
	if ( len >= sizeof(g_xsqlite_error) ) {
		len = sizeof(g_xsqlite_error) - 1u;
	}
	if ( len > 0u ) {
		memcpy(g_xsqlite_error, text, len);
	}
	g_xsqlite_error[len] = '\0';
}

static const char* xsqlite_return_text(const char* text)
{
	size_t len;
	size_t slot;
	char* copy;

	if ( text == NULL ) {
		text = "";
	}
	len = strlen(text);
	copy = (char*)malloc(len + 1u);
	if ( copy == NULL ) {
		xsqlite_set_error("out of memory");
		return "";
	}
	if ( len > 0u ) {
		memcpy(copy, text, len);
	}
	copy[len] = '\0';
	slot = g_xsqlite_return_slot % (sizeof(g_xsqlite_returns) / sizeof(g_xsqlite_returns[0]));
	g_xsqlite_return_slot += 1u;
	if ( g_xsqlite_returns[slot] != NULL ) {
		free(g_xsqlite_returns[slot]);
	}
	g_xsqlite_returns[slot] = copy;
	return g_xsqlite_returns[slot];
}

XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_OK = SQLITE_OK;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_ERROR = SQLITE_ERROR;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_INTERNAL = SQLITE_INTERNAL;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_PERM = SQLITE_PERM;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_ABORT = SQLITE_ABORT;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_BUSY = SQLITE_BUSY;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_LOCKED = SQLITE_LOCKED;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_NOMEM = SQLITE_NOMEM;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_READONLY = SQLITE_READONLY;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_INTERRUPT = SQLITE_INTERRUPT;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_IOERR = SQLITE_IOERR;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_CORRUPT = SQLITE_CORRUPT;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_NOTFOUND = SQLITE_NOTFOUND;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_FULL = SQLITE_FULL;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_CANTOPEN = SQLITE_CANTOPEN;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_PROTOCOL = SQLITE_PROTOCOL;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_EMPTY = SQLITE_EMPTY;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_SCHEMA = SQLITE_SCHEMA;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_TOOBIG = SQLITE_TOOBIG;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_CONSTRAINT = SQLITE_CONSTRAINT;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_MISMATCH = SQLITE_MISMATCH;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_MISUSE = SQLITE_MISUSE;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_NOLFS = SQLITE_NOLFS;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_AUTH = SQLITE_AUTH;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_FORMAT = SQLITE_FORMAT;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_RANGE = SQLITE_RANGE;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_NOTADB = SQLITE_NOTADB;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_NOTICE = SQLITE_NOTICE;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_WARNING = SQLITE_WARNING;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_ROW = SQLITE_ROW;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_DONE = SQLITE_DONE;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_INTEGER = SQLITE_INTEGER;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_FLOAT = SQLITE_FLOAT;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_TEXT = SQLITE_TEXT;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_BLOB = SQLITE_BLOB;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_NULL = SQLITE_NULL;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_OPEN_READONLY = SQLITE_OPEN_READONLY;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_OPEN_READWRITE = SQLITE_OPEN_READWRITE;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_OPEN_CREATE = SQLITE_OPEN_CREATE;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_OPEN_URI = SQLITE_OPEN_URI;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_OPEN_MEMORY = SQLITE_OPEN_MEMORY;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_OPEN_NOMUTEX = SQLITE_OPEN_NOMUTEX;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_OPEN_FULLMUTEX = SQLITE_OPEN_FULLMUTEX;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_OPEN_SHAREDCACHE = SQLITE_OPEN_SHAREDCACHE;
XSQLITE_EXPORT int64_t xsqlite_const_SQLITE_OPEN_PRIVATECACHE = SQLITE_OPEN_PRIVATECACHE;

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
	return xsqlite_return_text(g_xsqlite_error);
}

XSQLITE_EXPORT const char* xsqlite_last_error_native(void)
{
	return xsqlite3_last_error();
}

XSQLITE_EXPORT int xsqlite_available_native(void)
{
	return 1;
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
	const char* use_vfs = (vfs != NULL && vfs[0] != '\0') ? vfs : NULL;
	int rc = sqlite3_open_v2(filename == NULL ? "" : filename, &db, flags, use_vfs);
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
	return xsqlite_return_text(text == NULL ? "" : (const char*)text);
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
	return xsqlite_return_text(name == NULL ? "" : name);
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
	return xsqlite_return_text(g_xsqlite_int64);
}

XSQLITE_EXPORT const char* xsqlite3_errstr_native(int rc)
{
	return xsqlite_return_text(sqlite3_errstr(rc));
}

XSQLITE_EXPORT const char* xsqlite3_libversion_native(void)
{
	return xsqlite_return_text(sqlite3_libversion());
}

XSQLITE_EXPORT const char* xsqlite_version_native(void)
{
	return xsqlite3_libversion_native();
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
	if ( db == NULL ) return xsqlite_return_text(g_xsqlite_error);
	return xsqlite_return_text(sqlite3_errmsg((sqlite3*)db));
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
