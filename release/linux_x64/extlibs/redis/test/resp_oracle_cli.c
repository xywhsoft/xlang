/* 测试专用十六进制协议：保留NUL/非UTF8，不借JSON字符串替被测结果转义。 */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_BUFFER
#define XRT_MODULE_UNICODE
#include "xrt.h"
#include "../include/redis_native.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static xvalue* field(xvalue* value, cstr name)
{
    return xrtValueObjectGet(value, (xstrview){name, strlen(name)});
}

static bool blob(xvalue* value)
{
    xstrview text;
    xbytesview bytes;
    if (xrtValueType(value) == XVALUE_STRING && xrtValueGetString(value, &text)) {
        putchar('S'); bytes = (xbytesview){(cbytes)text.Data, text.Size};
    } else if (xrtValueType(value) == XVALUE_BYTES && xrtValueGetBytes(value, &bytes)) {
        putchar('B');
    } else return false;
    for (size_t i = 0u; i < bytes.Size; ++i) printf("%02x", (unsigned)bytes.Data[i]);
    return true;
}

static bool tagged(xvalue* value, unsigned depth)
{
    xstrview type;
    xvalue* payload = field(value, "value");
    if (depth >= 64u || !xrtValueGetString(field(value, "type"), &type)) return false;
    if (type.Size == 5u && memcmp(type.Data, "array", 5u) == 0) {
        if (xrtValueType(payload) != XVALUE_ARRAY) return false;
        putchar('[');
        for (size_t i = 0u; i < xrtValueCount(payload); ++i) {
            if (!tagged(xrtValueArrayAt(payload, (int64)i), depth + 1u)) return false;
            putchar(';');
        }
        putchar(']');
    } else if (type.Size == 7u && memcmp(type.Data, "integer", 7u) == 0) {
        int64 number;
        if (!xrtValueGetInt(payload, &number)) return false;
        printf("I%" PRId64, (int64_t)number);
    } else if (type.Size == 4u && memcmp(type.Data, "null", 4u) == 0) {
        if (xrtValueType(payload) != XVALUE_NULL) return false;
        putchar('N');
    } else {
        char code = type.Size == 6u && memcmp(type.Data, "simple", 6u) == 0 ? '+' :
            type.Size == 5u && memcmp(type.Data, "error", 5u) == 0 ? '-' :
            type.Size == 4u && memcmp(type.Data, "bulk", 4u) == 0 ? '$' : 0;
        if (!code) return false;
        putchar(code);
        if (!blob(payload)) return false;
    }
    return true;
}

static char status(bool complete)
{
    const xerror* error = xrtGetError();
    if (error != NULL) return !complete && xrtErrorKind(error) == XERR_VALUE ? 'E' : '?';
    return complete ? 'C' : 'I';
}

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->OverflowCount == after.OverflowCount && before->UnderflowCount == after.UnderflowCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount;
}

static bool inspect(unsigned char* data, size_t size)
{
    unsigned char relocated[2][4096];
    char states[4097];
    xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    void* frame = xr_frame_create();
    if (frame == NULL) return false;
    for (size_t i = 0u; i <= size; ++i) states[i] = status(xr_is_complete_raw(data, i));
    fwrite(states, 1u, size + 1u, stdout); putchar('\t');
    /* 同一追加前缀交替搬家，检查状态只记偏移，不暗中借用旧缓冲地址。 */
    for (size_t i = 0u; i <= size; ++i) {
        unsigned char* next = relocated[i & 1u];
        memcpy(next, data, i);
        states[i] = status(xr_frame_check(frame, next, i));
    }
    fwrite(states, 1u, size + 1u, stdout); putchar('\t');
    xr_frame_destroy(frame);
    xvalue* reply = xr_decode_raw(data, size);
    bool okay;
    if (states[size] == 'C') {
        okay = xrtGetError() == NULL && xrtValueType(reply) == XVALUE_OBJECT &&
            tagged(field(reply, "value"), 0u);
        putchar('|');
        okay = okay && blob(field(reply, "rest"));
    } else {
        okay = reply != NULL && xrtValueType(reply) == XVALUE_NULL &&
            xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE;
        putchar('-');
    }
    xrtValueRelease(reply);
    putchar('\n');
    return balanced(&before) && okay && !ferror(stdout);
}

int main(void)
{
    char line[8195];
    unsigned char data[4096];
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE, "test.redis", 0, "warm diagnostics"); xrtClearError();
    while (fgets(line, sizeof(line), stdin)) {
        size_t count = strcspn(line, "\r\n");
        if (count > sizeof(data) * 2u || count % 2u || line[count] != '\n') return 2;
        for (size_t i = 0u; i < count; ++i) {
            int digit = line[i] >= '0' && line[i] <= '9' ? line[i] - '0' :
                line[i] >= 'a' && line[i] <= 'f' ? line[i] - 'a' + 10 : -1;
            if (digit < 0) return 2;
            if (i % 2u) data[i / 2u] |= (unsigned char)digit;
            else data[i / 2u] = (unsigned char)(digit << 4);
        }
        if (!inspect(data, count / 2u)) return 3;
    }
    return ferror(stdin) || ferror(stdout) ? 2 : 0;
}

#ifndef XR_TEST_NATIVE
#define XR_TEST_NATIVE "../redis_native.c"
#endif
#include XR_TEST_NATIVE
