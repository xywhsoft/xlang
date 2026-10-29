#include <stdio.h>
#include <string.h>
#include "volc_core_native.h"

/* 与 XLang 范例使用同一组假凭据和固定日期；只预览签名，不联网。 */
int main(void)
{
    const char* preview = x_volc_core_sign_preview_sts(
        "AKID", "SECRET", "trn:iam::123456789:role/demo",
        "demo-session", 900, "20240102T030405Z");
    if (preview == NULL || strstr(preview, "AssumeRole") == NULL ||
        strstr(preview, "20240102T030405Z") == NULL ||
        strstr(preview, "\"authorization\"") == NULL) {
        fputs("volc.sts offline preview failed\n", stderr);
        return 1;
    }
    puts("volc.sts offline preview: ok");
    return 0;
}
