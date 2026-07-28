/*
 * flatbuffers_native.c - FlatBuffers zero-copy serialization
 * Tier 5: serialization
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xflatbuffers_type(long long id) {
    switch (id) {
        case 0: return "bool";
        case 1: return "byte";
        case 2: return "ubyte";
        case 3: return "short";
        case 4: return "ushort";
        case 5: return "int";
        case 6: return "uint";
        case 7: return "long";
        case 8: return "ulong";
        case 9: return "float";
        case 10: return "double";
        case 11: return "string";
        default: return "Unknown";
    }
}

/* Return byte size of a flatbuffers type */
long long xflatbuffers_type_size(long long id) {
    switch (id) {
        case 0: return 1;  /* bool */
        case 1: return 1;  /* byte */
        case 2: return 1;  /* ubyte */
        case 3: return 2;  /* short */
        case 4: return 2;  /* ushort */
        case 5: return 4;  /* int */
        case 6: return 4;  /* uint */
        case 7: return 8;  /* long */
        case 8: return 8;  /* ulong */
        case 9: return 4;  /* float */
        case 10: return 8; /* double */
        case 11: return 4; /* string (offset) */
        default: return 0;
    }
}

/* Align value to boundary */
long long xflatbuffers_align(long long value, long long alignment) {
    if (alignment <= 0) return value;
    long long rem = value % alignment;
    if (rem == 0) return value;
    return value + (alignment - rem);
}

/* Compute vtable offset */
long long xflatbuffers_vtable_offset(long long field_id) {
    return 4 + field_id * 2;
}

const char* xflatbuffers_builder_state(long long id) {
    switch (id) {
        case 0: return "idle";
        case 1: return "building_table";
        case 2: return "building_vector";
        case 3: return "finished";
        default: return "Unknown";
    }
}
