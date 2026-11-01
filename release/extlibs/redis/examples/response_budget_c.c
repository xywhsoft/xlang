#define XRT_MODULE_BUFFER
#define XRT_MODULE_UNICODE
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../include/redis_native.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const struct { const char* label; const char* header; bool invalid; } cases[] = {
        {"single_pending", "*1\r\n$67108847\r\n", false},
        {"siblings_pending", "*2\r\n$67108844\r\n", false},
        {"siblings_overflow", "*2\r\n$67108845\r\n", true},
        {"nested_overflow", "*2\r\n*2\r\n$67108838\r\n", true}
    };
    for (size_t at = 0u; at < sizeof(cases) / sizeof(cases[0]); ++at) {
        /* 原生输入只有这几个帧头字节，不按声明长度申请内存。 */
        bool complete = xr_is_complete_raw((void*)cases[at].header, strlen(cases[at].header));
        const xerror* error = xrtGetError();
        bool ok = !complete && (cases[at].invalid ? error && xrtErrorKind(error) == XERR_VALUE &&
            strcmp(xrtErrorMessage(error), "RESP reply is malformed") == 0 : !error);
        xrtClearError();
        if (!ok) { fputs("response budget mismatch\n", stderr); return 1; }
        printf("%s=%s\n", cases[at].label, cases[at].invalid ? "rejected" : "waiting");
    }
    return 0;
}

/* 单文件 C 范例可直接使用 xl run，不要求用户另行配置原生源文件。 */
#include "../redis_native.c"
