/*
 * clickhouse_native.c - ClickHouse protocol helpers
 * Tier 3: column-oriented OLAP database protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* Client packet types */
const char* xch_client_packet(long long type) {
    switch (type) {
        case 0: return "Hello";
        case 1: return "Query";
        case 2: return "Data";
        case 3: return "Cancel";
        case 4: return "Ping";
        case 5: return "TablesStatusRequest";
        case 6: return "KeepAlive";
        case 7: return "Scalar";
        case 8: return "IgnoredPartUUID";
        case 9: return "ReadTaskResponse";
        case 10: return "MergeTreeReadTaskResponse";
        default: return "Unknown";
    }
}

/* Server packet types */
const char* xch_server_packet(long long type) {
    switch (type) {
        case 0: return "Hello";
        case 1: return "Data";
        case 2: return "Exception";
        case 3: return "Progress";
        case 4: return "Pong";
        case 5: return "EndOfStream";
        case 6: return "ProfileInfo";
        case 7: return "Totals";
        case 8: return "Extremes";
        case 9: return "TablesStatusResponse";
        case 10: return "Log";
        case 11: return "TableColumns";
        case 12: return "PartUUIDs";
        case 13: return "ReadTaskRequest";
        case 14: return "MergeTreeReadTaskRequest";
        default: return "Unknown";
    }
}

/* Column type names */
const char* xch_type_name(long long type_id) {
    switch (type_id) {
        case 0: return "UInt8";
        case 1: return "UInt16";
        case 2: return "UInt32";
        case 3: return "UInt64";
        case 4: return "Int8";
        case 5: return "Int16";
        case 6: return "Int32";
        case 7: return "Int64";
        case 8: return "Float32";
        case 9: return "Float64";
        case 10: return "String";
        case 11: return "FixedString";
        case 12: return "DateTime";
        case 13: return "Date";
        case 14: return "UUID";
        case 15: return "IPv4";
        case 16: return "IPv6";
        case 17: return "Decimal";
        case 18: return "Enum8";
        case 19: return "Enum16";
        default: return "Unknown";
    }
}

/* Compression method */
const char* xch_compression(long long method) {
    switch (method) {
        case 0: return "None";
        case 1: return "LZ4";
        case 2: return "ZSTD";
        default: return "Unknown";
    }
}

/* Format a varint (LEB128 unsigned) into hex string */
const char* xch_encode_varint(long long value) {
    unsigned char tmp[10];
    int len = 0;
    unsigned long long v = (unsigned long long)value;
    do {
        unsigned char b = (unsigned char)(v & 0x7F);
        v >>= 7;
        if (v > 0) b |= 0x80;
        tmp[len++] = b;
    } while (v > 0 && len < 10);
    int pos = 0;
    for (int i = 0; i < len; i++) {
        pos += sprintf(buf + pos, "%02x", tmp[i]);
    }
    buf[pos] = '\0';
    return buf;
}

/* Decode varint from hex string, return value */
long long xch_decode_varint(const char *hex) {
    unsigned long long result = 0;
    int shift = 0;
    int len = (int)strlen(hex);
    for (int i = 0; i + 1 < len; i += 2) {
        unsigned int byte_val;
        char tmp[3] = {hex[i], hex[i+1], '\0'};
        sscanf(tmp, "%02x", &byte_val);
        result |= (unsigned long long)(byte_val & 0x7F) << shift;
        if ((byte_val & 0x80) == 0) break;
        shift += 7;
    }
    return (long long)result;
}

/* Query settings: return default value for known settings */
const char* xch_setting_default(const char *name) {
    if (strcmp(name, "max_block_size") == 0) return "65536";
    if (strcmp(name, "max_threads") == 0) return "0";
    if (strcmp(name, "max_memory_usage") == 0) return "10000000000";
    if (strcmp(name, "max_execution_time") == 0) return "0";
    if (strcmp(name, "max_result_rows") == 0) return "0";
    if (strcmp(name, "max_result_bytes") == 0) return "0";
    if (strcmp(name, "max_insert_block_size") == 0) return "1048576";
    if (strcmp(name, "min_insert_block_size_rows") == 0) return "1048576";
    if (strcmp(name, "min_insert_block_size_bytes") == 0) return "268435456";
    if (strcmp(name, "connect_timeout") == 0) return "10";
    if (strcmp(name, "receive_timeout") == 0) return "300";
    if (strcmp(name, "send_timeout") == 0) return "300";
    return "";
}

/* Table engine name */
const char* xch_engine_name(long long id) {
    switch (id) {
        case 0: return "MergeTree";
        case 1: return "ReplacingMergeTree";
        case 2: return "SummingMergeTree";
        case 3: return "AggregatingMergeTree";
        case 4: return "CollapsingMergeTree";
        case 5: return "VersionedCollapsingMergeTree";
        case 6: return "GraphiteMergeTree";
        case 7: return "Log";
        case 8: return "TinyLog";
        case 9: return "StripeLog";
        case 10: return "Memory";
        case 11: return "Buffer";
        case 12: return "Null";
        case 13: return "Distributed";
        case 14: return "ReplicatedMergeTree";
        default: return "Unknown";
    }
}
