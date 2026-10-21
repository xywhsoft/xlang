/* 测试调用包含副作用，发布配置下也必须实际执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "../include/volc_core_native.h"
static void test_pair_allocation_failure(void);
static void test_thread_isolation(void);
static void test_buffer_bounds(void);
static void test_signed_fields(void);

static int fail(const char* message)
{
    fprintf(stderr, "volc.core C smoke failed: %s\n", message);
    return 1;
}

int main(void)
{
    const char* digest = x_volc_core_sha256_hex("abc");
    const char* mac = x_volc_core_hmac_sha256_hex("key", "The quick brown fox jumps over the lazy dog");
    const char* query = x_volc_core_canonical_query("ListApps", "2020-01-01", "Limit=10\nName=x lang");
    const char* preview = x_volc_core_sign_preview_acep("ListApps", "2020-01-01", "AKID", "SECRET", "", "{}", "20240102T030405Z");

    if (x_volc_core_available() != 1 || x_volc_core_last_status() != 0)
        return fail("availability or initial status");
    if (strcmp(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != 0) return fail("sha256");
    if (strcmp(mac, "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8") != 0) return fail("hmac");
    if (strstr(query, "Action=ListApps") == NULL || strstr(query, "Name=x%20lang") == NULL) return fail("query");
    if (strstr(preview, "authorization") == NULL || strstr(preview, "20240102T030405Z") == NULL) return fail("preview");
    if (strcmp(x_volc_core_sign_preview_acep("ListApps", "2020-01-01", "AKID", "SECRET", "", "{}", "bad"), "") != 0 ||
        strcmp(x_volc_core_last_error(), "invalid x_date") != 0) return fail("malformed date accepted");
    if (strcmp(x_volc_core_sign_preview_acep("ListApps", "2020-01-01", "AKID", "SECRET", "", "{}", "20240230T030405Z"), "") != 0 ||
        strcmp(x_volc_core_last_error(), "invalid x_date") != 0) return fail("impossible date accepted");
    if (xrtGetError() != NULL) return fail("invalid date leaked XRT error");
    {
        const char* current = x_volc_core_sign_preview_acep(
            "ListApps", "2020-01-01", "AKID", "SECRET", "", "{}", "");
        const char* date = strstr(current, "\"x_date\":\"");
        if (date == NULL || strlen(date) < strlen("\"x_date\":\"") + 16u + 1u ||
            date[strlen("\"x_date\":\"") + 16u] != '"' ||
            x_volc_core_last_error()[0] != '\0' || xrtGetError() != NULL) return fail("current UTC date");
    }
    {
        xtime parsed = 0;
        if (xrtTimeParse(XRT_STR_LITERAL("bad"), XRT_STR_LITERAL("%Y%m%dT%H%M%SZ"), &parsed) ||
            xrtGetError() == NULL) return fail("XRT error setup");
        if (strcmp(x_volc_core_sha256_hex("abc"), digest) != 0 || xrtGetError() != NULL)
            return fail("successful call retained stale XRT error");
    }
    test_pair_allocation_failure();
    test_buffer_bounds();
    test_signed_fields();
    test_thread_isolation();
    puts("volc.core C smoke: PASS");
    return 0;
}

#include "../x_volc_core_native.c"

/* 控制字符不能改变规范请求的行结构或授权头。 */
static void test_signed_fields(void)
{
    const char* fields[] = {
        "POST", "open.volcengineapi.com", "/", "ACEP", "cn-north-1",
        "AKID", "token", "application/json"
    };
    size_t i;
    for (i = 0u; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        const char* original = fields[i];
        const char* preview;
        fields[i] = "bad\r\nheader";
        preview = x_volc_core_sign_preview(fields[0], fields[1], fields[2],
            fields[3], fields[4], "ListApps", "2020-01-01", fields[5],
            "SECRET", fields[6], fields[7], "", "{}", "20240102T030405Z");
        assert(strcmp(preview, "") == 0);
        assert(strcmp(x_volc_core_last_error(), "invalid signed field") == 0);
        fields[i] = original;
    }
    fields[1] = "bad\thost";
    assert(strcmp(x_volc_core_sign_preview(fields[0], fields[1], fields[2],
        fields[3], fields[4], "ListApps", "2020-01-01", fields[5],
        "SECRET", fields[6], fields[7], "", "{}", "20240102T030405Z"), "") == 0);
    fields[1] = "bad\x7f";
    assert(strcmp(x_volc_core_sign_preview(fields[0], fields[1], fields[2],
        fields[3], fields[4], "ListApps", "2020-01-01", fields[5],
        "SECRET", fields[6], fields[7], "", "{}", "20240102T030405Z"), "") == 0);
    fields[1] = "open.volcengineapi.com";
    assert(strstr(x_volc_core_sign_preview(fields[0], fields[1], fields[2],
        fields[3], fields[4], "ListApps", "2020-01-01", fields[5],
        "SECRET", fields[6], fields[7], "", "line\nbody", "20240102T030405Z"),
        "authorization") != NULL);
    assert(x_volc_core_last_error()[0] == '\0');
}

/* 正常扩容保留内容，极限长度必须在计算容量前拒绝。 */
static void test_buffer_bounds(void)
{
    xvolc_buf b;
    char text[600];
    memset(text, 'x', sizeof(text));
    xvolc_buf_init(&b);
    assert(xvolc_buf_append_n(&b, text, sizeof(text)));
    assert(b.len == sizeof(text) && b.cap > b.len);
    assert(memcmp(b.data, text, sizeof(text)) == 0 && b.data[b.len] == '\0');
    assert(xvolc_buf_appendf(&b, "-%d", 42));
    assert(strcmp(b.data + sizeof(text), "-42") == 0);
    xvolc_buf_free(&b);

    b.len = SIZE_MAX - 1u;
    assert(!xvolc_buf_append_n(&b, "x", 1u));
    assert(strcmp(x_volc_core_last_error(), "result is too large") == 0);
    assert(!xvolc_buf_appendf(&b, "%s", "x"));
    assert(strcmp(x_volc_core_last_error(), "result is too large") == 0);
    b.len = 0u;
    assert(!xvolc_buf_append_n(&b, "x", SIZE_MAX));
    assert(strcmp(x_volc_core_last_error(), "result is too large") == 0);
}

/* 查询参数构造失败后，不得留下半成品键值或泄漏分配。 */
static void test_pair_allocation_failure(void)
{
    size_t i;

    assert(xvolc_state_get() != NULL);
    for (i = 0; i < 4; ++i) {
        xvolc_pair* pairs = NULL;
        size_t count = 0;
        size_t capacity = 0;
        bool ok;
        assert(xrtMemDebugFailAfter(i));
        ok = xvolc_pairs_add(&pairs, &count, &capacity, "Action", "ListApps") != 0;
        assert(xrtMemDebugFailTriggered() == (i != 3));
        xrtMemDebugFailClear();
        assert(ok == (i == 3));
        if (!ok && pairs != NULL) {
            assert(pairs[0].key == NULL && pairs[0].value == NULL);
        }
        xvolc_pairs_free(pairs, count);
    }
}

typedef struct xvolc_test_thread {
    xevent* ready;
    xevent* resume;
    int isolated;
} xvolc_test_thread;

static int32 xvolc_test_worker(ptr data)
{
    xvolc_test_thread* test = (xvolc_test_thread*)data;
    const char* digest = x_volc_core_sha256_hex("abc");
    xvolc_state_get()->last_status = 207;
    xvolc_set_error("worker error");
    assert(xrtEventSet(test->ready));
    assert(xrtEventWaitFor(test->resume, 5000000) == XWAIT_OK);
    test->isolated = strcmp(x_volc_core_last_error(), "worker error") == 0 &&
        x_volc_core_last_status() == 207 &&
        strcmp(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0;
    return 0;
}

/* 主线程修改诊断后，工作线程仍应看到自己的诊断和返回值。 */
static void test_thread_isolation(void)
{
    xvolc_test_thread test = { 0 };
    xthread* thread;
    test.ready = xrtEventCreate(true, false);
    test.resume = xrtEventCreate(true, false);
    assert(test.ready != NULL && test.resume != NULL);
    thread = xrtThreadCreate(xvolc_test_worker, &test, 0);
    assert(thread != NULL);
    assert(xrtEventWaitFor(test.ready, 5000000) == XWAIT_OK);
    for (int i = 0; i < 16; ++i) {
        assert(x_volc_core_sha256_hex("main") != NULL);
    }
    xvolc_state_get()->last_status = 503;
    xvolc_set_error("main error");
    assert(xrtEventSet(test.resume));
    assert(xrtThreadWait(thread) == XWAIT_OK);
    assert(test.isolated);
    xrtThreadDestroy(thread);
    assert(xrtEventDestroy(test.ready));
    assert(xrtEventDestroy(test.resume));
}
