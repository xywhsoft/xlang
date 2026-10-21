#define XRT_MODULE_ALL
#include "xrt.h"
#include "../msgpack_native.h"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

int main(int argc, char **argv) {
    xvalue* object = xrtValueObject();
    xvalue* name = xrtValueString(XRT_STR_LITERAL("xlang"));
    xvalue* count = xrtValueInt(7);
    xvalue* maximum = xrtValueUInt(UINT64_MAX);
    xvalue* when = xrtValueTime(1);
    xbuffer* packed = NULL;
    xbuffer* binary = NULL;
    xbuffer* binary_packed = NULL;
    xbuffer* binary_restored = NULL;
    xvalue* restored = NULL;
    str binary_wire_hex = NULL;
    str binary_hex = NULL;
    xvalue* field;
    xstrview text;
    int64 number;
    uint64 unsigned_number;
    xtime moment;
    int status = 1;

    if (argc != 1 && argc != 2) goto done;
    if (!object || !name || !count || !maximum || !when ||
        !xrtValueObjectSetTake(object, XRT_STR_LITERAL("name"), &name) ||
        !xrtValueObjectSetTake(object, XRT_STR_LITERAL("count"), &count) ||
        !xrtValueObjectSetTake(object, XRT_STR_LITERAL("maximum"), &maximum) ||
        !xrtValueObjectSetTake(object, XRT_STR_LITERAL("when"), &when)) goto done;
    packed = xmsgpack_encode(object);
    restored = packed ? xmsgpack_decode(packed->Data, packed->Size) : NULL;
    if (!packed || !restored || xrtValueType(restored) != XVALUE_OBJECT) goto done;
    field = xrtValueObjectGet(restored, XRT_STR_LITERAL("name"));
    if (!field || !xrtValueGetString(field, &text)) goto done;
    printf("name=%.*s\n", (int)text.Size, text.Data);
    field = xrtValueObjectGet(restored, XRT_STR_LITERAL("count"));
    if (!field || !xrtValueGetInt(field, &number)) goto done;
    printf("count=%lld\n", (long long)number);
    field = xrtValueObjectGet(restored, XRT_STR_LITERAL("maximum"));
    if (!field || !xrtValueGetUInt(field, &unsigned_number) ||
        unsigned_number != UINT64_MAX) goto done;
    printf("maximum=%" PRIu64 "\n", unsigned_number);
    field = xrtValueObjectGet(restored, XRT_STR_LITERAL("when"));
    if (!field || !xrtValueGetTime(field, &moment) || moment != 1) goto done;
    printf("when=%lld\n", (long long)moment);
    printf("bytes=%zu\n", packed->Size);
    /* 二进制值包含 NUL 和非 UTF-8 字节，使用 bin 而非 str。 */
    binary = xmsgpack_from_hex("00ff41");
    binary_packed = binary ? xmsgpack_encode_bytes(binary->Data, binary->Size) : NULL;
    binary_wire_hex = binary_packed ? xmsgpack_to_hex(binary_packed->Data,
        binary_packed->Size) : NULL;
    binary_restored = binary_packed ? xmsgpack_decode_bytes(binary_packed->Data,
        binary_packed->Size) : NULL;
    binary_hex = binary_restored ? xmsgpack_to_hex(binary_restored->Data,
        binary_restored->Size) : NULL;
    if (!binary || !binary_packed || !binary_wire_hex || !binary_restored ||
        !binary_hex || strcmp(binary_wire_hex, "c40300ff41") != 0 ||
        strcmp(binary_hex, "00ff41") != 0) goto done;
    printf("binary=%s\n", binary_hex);
    if (argc == 2 && !xrtFileWriteAll(argv[1],
        (xbytesview){packed->Data, packed->Size})) goto done;
    status = 0;
done:
    xrtFree(binary_hex);
    xrtFree(binary_wire_hex);
    xrtBufferDestroy(binary_restored);
    xrtBufferDestroy(binary_packed);
    xrtBufferDestroy(binary);
    xrtValueRelease(restored);
    xrtBufferDestroy(packed);
    xrtValueRelease(when);
    xrtValueRelease(count);
    xrtValueRelease(maximum);
    xrtValueRelease(name);
    xrtValueRelease(object);
    return status;
}

/* 单文件范例直接运行；正式包仍由 manifest 编译原生实现。 */
#include "../msgpack_native.c"
