/*
 * protobuf_native.c - Protocol Buffers encoding
 * Tier 5: serialization
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xprotobuf_wire_type(long long id) {
    switch (id) {
        case 0: return "Varint";
        case 1: return "Fixed64";
        case 2: return "LengthDelimited";
        case 5: return "Fixed32";
        default: return "Unknown";
    }
}

/* Encode a varint (simplified: single byte for values < 128) */
long long xprotobuf_varint_size(long long value) {
    if (value < 0) value = -value;
    long long size = 1;
    while (value >= 128) {
        value >>= 7;
        size++;
    }
    return size;
}

/* Encode field tag: (field_number << 3) | wire_type */
long long xprotobuf_tag(long long field_num, long long wire_type) {
    return (field_num << 3) | wire_type;
}

/* Decode field number from tag */
long long xprotobuf_field_number(long long tag) {
    return tag >> 3;
}

/* Decode wire type from tag */
long long xprotobuf_wire_from_tag(long long tag) {
    return tag & 0x07;
}

const char* xprotobuf_type_name(long long id) {
    switch (id) {
        case 0: return "double";
        case 1: return "float";
        case 2: return "int32";
        case 3: return "int64";
        case 4: return "string";
        case 5: return "bytes";
        case 6: return "bool";
        default: return "Unknown";
    }
}
