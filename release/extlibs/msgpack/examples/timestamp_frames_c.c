/* 与 timestamp_frames.xl 使用相同线格式、时间值和五行预期输出。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER_HEX
#define XRT_MODULE_CODEC_HEX
#define XRT_MODULE_STRING
#define XRT_MODULE_UNICODE
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_TIME
#include "../msgpack_native.h"
#include <stdio.h>

static bool timestamp(cstr label, cstr hex, xtime expected)
{
    xbuffer* input = xmsgpack_from_hex(hex);
    xvalue* value = input != NULL ? xmsgpack_decode(input->Data, input->Size) : NULL;
    xbuffer* encoded = value != NULL ? xmsgpack_encode(value) : NULL;
    str output = encoded != NULL ? xmsgpack_to_hex(encoded->Data, encoded->Size) : NULL;
    xtime time;
    bool ok = output != NULL && xrtValueGetTime(value, &time) && time == expected;
    if (ok) printf("%s=%s\n", label, output);
    xrtFree(output);
    xrtBufferDestroy(encoded);
    xrtValueRelease(value);
    xrtBufferDestroy(input);
    return ok;
}

int main(void)
{
    if (!timestamp("epoch", "c80004ff00000000", XRT_TIME_UNIX_EPOCH) ||
        !timestamp("millis", "c900000008ff003d090000000000", XRT_TIME_UNIX_EPOCH+1) ||
        !timestamp("negative", "c8000cff1dcd6500ffffffffffffffff", XRT_TIME_UNIX_EPOCH-500)) return 1;

    xbuffer* raw = xmsgpack_from_hex("00ff41");
    xbuffer* binary = raw != NULL ? xmsgpack_encode_bytes(raw->Data, raw->Size) : NULL;
    xbuffer* restored = binary != NULL ? xmsgpack_decode_bytes(binary->Data, binary->Size) : NULL;
    str wire = binary != NULL ? xmsgpack_to_hex(binary->Data, binary->Size) : NULL;
    str bytes = restored != NULL ? xmsgpack_to_hex(restored->Data, restored->Size) : NULL;
    bool ok = bytes != NULL && wire != NULL;
    if (ok) printf("binary=%s\nbytes=%s\n", wire, bytes);
    xrtFree(bytes);
    xrtFree(wire);
    xrtBufferDestroy(restored);
    xrtBufferDestroy(binary);
    xrtBufferDestroy(raw);
    return ok ? 0 : 1;
}

#include "../msgpack_native.c"
