#define XRT_MODULE_ALL
#include "xrt.h"
#include "../cbor_native.h"
#include "../cbor_native.c"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    xvalue *source = xrtValueObject();
    xvalue *name = xrtValueString(XRT_STR_LITERAL("xlang"));
    xvalue *maximum = xrtValueUInt(UINT64_MAX);
    xvalue *restored = NULL;
    xbuffer *wire = NULL;
    xbuffer *raw = NULL;
    xbuffer *binary_wire = NULL;
    xbuffer *binary_restored = NULL;
    char *hex = NULL;
    char *binary_wire_hex = NULL;
    char *binary_hex = NULL;
    xstrview text;
    uint64 unsigned_number;
    int status = 1;
    if (argc != 1 && argc != 2) goto done;
    if (!source || !name || !maximum ||
        !xrtValueObjectSetTake(source, XRT_STR_LITERAL("name"), &name) ||
        !xrtValueObjectSetTake(source, XRT_STR_LITERAL("maximum"), &maximum)) goto done;
    wire = xcbor_encode(source);
    if (!wire) goto done;
    hex = xcbor_to_hex(wire->Data, wire->Size);
    restored = xcbor_decode(wire->Data, wire->Size);
    if (!hex || !restored || xrtValueType(restored) != XVALUE_OBJECT ||
        !xrtValueGetString(xrtValueObjectGet(restored, XRT_STR_LITERAL("name")), &text) ||
        !xrtValueGetUInt(xrtValueObjectGet(restored, XRT_STR_LITERAL("maximum")),
            &unsigned_number) || unsigned_number != UINT64_MAX) goto done;
    /* bin 值可以原样保存非 UTF-8 字节。 */
    raw = xcbor_from_hex("00ff41");
    binary_wire = raw ? xcbor_encode_bytes(raw->Data, raw->Size) : NULL;
    binary_wire_hex = binary_wire ? xcbor_to_hex(binary_wire->Data,
        binary_wire->Size) : NULL;
    binary_restored = binary_wire ? xcbor_decode_bytes(binary_wire->Data,
        binary_wire->Size) : NULL;
    binary_hex = binary_restored ? xcbor_to_hex(binary_restored->Data,
        binary_restored->Size) : NULL;
    if (!raw || !binary_wire || !binary_wire_hex || !binary_restored ||
        !binary_hex || strcmp(binary_wire_hex, "4300ff41") != 0 ||
        strcmp(binary_hex, "00ff41") != 0) goto done;
    if (argc == 2 && !xrtFileWriteAll(argv[1],
        (xbytesview){wire->Data, wire->Size})) goto done;
    printf("hex: %s\n", hex);
    printf("name: %.*s\n", (int)text.Size, text.Data);
    printf("maximum: %llu\n", (unsigned long long)unsigned_number);
    printf("binary: %s\n", binary_hex);
    status = 0;
done:
    xrtFree(binary_hex);
    xrtFree(binary_wire_hex);
    xrtBufferDestroy(binary_restored);
    xrtBufferDestroy(binary_wire);
    xrtBufferDestroy(raw);
    xrtFree(hex);
    xrtValueRelease(restored);
    xrtBufferDestroy(wire);
    xrtValueRelease(maximum);
    xrtValueRelease(name);
    xrtValueRelease(source);
    return status;
}
