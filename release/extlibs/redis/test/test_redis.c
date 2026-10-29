#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/redis_native.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "redis C test failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static bool text_equals(xvalue* Value, const char* Data, size_t Size)
{
    xstrview Text;
    return Value != NULL && xrtValueGetString(Value, &Text) &&
        Text.Size == Size && memcmp(Text.Data, Data, Size) == 0;
}

static xvalue* field(xvalue* Value, const char* Name)
{
    return xrtValueObjectGet(Value, (xstrview){ Name, strlen(Name) });
}

static bool append_text(xvalue* Array, const char* Data, size_t Size)
{
    xvalue* Text = xrtValueString((xstrview){ Data, Size });
    bool Ok = Text != NULL && xrtValueArrayAppendTake(Array, &Text);
    xrtValueRelease(Text);
    return Ok;
}

static uint32_t next_random(uint32_t* State)
{
    *State ^= *State << 13;
    *State ^= *State >> 17;
    *State ^= *State << 5;
    return *State;
}

static int test_mutated_frames(void)
{
    static const unsigned char Source[] =
        "*3\r\n+OK\r\n$3\r\nA\0\xff\r\n*2\r\n:17\r\n$-1\r\n";
    static const unsigned char Changes[] =
        { '\r', '\n', '+', '-', ':', '$', '*', '0', '1', '9', 0, 0xff };
    uint32_t State = UINT32_C(0x6a3d2f17);
    unsigned caseIndex;

    /* 固定畸变与分包位置，交叉验证无状态解析器和有状态扫描器。 */
    for (caseIndex = 0u; caseIndex < 1024u; ++caseIndex) {
        unsigned char Mutated[sizeof(Source) - 1u];
        unsigned char First[sizeof(Mutated)], Second[sizeof(Mutated)];
        size_t length;
        void* Scanner = xr_frame_create();
        CHECK(Scanner != NULL);
        memcpy(Mutated, Source, sizeof(Mutated));
        Mutated[next_random(&State) % sizeof(Mutated)] =
            Changes[next_random(&State) % sizeof(Changes)];
        if ((caseIndex & 1u) != 0u)
            Mutated[next_random(&State) % sizeof(Mutated)] =
                Changes[next_random(&State) % sizeof(Changes)];
        for (length = 0u; length <= sizeof(Mutated); ++length) {
            unsigned char* Buffer = (length & 1u) ? First : Second;
            bool Expected = xr_is_complete_raw(Mutated, length);
            const xerror* Error = xrtGetError();
            bool Failed = Error != NULL;
            xerrkind Kind = Failed ? xrtErrorKind(Error) : 0;
            bool Actual;
            memcpy(Buffer, Mutated, length);
            Actual = xr_frame_check(Scanner, Buffer, length);
            Error = xrtGetError();
            CHECK(Actual == Expected && (Error != NULL) == Failed);
            if (Failed) CHECK(xrtErrorKind(Error) == Kind);
        }
        xr_frame_destroy(Scanner);
    }
    return 0;
}

int main(void)
{
    xvalue* Args = xrtValueArray();
    xvalue* Encoded;
    xvalue* Input;
    xvalue* Reply;
    xvalue* Item;
    xvalue* Elements;
    xbuffer* Binary;
    xstrview Text;
    int64 Number;
    char LongText[12000];
    char Deep[512];
    int i;

    /* 空 RESP 数组不是可执行命令；失败不能污染下一次编码。 */
    CHECK(Args != NULL);
    Encoded = xr_encode(Args);
    CHECK(Encoded != NULL && xrtValueType(Encoded) == XVALUE_NULL &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(Encoded);
    CHECK(xr_encode_bytes(Args) == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(append_text(Args, "PING", 4u));
    Encoded = xr_encode(Args);
    CHECK(text_equals(Encoded, "*1\r\n$4\r\nPING\r\n", 14u) && xrtGetError() == NULL);
    xrtValueRelease(Encoded);
    xrtValueRelease(Args);

    Args = xrtValueArray();
    CHECK(Args != NULL);
    CHECK(append_text(Args, "SET", 3u));
    CHECK(append_text(Args, "a\0b", 3u));
    CHECK(append_text(Args, "hello", 5u));
    Encoded = xr_encode(Args);
    CHECK(text_equals(Encoded, "*3\r\n$3\r\nSET\r\n$3\r\na\0b\r\n$5\r\nhello\r\n", 33u));
    xrtValueRelease(Encoded);
    xrtValueRelease(Args);

    {
        const unsigned char Octets[] = { 0x00u, 0xffu, 0x41u };
        const unsigned char Expected[] = "*3\r\n$3\r\nSET\r\n$6\r\nbinary\r\n$3\r\n\x00\xff" "A\r\n";
        xbytesview View;
        Args = xrtValueArray();
        CHECK(Args != NULL && append_text(Args, "SET", 3u) &&
            append_text(Args, "binary", 6u));
        Item = xrtValueBytes((xbytesview){ Octets, sizeof(Octets) });
        CHECK(Item != NULL && xrtValueArrayAppendTake(Args, &Item));
        Binary = (xbuffer*)xr_encode_bytes(Args);
        CHECK(Binary != NULL);
        View = xrtBufferView(Binary);
        CHECK(View.Size == sizeof(Expected) - 1u &&
            memcmp(View.Data, Expected, View.Size) == 0);
        xrtBufferDestroy(Binary);
        Encoded = xr_encode(Args);
        CHECK(Encoded != NULL && xrtValueType(Encoded) == XVALUE_NULL &&
            xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_TYPE);
        xrtValueRelease(Encoded);
        xrtValueRelease(Args);
    }
    {
        const char Expected[] = "*2\r\n$3\r\nSET\r\n$0\r\n\r\n";
        xbytesview View;
        Args = xrtValueArray();
        CHECK(Args != NULL && append_text(Args, "SET", 3u));
        Item = xrtValueBytes((xbytesview){ NULL, 0u });
        CHECK(Item != NULL && xrtValueArrayAppendTake(Args, &Item));
        Binary = (xbuffer*)xr_encode_bytes(Args);
        CHECK(Binary != NULL);
        View = xrtBufferView(Binary);
        CHECK(View.Size == sizeof(Expected) - 1u &&
            memcmp(View.Data, Expected, View.Size) == 0);
        xrtBufferDestroy(Binary);
        xrtValueRelease(Args);
    }

    Args = xrtValueArray();
    Item = xrtValueInt(1);
    CHECK(Args != NULL && Item != NULL && xrtValueArrayAppendTake(Args, &Item));
    Encoded = xr_encode(Args);
    CHECK(xrtValueType(Encoded) == XVALUE_NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_TYPE);
    xrtValueRelease(Encoded);
    xrtValueRelease(Args);

    Input = xrtValueString(XRT_STR_LITERAL("$3\r\na\0b\r\n+OK\r\n"));
    CHECK(Input != NULL && !xr_is_complete(NULL));
    xrtClearError();
    CHECK(xr_is_complete(Input));
    Reply = xr_decode(Input);
    CHECK(Reply != NULL && xrtValueType(Reply) == XVALUE_OBJECT);
    Item = field(Reply, "value");
    CHECK(text_equals(field(Item, "type"), "bulk", 4u));
    CHECK(text_equals(field(Item, "value"), "a\0b", 3u));
    CHECK(text_equals(field(Reply, "rest"), "+OK\r\n", 5u));
    xrtValueRelease(Reply);
    xrtValueRelease(Input);

    {
        const unsigned char Raw[] = { '$', '3', '\r', '\n', 'a', 0xffu, 'b', '\r', '\n' };
        xbytesview Bytes;
        CHECK(xr_is_complete_raw((void*)Raw, sizeof(Raw)));
        Reply = xr_decode_raw((void*)Raw, sizeof(Raw));
        Item = field(field(Reply, "value"), "value");
        CHECK(Item != NULL && xrtValueGetBytes(Item, &Bytes) &&
            Bytes.Size == 3u && Bytes.Data[0] == 'a' &&
            Bytes.Data[1] == 0xffu && Bytes.Data[2] == 'b');
        xrtValueRelease(Reply);
    }
    {
        const unsigned char Raw[] = { '+', 'O', 'K', '\r', '\n', '$', '1', '\r', '\n',
            0xffu, '\r', '\n' };
        xbytesview Bytes;
        Reply = xr_decode_raw((void*)Raw, sizeof(Raw));
        Item = field(Reply, "rest");
        CHECK(Item != NULL && xrtValueGetBytes(Item, &Bytes) &&
            Bytes.Size == 7u && Bytes.Data[4] == 0xffu);
        xrtValueRelease(Reply);
    }
    {
        const unsigned char Raw[] = { '-', 'E', 'R', 'R', ' ', 0xffu, '\r', '\n' };
        xbytesview Bytes;
        CHECK(xr_is_complete_raw((void*)Raw, sizeof(Raw)) && xrtGetError() == NULL);
        Reply = xr_decode_raw((void*)Raw, sizeof(Raw));
        Item = field(Reply, "value");
        CHECK(Reply != NULL && text_equals(field(Item, "type"), "error", 5u));
        Item = field(Item, "value");
        CHECK(Item != NULL && xrtValueGetBytes(Item, &Bytes) &&
            Bytes.Size == 5u && memcmp(Bytes.Data, "ERR \xff", 5u) == 0 &&
            xrtGetError() == NULL);
        xrtValueRelease(Reply);
    }

    Input = xrtValueString(XRT_STR_LITERAL("*3\r\n:9223372036854775807\r\n:-9223372036854775808\r\n*1\r\n$-1\r\n"));
    CHECK(Input != NULL && xr_is_complete(Input));
    Reply = xr_decode(Input);
    Item = field(Reply, "value");
    Elements = field(Item, "value");
    CHECK(Elements != NULL && xrtValueCount(Elements) == 3u);
    CHECK(xrtValueGetInt(field(xrtValueArrayAt(Elements, 0), "value"), &Number) && Number == INT64_MAX);
    CHECK(xrtValueGetInt(field(xrtValueArrayAt(Elements, 1), "value"), &Number) && Number == INT64_MIN);
    CHECK(xrtValueType(field(xrtValueArrayAt(field(xrtValueArrayAt(Elements, 2), "value"), 0), "value")) == XVALUE_NULL);
    xrtValueRelease(Reply);
    xrtValueRelease(Input);

    /* 每个严格前缀都只能是未完成帧，不能误判成坏帧或提前交出结果。 */
    {
        const unsigned char Frame[] =
            "*3\r\n+OK\r\n$3\r\nA\0\xff\r\n*2\r\n:17\r\n$-1\r\n";
        size_t length;
        void* Scanner = xr_frame_create();
        CHECK(Scanner != NULL);
        for (length = 0; length < sizeof(Frame) - 1u; ++length) {
            CHECK(!xr_is_complete_raw((void*)Frame, length) &&
                xrtGetError() == NULL);
            CHECK(!xr_frame_check(Scanner, (void*)Frame, length) &&
                xrtGetError() == NULL);
            Reply = xr_decode_raw((void*)Frame, length);
            CHECK(Reply != NULL && xrtValueType(Reply) == XVALUE_NULL &&
                xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
            xrtValueRelease(Reply);
        }
        CHECK(xr_is_complete_raw((void*)Frame, length) && xrtGetError() == NULL);
        CHECK(xr_frame_check(Scanner, (void*)Frame, length) &&
            xrtGetError() == NULL);
        xr_frame_destroy(Scanner);
        Reply = xr_decode_raw((void*)Frame, length);
        CHECK(Reply != NULL && xrtValueType(Reply) == XVALUE_OBJECT &&
            text_equals(field(Reply, "rest"), "", 0u) && xrtGetError() == NULL);
        Elements = field(field(Reply, "value"), "value");
        CHECK(Elements != NULL && xrtValueCount(Elements) == 3u &&
            text_equals(field(xrtValueArrayAt(Elements, 0), "value"), "OK", 2u));
        Item = field(xrtValueArrayAt(Elements, 1), "value");
        {
            xbytesview bytes;
            CHECK(Item != NULL && xrtValueGetBytes(Item, &bytes) &&
                bytes.Size == 3u && bytes.Data[0] == 'A' &&
                bytes.Data[1] == 0u && bytes.Data[2] == 0xffu);
        }
        Item = field(xrtValueArrayAt(Elements, 2), "value");
        CHECK(Item != NULL && xrtValueCount(Item) == 2u &&
            xrtValueGetInt(field(xrtValueArrayAt(Item, 0), "value"), &Number) &&
            Number == 17 &&
            xrtValueType(field(xrtValueArrayAt(Item, 1), "value")) == XVALUE_NULL);
        xrtValueRelease(Reply);
    }

    /* 大数组逐项送入：增量检查器不能反复重扫已完成的前缀。 */
    {
        const size_t count = 10000u;
        char header[32];
        int headerSize = snprintf(header, sizeof(header), "*%u\r\n", (unsigned)count);
        size_t frameSize = (size_t)headerSize + count * 4u;
        char* frame = xrtMalloc(frameSize);
        void* scanner = xr_frame_create();
        size_t part;
        CHECK(headerSize > 0 && frame != NULL && scanner != NULL);
        memcpy(frame, header, (size_t)headerSize);
        for (part = 0u; part < count; ++part)
            memcpy(frame + headerSize + part * 4u, ":1\r\n", 4u);
        for (part = 1u; part <= count; ++part) {
            CHECK(xr_frame_check(scanner, frame,
                (size_t)headerSize + part * 4u) == (part == count));
            CHECK(xrtGetError() == NULL);
        }
        CHECK(xr_is_complete_raw(frame, frameSize) && xrtGetError() == NULL);
        xr_frame_destroy(scanner);
        xrtFree(frame);
    }

    {
        const char Invalid[] = "*2\r\n+OK\r\n?\r\n";
        void* scanner = xr_frame_create();
        CHECK(scanner != NULL &&
            !xr_frame_check(scanner, (void*)Invalid, sizeof(Invalid) - 1u) &&
            xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xr_frame_destroy(scanner);
    }

    {
        static const char* const Frames[] = {
            "+OK\r\n", "-ERR\r\n", ":+3\r\n", "$0\r\n\r\n",
            "$-1\r\n", "*0\r\n", "*1\r\n*1\r\n:3\r\n",
            "+OK\r\n$1\r\na\r\n", "$-2\r\n",
            ":9223372036854775808\r\n", "$2\r\nab\rx"
        };
        size_t caseIndex;
        for (caseIndex = 0u; caseIndex < sizeof(Frames) / sizeof(Frames[0]);
             ++caseIndex) {
            const char* frame = Frames[caseIndex];
            size_t length, size = strlen(frame);
            char first[64], second[64];
            void* scanner = xr_frame_create();
            CHECK(scanner != NULL && size < sizeof(first));
            for (length = 0u; length <= size; ++length) {
                char* relocated = (length & 1u) ? first : second;
                bool expected = xr_is_complete_raw((void*)frame, length);
                const xerror* oldError = xrtGetError();
                bool oldFailed = oldError != NULL;
                xerrkind oldKind = oldFailed ? xrtErrorKind(oldError) : 0;
                memcpy(relocated, frame, length);
                bool actual = xr_frame_check(scanner, relocated, length);
                const xerror* newError = xrtGetError();
                CHECK(actual == expected && (newError != NULL) == oldFailed);
                if (oldFailed) CHECK(xrtErrorKind(newError) == oldKind);
            }
            xr_frame_destroy(scanner);
        }
    }

    /* 不完整回复不报错；非法长度和非法换行则保留结构化错误。 */
    Input = xrtValueString(XRT_STR_LITERAL("$5\r\nabc"));
    CHECK(Input != NULL && !xr_is_complete(Input) && xrtGetError() == NULL);
    xrtValueRelease(Input);
    Input = xrtValueString(XRT_STR_LITERAL("$9223372036854775808\r\n"));
    CHECK(Input != NULL && !xr_is_complete(Input) &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(Input);
    Input = xrtValueString(XRT_STR_LITERAL("+bad\n"));
    CHECK(Input != NULL && !xr_is_complete(Input) &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(Input);
    Input = xrtValueString(XRT_STR_LITERAL("*2\r\n+OK\r\n"));
    CHECK(Input != NULL && !xr_is_complete(Input) && xrtGetError() == NULL);
    xrtValueRelease(Input);
    Input = xrtValueString(XRT_STR_LITERAL("$-2\r\n"));
    CHECK(Input != NULL && !xr_is_complete(Input) &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(Input);
    Input = xrtValueString(XRT_STR_LITERAL("*100001\r\n"));
    CHECK(Input != NULL && !xr_is_complete(Input) &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    {
        void* scanner = xr_frame_create();
        CHECK(scanner != NULL &&
            !xr_frame_check(scanner, "*100001\r\n", 9u) &&
            xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xr_frame_destroy(scanner);
    }
    xrtValueRelease(Input);
    for (i = 0; i < 65; ++i) memcpy(Deep + i * 4, "*1\r\n", 4u);
    memcpy(Deep + 65 * 4, "+X\r\n", 4u);
    Input = xrtValueString((xstrview){ Deep, 65u * 4u + 4u });
    CHECK(Input != NULL && !xr_is_complete(Input) &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    {
        void* scanner = xr_frame_create();
        CHECK(scanner != NULL &&
            !xr_frame_check(scanner, Deep, 65u * 4u + 4u) &&
            xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xr_frame_destroy(scanner);
    }
    xrtValueRelease(Input);

    memset(LongText, 'x', sizeof(LongText));
    Args = xrtValueArray();
    CHECK(Args != NULL && append_text(Args, "SET", 3u) &&
        append_text(Args, LongText, sizeof(LongText)));
    Encoded = xr_encode(Args);
    CHECK(Encoded != NULL && xrtValueGetString(Encoded, &Text) &&
        Text.Size > sizeof(LongText) &&
        memcmp(Text.Data + Text.Size - sizeof(LongText) - 2u,
            LongText, sizeof(LongText)) == 0);
    xrtValueRelease(Encoded);
    xrtValueRelease(Args);

    Input = xrtValueString(XRT_STR_LITERAL("+PONG\r\n"));
    CHECK(Input != NULL);
    for (i = 0; i < 1000; ++i) {
        Reply = xr_decode(Input);
        CHECK(Reply != NULL && text_equals(field(field(Reply, "value"), "value"), "PONG", 4u));
        xrtValueRelease(Reply);
    }
    xrtValueRelease(Input);
    CHECK(test_mutated_frames() == 0);
    puts("redis C boundaries: PASS");
    return 0;
}
