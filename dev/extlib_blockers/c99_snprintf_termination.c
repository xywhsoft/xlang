/* 独立于扩展库/XRT；只检查返回值和有界字节，不读取未终止 C 字符串。 */
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

static int format_v(char* output, size_t size, const char* format, ...)
{
    va_list args; va_start(args, format);
    int result = vsnprintf(output, size, format, args);
    va_end(args); return result;
}

int main(void)
{
    unsigned failures = 0;
    for (unsigned mode = 0; mode < 2; ++mode) {
        struct { unsigned char before[8]; char text[4]; unsigned char after[8]; } record;
        memset(&record, 0xa5, sizeof(record));
        int result = mode ? format_v(record.text, sizeof(record.text), "%s", "abcdef") :
            snprintf(record.text, sizeof(record.text), "%s", "abcdef");
        const char* zero = memchr(record.text, 0, sizeof(record.text));
        bool guards = true;
        for (size_t i = 0; i < sizeof(record.before); ++i)
            if (record.before[i] != 0xa5 || record.after[i] != 0xa5) guards = false;
        printf("%s: result=%d zero=%d guards=%d bytes=%02x%02x%02x%02x\n", mode ? "vsnprintf" : "snprintf",
            result, zero ? (int)(zero-record.text) : -1, (int)guards,
            (unsigned char)record.text[0], (unsigned char)record.text[1], (unsigned char)record.text[2], (unsigned char)record.text[3]);
        if (result != 6 || zero != record.text+3 || memcmp(record.text, "abc", 3) != 0 || !guards) ++failures;
    }
    return failures ? 1 : 0;
}
