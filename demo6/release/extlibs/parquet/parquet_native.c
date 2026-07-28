/*
 * parquet_native.c - Columnar file format
 * Tier 4: data processing
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xparquet_type(long long id) {
    switch (id) {
        case 0: return "BOOLEAN";
        case 1: return "INT32";
        case 2: return "INT64";
        case 3: return "INT96";
        case 4: return "FLOAT";
        case 5: return "DOUBLE";
        case 6: return "BYTE_ARRAY";
        case 7: return "FIXED_LEN_BYTE_ARRAY";
        default: return "Unknown";
    }
}

const char* xparquet_encoding(long long id) {
    switch (id) {
        case 0: return "PLAIN";
        case 1: return "PLAIN_DICTIONARY";
        case 2: return "RLE";
        case 3: return "BIT_PACKED";
        case 4: return "DELTA_BINARY_PACKED";
        case 5: return "DELTA_LENGTH_BYTE_ARRAY";
        case 6: return "DELTA_BYTE_ARRAY";
        case 7: return "RLE_DICTIONARY";
        case 8: return "BYTE_STREAM_SPLIT";
        default: return "Unknown";
    }
}

const char* xparquet_compression(long long id) {
    switch (id) {
        case 0: return "UNCOMPRESSED";
        case 1: return "SNAPPY";
        case 2: return "GZIP";
        case 3: return "LZO";
        case 4: return "BROTLI";
        case 5: return "LZ4";
        case 6: return "ZSTD";
        default: return "Unknown";
    }
}

const char* xparquet_repetition(long long id) {
    switch (id) {
        case 0: return "REQUIRED";
        case 1: return "OPTIONAL";
        case 2: return "REPEATED";
        default: return "Unknown";
    }
}

const char* xparquet_converted_type(long long id) {
    switch (id) {
        case 0: return "UTF8";
        case 1: return "MAP";
        case 2: return "LIST";
        case 3: return "ENUM";
        case 4: return "DECIMAL";
        case 5: return "DATE";
        case 6: return "TIME_MILLIS";
        case 7: return "TIMESTAMP_MILLIS";
        case 8: return "INT_8";
        case 9: return "INT_16";
        case 10: return "INT_32";
        case 11: return "INT_64";
        case 12: return "JSON";
        case 13: return "BSON";
        default: return "Unknown";
    }
}

/* Estimate row group size: rows * cols * avg_bytes */
long long xparquet_estimate_size(long long rows, long long cols, long long avg_bytes) {
    return rows * cols * avg_bytes;
}

const char* xparquet_page_type(long long id) {
    switch (id) {
        case 0: return "DATA_PAGE";
        case 1: return "INDEX_PAGE";
        case 2: return "DICTIONARY_PAGE";
        case 3: return "DATA_PAGE_V2";
        default: return "Unknown";
    }
}
