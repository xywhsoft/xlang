#define XRT_MODULE_NUMBER_FORMAT
#include "xrt.h"
#include "../include/units_native.h"
#include <stdio.h>

int main(void)
{
    char* binary = (char*)xu_bytes(1536.0);
    char* decimal = (char*)xu_bytes_decimal(1500.0);
    char* rounded = (char*)xu_bytes(1048575.0);
    char* duration = (char*)xu_duration(3725.0);
    char* short_time = (char*)xu_duration_short(3725.0);
    char* count = (char*)xu_count(1200.0);
    char* rounded_count = (char*)xu_count(999960.0);
    int valid = binary != NULL && decimal != NULL && rounded != NULL &&
        duration != NULL && short_time != NULL && count != NULL && rounded_count != NULL;
    if (valid) {
        printf("binary: %s\n", binary);
        printf("decimal: %s\n", decimal);
        printf("rounded: %s\n", rounded);
        printf("parsed: %.1f\n", xu_parse_bytes("1.5MiB", 6));
        printf("duration: %s\n", duration);
        printf("short: %s\n", short_time);
        printf("seconds: %.1f\n", xu_parse_duration("1h2m5s", 6));
        printf("count: %s\n", count);
        printf("rounded count: %s\n", rounded_count);
        puts(xu_is_pow2(64) ? "power of two: true" : "power of two: false");
    }
    xrtFree(binary);
    xrtFree(decimal);
    xrtFree(rounded);
    xrtFree(duration);
    xrtFree(short_time);
    xrtFree(count);
    xrtFree(rounded_count);
    return valid ? 0 : 1;
}

/* 单文件运行由 TCC 编译实现；普通 C 构建仍单独链接包源码。 */
#ifdef __TINYC__
#include "../units_native.c"
#endif
