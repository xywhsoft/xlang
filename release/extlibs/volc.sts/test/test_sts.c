#define XRT_MODULE_ALL
#include "xrt.h"
#include "volc_core_native.h"

#include <stdio.h>
#include <string.h>

static int fail(const char* message)
{
    fprintf(stderr, "volc.sts C test failed: %s\n", message);
    return 1;
}

int main(void)
{
    const char* role = "trn:iam::123456789:role/demo";
    const char* date = "20240102T030405Z";
    const char* spec =
        "access_key=AKID\nsecret_key=SECRET\n"
        "role_trn=trn:iam::123456789:role/demo\n"
        "role_session_name=demo-session\nduration_seconds=900\n"
        "x_date=20240102T030405Z\n";
    const char* direct = x_volc_core_sign_preview_sts(
        "AKID", "SECRET", role, "demo-session", 900, date);
    const char* from_spec;

    if (direct == NULL || strstr(direct, "Action=AssumeRole") == NULL ||
        strstr(direct, "DurationSeconds=900") == NULL ||
        strstr(direct, "RoleTrn=trn%3Aiam%3A%3A123456789%3Arole%2Fdemo") == NULL ||
        strstr(direct, "\"x_date\":\"20240102T030405Z\"") == NULL ||
        strstr(direct, "\"authorization\"") == NULL)
        return fail("direct preview fields");

    from_spec = x_volc_core_sign_preview_sts_spec(spec);
    if (from_spec == NULL || strcmp(direct, from_spec) != 0)
        return fail("direct/spec preview mismatch");

    if (strcmp(x_volc_core_sign_preview_sts_spec(
            "access_key=AKID\nsecret_key=SECRET\n"
            "role_trn=trn:iam::123456789:role/demo\n"
            "role_session_name=demo-session\nduration_seconds=bad\n"
            "x_date=20240102T030405Z\n"), "") != 0 ||
        strcmp(x_volc_core_last_error(), "invalid duration_seconds") != 0 ||
        xrtGetError() != NULL)
        return fail("invalid numeric spec must not use default duration");

    if (strcmp(x_volc_core_sign_preview_sts(
            "AKID", "SECRET", role, "demo-session", 900,
            "20240230T030405Z"), "") != 0 ||
        strcmp(x_volc_core_last_error(), "invalid x_date") != 0 ||
        xrtGetError() != NULL)
        return fail("invalid date");

    if (strcmp(x_volc_core_sign_preview_sts(
            "AKID", "SECRET", "role\nDurationSeconds=1", "demo-session",
            900, date), "") != 0 ||
        strcmp(x_volc_core_last_error(), "invalid STS role field") != 0)
        return fail("role newline injection");
    if (strcmp(x_volc_core_sign_preview_sts(
            "AKID", "SECRET", role, "demo\rRoleTrn=other", 900,
            date), "") != 0 ||
        strcmp(x_volc_core_last_error(), "invalid STS role field") != 0)
        return fail("session newline injection");
#ifdef _WIN32
    /* Linux 离线测试宿主不链接完整 HTTP 客户端。 */
    if (strcmp(x_volc_core_request_sts(
            "AKID", "SECRET", "role\nDurationSeconds=1", "demo-session",
            900, 1000), "") != 0 ||
        strcmp(x_volc_core_last_error(), "invalid STS role field") != 0)
        return fail("request must reject before sending");
#endif

    if (x_volc_core_sign_preview_sts(
            "AKID", "SECRET", role, "demo-session", 900, date)[0] == '\0' ||
        x_volc_core_last_error()[0] != '\0' || xrtGetError() != NULL)
        return fail("recovery after invalid date");

    puts("volc.sts C boundary: PASS");
    return 0;
}
