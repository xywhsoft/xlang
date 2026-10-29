#include <stdio.h>
#include <string.h>
#include "../include/volc_core_native.h"

/* 离线验证签名输入，不发出请求，也不需要云端凭据。 */
int main(void)
{
    const char* query = x_volc_core_canonical_query(
        "ListApps", "2020-01-01", "Limit=10\nName=x lang");
    const char* preview = x_volc_core_sign_preview_acep(
        "ListApps", "2020-01-01", "AKID", "SECRET", "", "{}", "20240102T030405Z");
    const char* path_preview = x_volc_core_sign_preview(
        "POST", "open.volcengineapi.com", "/a b/\xe4\xb8\xad\xe6\x96\x87",
        "ACEP", "cn-north-1", "ListApps", "2020-01-01", "AKID", "SECRET",
        "", "application/json", "", "{}", "20240102T030405Z");

    if (strstr(query, "Name=x%20lang") == NULL ||
        strstr(preview, "20240102T030405Z") == NULL ||
        strstr(preview, "\"authorization\"") == NULL ||
        strstr(path_preview, "Signature=b7314969a3890d3f9d541282fc8814269023fc1f6945a7e1ff80a52b465ba485") == NULL) {
        fputs("volc.core signing example failed\n", stderr);
        return 1;
    }
    puts("volc.core extlib ok");
    return 0;
}
