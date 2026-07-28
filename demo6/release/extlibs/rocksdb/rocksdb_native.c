/*
 * rocksdb_native.c - High-performance embedded KV store
 * Tier 4: database
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xrocksdb_compression(long long id) {
    switch (id) {
        case 0: return "none";
        case 1: return "snappy";
        case 2: return "zlib";
        case 3: return "bz2";
        case 4: return "lz4";
        case 5: return "lz4hc";
        case 6: return "zstd";
        default: return "Unknown";
    }
}

const char* xrocksdb_compaction_style(long long id) {
    switch (id) {
        case 0: return "level";
        case 1: return "universal";
        case 2: return "fifo";
        case 3: return "none";
        default: return "Unknown";
    }
}

const char* xrocksdb_memtable(long long id) {
    switch (id) {
        case 0: return "skip list";
        case 1: return "vector";
        case 2: return "hash skip list";
        case 3: return "hash link list";
        default: return "Unknown";
    }
}

const char* xrocksdb_wal_mode(long long id) {
    switch (id) {
        case 0: return "sync";
        case 1: return "async";
        default: return "Unknown";
    }
}

const char* xrocksdb_iterator_direction(long long id) {
    switch (id) {
        case 0: return "forward";
        case 1: return "reverse";
        default: return "Unknown";
    }
}

/* Simple hash for key distribution estimation */
long long xrocksdb_hash_slot(const char *key, long long num_slots) {
    unsigned long long h = 5381;
    const char *p = key;
    while (*p) {
        h = ((h << 5) + h) + (unsigned char)*p;
        p++;
    }
    if (num_slots <= 0) return 0;
    return (long long)(h % (unsigned long long)num_slots);
}

const char* xrocksdb_cf_option(long long id) {
    switch (id) {
        case 0: return "write_buffer_size";
        case 1: return "max_write_buffer_number";
        case 2: return "level0_file_num_compaction_trigger";
        case 3: return "target_file_size_base";
        case 4: return "max_bytes_for_level_base";
        case 5: return "num_levels";
        case 6: return "bloom_locality";
        case 7: return "memtable_prefix_bloom_size_ratio";
        default: return "Unknown";
    }
}

const char* xrocksdb_stat_name(long long id) {
    switch (id) {
        case 0: return "rocksdb.num-keys-read";
        case 1: return "rocksdb.num-keys-written";
        case 2: return "rocksdb.bytes-read";
        case 3: return "rocksdb.bytes-written";
        case 4: return "rocksdb.compaction.times.micros";
        case 5: return "rocksdb.mem-table-flush-total";
        case 6: return "rocksdb.block-cache-usage";
        case 7: return "rocksdb.estimate-num-keys";
        default: return "Unknown";
    }
}
