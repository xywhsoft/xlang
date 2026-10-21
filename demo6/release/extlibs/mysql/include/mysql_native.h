#ifndef XL_MYSQL_NATIVE_H
#define XL_MYSQL_NATIVE_H
#ifdef __cplusplus
extern "C" {
#endif
const char* xmysql_cmd_name(long long command);
const char* xmysql_col_type(long long type);
const char* xmysql_status_str(long long status);
const char* xmysql_cap_name(long long capability);
const char* xmysql_escape(const char* input);
const char* xmysql_format_dsn(const char* host, int port, const char* database, const char* user);
const char* xmysql_dsn_part(const char* dsn, const char* part);
const char* xmysql_error_class(long long code);
#ifdef __cplusplus
}
#endif
#endif
