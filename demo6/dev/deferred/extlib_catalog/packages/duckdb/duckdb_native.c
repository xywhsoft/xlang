/*
 * duckdb_native.c - Embedded OLAP database
 * Tier 4: database
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xduckdb_type(long long id) {
    switch (id) {
        case 0: return "BOOLEAN";
        case 1: return "TINYINT";
        case 2: return "SMALLINT";
        case 3: return "INTEGER";
        case 4: return "BIGINT";
        case 5: return "FLOAT";
        case 6: return "DOUBLE";
        case 7: return "VARCHAR";
        case 8: return "BLOB";
        case 9: return "TIMESTAMP";
        case 10: return "DATE";
        case 11: return "INTERVAL";
        case 12: return "HUGEINT";
        case 13: return "DECIMAL";
        case 14: return "UUID";
        case 15: return "JSON";
        default: return "Unknown";
    }
}

const char* xduckdb_result_state(long long id) {
    switch (id) {
        case 0: return "success";
        case 1: return "query error";
        case 2: return "connection error";
        case 3: return "out of memory";
        case 4: return "interrupted";
        default: return "Unknown";
    }
}

const char* xduckdb_function_category(long long id) {
    switch (id) {
        case 0: return "aggregate";
        case 1: return "scalar";
        case 2: return "table";
        case 3: return "window";
        case 4: return "macro";
        default: return "Unknown";
    }
}

const char* xduckdb_aggregate_func(long long id) {
    switch (id) {
        case 0: return "count";
        case 1: return "sum";
        case 2: return "avg";
        case 3: return "min";
        case 4: return "max";
        case 5: return "stddev";
        case 6: return "variance";
        case 7: return "median";
        case 8: return "mode";
        case 9: return "string_agg";
        case 10: return "list_agg";
        case 11: return "approx_count_distinct";
        default: return "Unknown";
    }
}

const char* xduckdb_storage_format(long long id) {
    switch (id) {
        case 0: return "duckdb native";
        case 1: return "parquet";
        case 2: return "csv";
        case 3: return "json";
        case 4: return "arrow";
        case 5: return "sqlite";
        default: return "Unknown";
    }
}

/* Estimate query cost: simple heuristic based on table rows and columns */
long long xduckdb_estimate_cost(long long rows, long long cols, long long has_join) {
    long long base = rows * cols;
    if (has_join) base = base * 3;
    return base;
}

const char* xduckdb_extension(long long id) {
    switch (id) {
        case 0: return "parquet";
        case 1: return "json";
        case 2: return "httpfs";
        case 3: return "tpch";
        case 4: return "tpcds";
        case 5: return "fts";
        case 6: return "visualizer";
        case 7: return "excel";
        case 8: return "spatial";
        case 9: return "postgres_scanner";
        default: return "Unknown";
    }
}
