#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/redis/include/redis_native.h"

#include <stdio.h>
#include <string.h>

static bool append_text(xvalue* Array, const char* Data, size_t Size)
{
    xvalue* Text = xrtValueString((xstrview){ Data, Size });
    bool Ok = Text != NULL && xrtValueArrayAppendTake(Array, &Text);
    xrtValueRelease(Text);
    return Ok;
}

int main(void)
{
    xvalue* Args = xrtValueArray();
    xvalue* Request;
    xvalue* Input;
    xvalue* Bulk;
    xvalue* Array;
    xvalue* BulkValue;
    xvalue* ArrayValue;
    xvalue* BinaryArgs;
    xvalue* BinaryValue;
    xvalue* BinaryReply = NULL;
    xbuffer* BinaryRequest;
    xstrview Bytes;
    xbytesview BinaryBytes;
    xstrview Key = XRT_STR_LITERAL("value");
    const unsigned char RawReply[] = { '$', '3', '\r', '\n', 'a', 0xffu, 'b', '\r', '\n' };
    int Ok;
    void* Frame;

    if (Args == NULL || !append_text(Args, "SET", 3u) ||
        !append_text(Args, "a\0b", 3u) || !append_text(Args, "hello", 5u)) {
        xrtValueRelease(Args);
        return 2;
    }
    Request = xr_encode(Args);
    Input = xrtValueString(XRT_STR_LITERAL("$3\r\na\0b\r\n"));
    Bulk = xr_decode(Input);
    xrtValueRelease(Input);
    Input = xrtValueString(XRT_STR_LITERAL("*2\r\n$3\r\none\r\n$3\r\ntwo\r\n"));
    Array = xr_decode(Input);
    xrtValueRelease(Input);

    BulkValue = xrtValueObjectGet(xrtValueObjectGet(Bulk, Key), Key);
    ArrayValue = xrtValueObjectGet(xrtValueObjectGet(Array, Key), Key);
    Ok = Request != NULL && xrtValueGetString(Request, &Bytes);
    if (Ok) printf("encoded bytes: %llu\n", (unsigned long long)Bytes.Size);
    Ok = Ok && BulkValue != NULL && xrtValueGetString(BulkValue, &Bytes);
    if (Ok) printf("bulk value bytes: %llu\n", (unsigned long long)Bytes.Size);
    Ok = Ok && ArrayValue != NULL && xrtValueType(ArrayValue) == XVALUE_ARRAY;
    if (Ok) printf("array reply: %llu items\n", (unsigned long long)xrtValueCount(ArrayValue));

    /* 收包缓冲可搬迁；增量状态只记录已经扫描的字节偏移。 */
    Frame = xr_frame_create();
    Ok = Ok && Frame != NULL &&
        !xr_frame_check(Frame, "*1\r\n$3\r\nab", 10u) && xrtGetError() == NULL &&
        xr_frame_check(Frame, "*1\r\n$3\r\nabc\r\n", 13u) &&
        xrtGetError() == NULL;
    xr_frame_destroy(Frame);

    BinaryArgs = xrtValueArray();
    BinaryValue = xrtValueBytes((xbytesview){ (const unsigned char*)"\x00\xff" "A", 3u });
    Ok = Ok && BinaryArgs != NULL && BinaryValue != NULL &&
        append_text(BinaryArgs, "SET", 3u) && append_text(BinaryArgs, "k", 1u) &&
        xrtValueArrayAppendTake(BinaryArgs, &BinaryValue);
    BinaryRequest = Ok ? (xbuffer*)xr_encode_bytes(BinaryArgs) : NULL;
    Ok = Ok && BinaryRequest != NULL;
    if (Ok) {
        printf("binary request bytes: %llu\n", (unsigned long long)xrtBufferView(BinaryRequest).Size);
        /* 先确认分片尚未形成完整 RESP 帧，再解析含非 UTF-8 字节的正文。 */
        Ok = !xr_is_complete_raw((void*)RawReply, sizeof(RawReply) - 2u) &&
            xrtGetError() == NULL &&
            xr_is_complete_raw((void*)RawReply, sizeof(RawReply));
        BinaryReply = Ok ? xr_decode_raw((void*)RawReply, sizeof(RawReply)) : NULL;
        BinaryValue = BinaryReply ? xrtValueObjectGet(BinaryReply, Key) : NULL;
        BinaryValue = BinaryValue ? xrtValueObjectGet(BinaryValue, Key) : NULL;
        Ok = Ok && BinaryValue != NULL && xrtValueGetBytes(BinaryValue, &BinaryBytes) &&
            BinaryBytes.Size == 3u && memcmp(BinaryBytes.Data, "a\xff" "b", 3u) == 0;
    }
    if (Ok) {
        puts("binary decode: true");
        puts("redis C/XL example: PASS");
    }
    xrtValueRelease(BinaryReply);
    xrtBufferDestroy(BinaryRequest);
    xrtValueRelease(BinaryValue);
    xrtValueRelease(BinaryArgs);
    xrtValueRelease(Array);
    xrtValueRelease(Bulk);
    xrtValueRelease(Request);
    xrtValueRelease(Args);
    return Ok ? 0 : 2;
}

/* 单文件范例直接包含原生实现；包导入时仍按清单独立编译。 */
#include "../../extlibs/redis/redis_native.c"
