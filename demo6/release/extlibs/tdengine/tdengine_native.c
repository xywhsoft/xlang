/* tdengine_native.c - TDengine time-series database helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char str_buf[4096];

/* Build CREATE DATABASE statement */
const char *xtdengine_create_db(const char *name, long long precision) {
    const char *prec_str = "ms";
    if (precision == 0) prec_str = "ms";
    else if (precision == 1) prec_str = "us";
    else if (precision == 2) prec_str = "ns";
    sprintf(str_buf, "CREATE DATABASE IF NOT EXISTS %s PRECISION '%s'", name, prec_str);
    return str_buf;
}

/* Build CREATE STABLE (super table) statement */
const char *xtdengine_create_stable(const char *name, const char *columns, const char *tags) {
    sprintf(str_buf, "CREATE STABLE IF NOT EXISTS %s (%s) TAGS (%s)", name, columns, tags);
    return str_buf;
}

/* Build CREATE TABLE using STABLE */
const char *xtdengine_create_table(const char *table, const char *stable, const char *tag_values) {
    sprintf(str_buf, "CREATE TABLE IF NOT EXISTS %s USING %s TAGS (%s)", table, stable, tag_values);
    return str_buf;
}

/* Build INSERT statement */
const char *xtdengine_insert(const char *table, long long ts, const char *values) {
    sprintf(str_buf, "INSERT INTO %s VALUES (%lld, %s)", table, ts, values);
    return str_buf;
}

/* Build SELECT statement */
const char *xtdengine_select(const char *columns, const char *table, const char *where) {
    if (where && strlen(where) > 0) {
        sprintf(str_buf, "SELECT %s FROM %s WHERE %s", columns, table, where);
    } else {
        sprintf(str_buf, "SELECT %s FROM %s", columns, table);
    }
    return str_buf;
}

/* Build time-range query */
const char *xtdengine_time_range(const char *table, long long start_ts, long long end_ts) {
    sprintf(str_buf, "SELECT * FROM %s WHERE ts >= %lld AND ts < %lld", table, start_ts, end_ts);
    return str_buf;
}

/* Build interval aggregation */
const char *xtdengine_interval(const char *table, const char *func, const char *col, long long interval_ms) {
    sprintf(str_buf, "SELECT _wstart, %s(%s) FROM %s INTERVAL(%lld)", func, col, table, interval_ms);
    return str_buf;
}

/* Data type names */
const char *xtdengine_type_name(long long dtype) {
    switch(dtype) {
        case 0: return "TIMESTAMP";
        case 1: return "INT";
        case 2: return "BIGINT";
        case 3: return "FLOAT";
        case 4: return "DOUBLE";
        case 5: return "BINARY";
        case 6: return "NCHAR";
        case 7: return "BOOL";
        case 8: return "TINYINT";
        case 9: return "SMALLINT";
        default: return "UNKNOWN";
    }
}

/* Aggregate function names */
const char *xtdengine_agg_name(long long agg) {
    switch(agg) {
        case 0: return "COUNT";
        case 1: return "AVG";
        case 2: return "SUM";
        case 3: return "MIN";
        case 4: return "MAX";
        case 5: return "FIRST";
        case 6: return "LAST";
        case 7: return "STDDEV";
        default: return "UNKNOWN";
    }
}

/* Validate database name */
long long xtdengine_valid_db_name(const char *name) {
    int len = (int)strlen(name);
    if (len < 1 || len > 192) return 0;
    if (name[0] >= '0' && name[0] <= '9') return 0; /* cannot start with digit */
    return 1;
}

/* Validate table name */
long long xtdengine_valid_table_name(const char *name) {
    int len = (int)strlen(name);
    if (len < 1 || len > 192) return 0;
    return 1;
}

/* Default port */
long long xtdengine_port(void) { return 6030; }

/* REST API port */
long long xtdengine_rest_port(void) { return 6041; }

/* Precision names */
const char *xtdengine_precision_name(long long prec) {
    switch(prec) {
        case 0: return "ms";
        case 1: return "us";
        case 2: return "ns";
        default: return "UNKNOWN";
    }
}
