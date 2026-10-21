#include <windows.h>
#include <stdint.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xrt.h"
#include "../include/op.h"

int main(void)
{
    unsigned char *pages = VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT,
                                         PAGE_READWRITE);
    char *large = NULL, *hit = NULL, *text = NULL, *miss = NULL;
    DWORD old_access;
    char expected[32];
    char target[20] = "before";
    int number = 0;
    float fraction = 0.0f;
    long long pid = (long long)GetCurrentProcessId();
    int ok = 0;

    if (!pages) return 1;
    large = VirtualAlloc(NULL, 131072, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!large) goto done;
    memset(pages, 0, 8192);
    memset(large, 'a', 70000);
    large[70000] = '\0';
    pages[4094] = 0xDE;
    pages[4095] = 0xAD;
    pages[4096] = 0x7F;
    pages[4097] = 0xEF;
    memcpy(pages + 6000, "hello", 6);
    pages[5000] = 0xFF;
    if (!VirtualProtect(pages + 4096, 4096, PAGE_READONLY, &old_access)) goto done;

    /* 模式跨越两个具有不同页保护属性的可读区域。 */
    snprintf(expected, sizeof(expected), "%lld", (long long)(intptr_t)(pages + 4094));
    hit = xop_find_memory(pid, (long long)(intptr_t)(pages + 4000),
                          (long long)(intptr_t)(pages + 4200), "DE AD ?? EF");
    text = xop_read_string(pid, (long long)(intptr_t)(pages + 6000), 6);
    if (!hit || !text || strcmp(hit, expected) || strcmp(text, "hello")) goto done;
    if (strcmp(hit, expected)) goto done;

    miss = xop_find_memory(pid, (long long)(intptr_t)(pages + 4094),
                           (long long)(intptr_t)(pages + 4096), "DEAD7FEF");
    if (!miss || *miss) goto done;
    xrtFree(miss);
    miss = xop_find_memory(pid, (long long)(intptr_t)pages,
                           (long long)(intptr_t)(pages + 8192), "FEED");
    if (!miss || *miss) goto done;
    if (xop_find_memory(pid, (long long)(intptr_t)pages,
                        (long long)(intptr_t)(pages + 8192), "D?") != NULL) goto done;
    if (xop_read_string(pid, (long long)(intptr_t)(pages + 5000), 2) != NULL) goto done;
    if (xop_read_string(pid, (long long)(intptr_t)pages, 16 * 1024 * 1024 + 1) != NULL)
        goto done;
    {
        char *long_text = xop_read_string(pid, (long long)(intptr_t)large, 70001);
        int complete = long_text && strlen(long_text) == 70000;
        xrtFree(long_text);
        if (!complete) goto done;
    }

    if (!xop_write_int(pid, (long long)(intptr_t)&number, 42) ||
        xop_read_int(pid, (long long)(intptr_t)&number) != 42 || number != 42)
        goto done;
    if (xop_write_int(pid, (long long)(intptr_t)&number, (long long)INT32_MAX + 1) ||
        xop_write_int(pid, (long long)(intptr_t)&number, (long long)INT32_MIN - 1) ||
        number != 42) goto done;
    if (!xop_write_float(pid, (long long)(intptr_t)&fraction, 1.5) ||
        xop_read_float(pid, (long long)(intptr_t)&fraction) != 1.5 || fraction != 1.5f)
        goto done;
    if (xop_write_float(pid, (long long)(intptr_t)&fraction, (double)FLT_MAX * 2.0) ||
        xop_write_float(pid, (long long)(intptr_t)&fraction, 1e-100) ||
        fraction != 1.5f) goto done;
    if (!xop_write_string(pid, (long long)(intptr_t)target, "after") ||
        strcmp(target, "after")) goto done;
    if (!VirtualProtect(pages + 4096, 4096, PAGE_NOACCESS, &old_access)) goto done;
    {
        char *blocked = xop_find_memory(pid, (long long)(intptr_t)(pages + 4000),
                                        (long long)(intptr_t)(pages + 4200),
                                        "DE AD ?? EF");
        int no_false_match = blocked && *blocked == '\0';
        xrtFree(blocked);
        if (!no_false_match) goto done;
    }
    if (xop_read_string(pid, (long long)(intptr_t)(pages + 4094), 4) != NULL)
        goto done;
    ok = 1;

done:
    xrtFree(hit);
    xrtFree(text);
    xrtFree(miss);
    if (large) VirtualFree(large, 0, MEM_RELEASE);
    VirtualFree(pages, 0, MEM_RELEASE);
    if (!ok) return 2;
    puts("op memory scan: ok");
    return 0;
}
