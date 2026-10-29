#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_BUFFER
#include "xrt.h"
#include "../cbor_native.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../cbor_native.c"
#endif

int main(void) {
    xvalue *number = xrtValueInt(42), *message = NULL;
    xbuffer *wire = NULL, *raw = NULL, *encoded = NULL, *restored = NULL;
    char *hex = NULL;
    xstrview text;
    int64 count;
    int status = 1;
    if (!number || !(wire = xcbor_encode(number)) ||
        !(hex = xcbor_to_hex(wire->Data, wire->Size))) goto done;
    printf("integer: %s\n", hex);
    xrtFree(hex); hex = NULL; xrtBufferDestroy(wire); wire = NULL;
    wire = xcbor_from_hex("a2636d736762686965636f756e74182a");
    if (!wire || !(message = xcbor_decode(wire->Data, wire->Size)) ||
        !xrtValueGetString(xrtValueObjectGet(message, XRT_STR_LITERAL("msg")), &text) ||
        !xrtValueGetInt(xrtValueObjectGet(message, XRT_STR_LITERAL("count")), &count) ||
        text.Size != 2 || memcmp(text.Data, "hi", 2) != 0 || count != 42) goto done;
    printf("message: %.*s %" PRId64 "\n", (int)text.Size, text.Data, count);
    xrtValueRelease(message); message = NULL; xrtBufferDestroy(wire); wire = NULL;
    raw = xcbor_from_hex("00ff41");
    if (!raw || !(encoded = xcbor_encode_bytes(raw->Data, raw->Size)) ||
        !(restored = xcbor_decode_bytes(encoded->Data, encoded->Size)) ||
        !(hex = xcbor_to_hex(restored->Data, restored->Size)) || strcmp(hex, "00ff41")) goto done;
    printf("binary: %s\n", hex);
    xrtFree(hex); hex = NULL;
    static const char *invalid[] = {"1f", "f816", "5f00ff"};
    for (size_t i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        wire = xcbor_from_hex(invalid[i]);
        if (!wire) goto done;
        message = xcbor_decode(wire->Data, wire->Size);
        if (message || xrtErrorKind(xrtGetError()) != XERR_VALUE) goto done;
        printf("reject: %s\n", invalid[i]);
        xrtBufferDestroy(wire); wire = NULL;
    }
    message = xcbor_decode("\xf6", 1);
    if (!message || xcbor_failed() || xrtValueType(message) != XVALUE_NULL) goto done;
    puts("cbor wire validation: PASS");
    status = 0;
done:
    /* C持有XRT值/缓冲/字符串，各按其公开所有权接口释放。 */
    xrtValueRelease(number); xrtValueRelease(message);
    xrtBufferDestroy(wire); xrtBufferDestroy(raw);
    xrtBufferDestroy(encoded); xrtBufferDestroy(restored); xrtFree(hex);
    return status;
}
