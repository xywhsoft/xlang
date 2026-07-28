/*
 * arrow_native.c - Columnar in-memory format
 * Tier 4: data processing
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xarrow_type(long long id) {
    switch (id) {
        case 0: return "null";
        case 1: return "bool";
        case 2: return "int8";
        case 3: return "int16";
        case 4: return "int32";
        case 5: return "int64";
        case 6: return "uint8";
        case 7: return "uint16";
        case 8: return "uint32";
        case 9: return "uint64";
        case 10: return "float32";
        case 11: return "float64";
        case 12: return "string";
        case 13: return "binary";
        case 14: return "date32";
        case 15: return "date64";
        case 16: return "timestamp";
        case 17: return "decimal128";
        case 18: return "list";
        case 19: return "struct";
        default: return "Unknown";
    }
}

const char* xarrow_buffer_type(long long id) {
    switch (id) {
        case 0: return "validity";
        case 1: return "offset";
        case 2: return "data";
        case 3: return "type_ids";
        default: return "Unknown";
    }
}

const char* xarrow_ipc_format(long long id) {
    switch (id) {
        case 0: return "streaming";
        case 1: return "file";
        default: return "Unknown";
    }
}

const char* xarrow_compute_func(long long id) {
    switch (id) {
        case 0: return "add";
        case 1: return "subtract";
        case 2: return "multiply";
        case 3: return "divide";
        case 4: return "sum";
        case 5: return "mean";
        case 6: return "min";
        case 7: return "max";
        case 8: return "sort_indices";
        case 9: return "filter";
        case 10: return "take";
        case 11: return "unique";
        case 12: return "value_counts";
        case 13: return "cast";
        default: return "Unknown";
    }
}

/* Calculate buffer size with 64-byte alignment */
long long xarrow_aligned_size(long long bytes) {
    return ((bytes + 63) / 64) * 64;
}

const char* xarrow_memory_pool(long long id) {
    switch (id) {
        case 0: return "system";
        case 1: return "jemalloc";
        case 2: return "mimalloc";
        default: return "Unknown";
    }
}

const char* xarrow_flight_action(long long id) {
    switch (id) {
        case 0: return "do_get";
        case 1: return "do_put";
        case 2: return "do_action";
        case 3: return "list_actions";
        case 4: return "list_flights";
        case 5: return "get_flight_info";
        case 6: return "get_schema";
        default: return "Unknown";
    }
}
