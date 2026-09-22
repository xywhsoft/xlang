#define XRT_MODULE_ALL
#include <stdio.h>
#include <string.h>

void* xsqlite3_open_native(const char* filename);
int xsqlite3_close_native(void* db);
int xsqlite3_exec_native(void* db, const char* sql);
void* xsqlite3_prepare_v2_native(void* db, const char* sql);
int xsqlite3_bind_text_native(void* stmt, int index, const char* value);
int xsqlite3_step_native(void* stmt);
const char* xsqlite3_column_text_native(void* stmt, int column);
int xsqlite3_finalize_native(void* stmt);
const char* xsqlite3_last_error(void);

int main(void)
{
    void* db = xsqlite3_open_native(":memory:");
    void* statement;

    if ( db == NULL ) return 1;
    if ( xsqlite3_exec_native(db, "create table entry(name text)") != 0 ) return 2;
    statement = xsqlite3_prepare_v2_native(db, "insert into entry(name) values(?)");
    if ( statement == NULL || xsqlite3_bind_text_native(statement, 1, "xlang") != 0 ||
         xsqlite3_step_native(statement) != 101 || xsqlite3_finalize_native(statement) != 0 ) return 3;
    statement = xsqlite3_prepare_v2_native(db, "select name from entry");
    if ( statement == NULL || xsqlite3_step_native(statement) != 100 ||
         strcmp(xsqlite3_column_text_native(statement, 0), "xlang") != 0 ||
         xsqlite3_finalize_native(statement) != 0 ) return 4;
    if ( xsqlite3_exec_native(NULL, "select 1") != 21 ||
         strstr(xsqlite3_last_error(), "null sqlite3 handle") == NULL ) return 5;
    if ( xsqlite3_close_native(db) != 0 ) return 6;

    puts("sqlite c smoke ok");
    return 0;
}

#include "../third_party/sqlite3.c"
#include "../sqlite_native.c"
