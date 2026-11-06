/* 与frame_headers.xl等价；默认不连接网络、不创建文件。 */
#include "../redis_native.c"

static xvalue* payload(xvalue* reply)
{
    return xrtValueObjectGet(xrtValueObjectGet(reply, XRT_STR_LITERAL("value")), XRT_STR_LITERAL("value"));
}

int main(void)
{
    static const char* const integers[] = {":+7\r\n", ":-7\r\n"};
    static const char* const nulls[] = {"$-1\r\n", "*-1\r\n"};
    static const char* const invalid[] = {"$+1\r\n", "*+1\r\n", "$-0\r\n", "*-0\r\n",
        "$-01\r\n", "*-01\r\n", "$-001\r\n", "*1\r\n$-01\r\n"};
    for (size_t i = 0u; i < 2u; ++i) {
        xvalue* reply = xr_decode_raw((void*)integers[i], strlen(integers[i]));
        int64 number;
        bool okay = xrtGetError() == NULL && xrtValueGetInt(payload(reply), &number) && number == (i ? -7 : 7);
        xrtValueRelease(reply);
        if (!okay) return 1;
    }
    puts("integer signs: true");
    xvalue* reply = xr_decode_raw((void*)"$00\r\n\r\n", 7u);
    xstrview text;
    bool okay = xrtGetError() == NULL && xrtValueGetString(payload(reply), &text) && text.Size == 0u;
    xrtValueRelease(reply);
    if (!okay) return 1;
    for (size_t i = 0u; i < 2u; ++i) {
        reply = xr_decode_raw((void*)nulls[i], strlen(nulls[i]));
        okay = xrtGetError() == NULL && xrtValueType(payload(reply)) == XVALUE_NULL;
        xrtValueRelease(reply);
        if (!okay) return 1;
    }
    puts("empty and null: true");
    for (size_t i = 0u; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        bool complete = xr_is_complete_raw((void*)invalid[i], strlen(invalid[i]));
        const xerror* error = xrtGetError();
        if (complete || error == NULL || xrtErrorKind(error) != XERR_VALUE ||
            strcmp(xrtErrorMessage(error), "RESP reply is malformed") != 0) return 1;
        if (xr_is_complete_raw((void*)"$1\r\n", 4u) || xrtGetError() != NULL) return 1;
        reply = xr_decode_raw((void*)"+OK\r\n", 5u);
        okay = xrtGetError() == NULL && xrtValueGetString(payload(reply), &text) &&
            text.Size == 2u && memcmp(text.Data, "OK", 2u) == 0;
        xrtValueRelease(reply);
        if (!okay) return 1;
    }
    puts("invalid lengths: 8");
    puts("recovery: true");
    return 0;
}
