#define XRT_MODULE_ALL
#include "xrt.h"
#include "../json5_native.h"
#include <stdio.h>
#include <string.h>

static int hex_digit(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* 十六进制逐行输入避免 shell 编码影响参考实现对照。 */
int main(void)
{
    char line[8194];
    char bytes[4096];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t length = strcspn(line, "\r\n");
        size_t i;
        if (length % 2u != 0u || length / 2u > sizeof(bytes)) return 2;
        for (i = 0u; i < length / 2u; ++i) {
            int high = hex_digit(line[i * 2u]);
            int low = hex_digit(line[i * 2u + 1u]);
            if (high < 0 || low < 0) return 2;
            bytes[i] = (char)((high << 4) | low);
        }
        {
            xvalue* value = xjson5_parse_len(bytes, length / 2u);
            if (xrtGetError() != NULL || value == NULL) {
                puts("0");
                xrtValueRelease(value);
            } else {
                size_t jsonSize = 0;
                str json = xrtJsonStringify(value, false, &jsonSize);
                putchar('1');
                if (json != NULL) {
                    putchar('\t');
                    for (size_t j = 0; j < jsonSize; ++j)
                        printf("%02x", (unsigned char)json[j]);
                }
                putchar('\n');
                xrtFree(json);
                xrtValueRelease(value);
            }
        }
    }
    return ferror(stdin) ? 2 : 0;
}
