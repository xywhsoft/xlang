#define XRT_MODULE_CRYPTO_SHA1
#define XRT_MODULE_TIME
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/totp_native.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "totp C test failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int same(void* result, const char* expected)
{
    int valid = result != NULL && strcmp((const char*)result, expected) == 0;
    xrtFree(result);
    return valid;
}

#if defined(__linux__)
static int fail_clock_once;

int __real_clock_gettime(clockid_t clock_id, struct timespec* value);

int __wrap_clock_gettime(clockid_t clock_id, struct timespec* value)
{
    if (fail_clock_once) {
        fail_clock_once = 0;
        errno = EIO;
        return -1;
    }
    return __real_clock_gettime(clock_id, value);
}
#endif

int main(void)
{
    static const char* secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
    static const char* hotp[] = {"755224", "287082", "359152", "969429",
        "338314", "254676", "287922", "162583", "399871", "520489"};
    static const char* long_secret =
        "AEBAGBAFAYDQQCIKBMGA2DQPCAIREEYUCULBOGAZDINRYHI6D4QCCIRDEQSSMJZ"
        "IFEVCWLBNFYXTAMJSGM2DKNRXHA4TUOZ4HU7D6QCBIJBUIRKGI5EESSSLJRGU4T2Q";
    char* first;
    char* second;
    char max_secret[257];
    int i;

    /* RFC 4226 的全部十个 6 位测试向量。 */
    for (i = 0; i < 10; ++i)
        CHECK(same(xtotp_hotp(secret, 32, i, 6), hotp[i]));
    /* RFC 6238 的 8 位 SHA-1 向量；长密钥覆盖 HMAC 的预哈希分支。 */
    CHECK(same(xtotp_generate_at(secret, 32, 59, 30, 8), "94287082"));
    CHECK(same(xtotp_generate_at(secret, 32, 1111111109, 30, 8), "07081804"));
    CHECK(same(xtotp_generate_at(secret, 32, 29, 30, 6), "755224"));
    CHECK(same(xtotp_generate_at(secret, 32, 30, 30, 6), "287082"));
    CHECK(same(xtotp_generate_at(secret, 32, 60, 30, 6), "359152"));
    CHECK(same(xtotp_hotp(secret, 32, 0, 9), "284755224"));
    CHECK(same(xtotp_hotp(long_secret, (long long)strlen(long_secret), 1, 6), "999505"));
    /* 最大密钥与计数上界的固定向量由独立 Python HMAC-SHA1 计算。 */
    memset(max_secret, 'A', 256u);
    max_secret[256] = '\0';
    CHECK(same(xtotp_generate_at(max_secret, 256, LLONG_MAX, 1, 9), "852875294"));
    CHECK(xtotp_verify_at(max_secret, 256, "852875294", 9,
        LLONG_MAX, 1, 9, 10) == 1);
    CHECK(xtotp_hotp(max_secret, 257, 0, 6) == NULL);
    first = (char*)xtotp_hotp("MY", 2, 0, 6);
    second = (char*)xtotp_hotp("MY======", 8, 0, 6);
    CHECK(first != NULL && second != NULL && strcmp(first, second) == 0);
    xrtFree(first);
    xrtFree(second);
    first = (char*)xtotp_hotp("my", 2, 0, 6);
    second = (char*)xtotp_hotp("MY", 2, 0, 6);
    CHECK(first != NULL && second != NULL && strcmp(first, second) == 0);
    xrtFree(first);
    xrtFree(second);

    first = (char*)xtotp_hotp(secret, 32, 0, 6);
    second = (char*)xtotp_hotp(secret, 32, 1, 6);
    CHECK(first != NULL && second != NULL && first != second);
    CHECK(strcmp(first, "755224") == 0 && strcmp(second, "287082") == 0);
    xrtFree(first);
    xrtFree(second);

    CHECK(xtotp_verify_at(secret, 32, "287082", 6, 59, 30, 6, 0) == 1);
    CHECK(xtotp_verify_at(secret, 32, "755224", 6, 59, 30, 6, 0) == 0);
    CHECK(xtotp_verify_at(secret, 32, "755224", 6, 59, 30, 6, 1) == 1);
    CHECK(xtotp_verify_at(secret, 32, "359152", 6, 59, 30, 6, 1) == 1);
    CHECK(xtotp_verify_at(secret, 32, "359152", 6, 59, 30, 6, 0) == 0);
    CHECK(xtotp_verify_at(secret, 32, "999999", 6, 59, 30, 6, 10) == 0);
    CHECK(xtotp_verify_at(secret, 32, "755224", 6, 0, 30, 6, 1) == 1);
    CHECK(xtotp_verify_at(secret, 32, "287082", 6, 0, 30, 6, 1) == 1);
    CHECK(xtotp_verify_at(secret, 32, "287082", 5, 59, 30, 6, 0) == 0);
    CHECK(xtotp_verify_at(secret, 32, "28x082", 6, 59, 30, 6, 0) == 0);
    CHECK(xtotp_verify_at(secret, 32, "28\0" "082", 6, 59, 30, 6, 0) == 0);
    CHECK(xtotp_verify_at(secret, 32, NULL, 6, 59, 30, 6, 0) == 0);

    CHECK(xtotp_hotp("MZ", 2, 0, 6) == NULL); /* 非零尾部填充位 */
    CHECK(xtotp_hotp("M", 1, 0, 6) == NULL);
    CHECK(xtotp_hotp("MY=", 3, 0, 6) == NULL);
    CHECK(xtotp_hotp("MY=======", 9, 0, 6) == NULL);
    CHECK(xtotp_hotp("MY======A", 9, 0, 6) == NULL);
    CHECK(xtotp_hotp("MY  ", 4, 0, 6) == NULL);
    CHECK(xtotp_hotp("MY\0", 3, 0, 6) == NULL);
    CHECK(xtotp_hotp(NULL, 1, 0, 6) == NULL);
    CHECK(xtotp_hotp(secret, 32, -1, 6) == NULL);
    CHECK(xtotp_hotp(secret, 32, 0, 5) == NULL);
    CHECK(xtotp_hotp(secret, 32, 0, 10) == NULL);
    CHECK(xtotp_generate_at(secret, 32, -1, 30, 6) == NULL);
    CHECK(xtotp_generate_at(secret, 32, 1, 0, 6) == NULL);
    CHECK(xtotp_verify_at(secret, 32, "755224", 6, 0, 30, 6, 11) == -1);
    CHECK(xtotp_verify_at("MZ", 2, "000000", 6, 0, 30, 6, 0) == -1);

    /* 失败后的成功调用不得留下前一次的线程错误。 */
    CHECK(xtotp_hotp("MZ", 2, 0, 6) == NULL && xrtGetError() != NULL);
    CHECK(same(xtotp_hotp(secret, 32, 0, 6), "755224"));
    CHECK(xrtGetError() == NULL);
    CHECK(xtotp_generate_at(secret, 32, -1, 30, 6) == NULL && xrtGetError() != NULL);
    CHECK(same(xtotp_generate_at(secret, 32, 59, 30, 8), "94287082"));
    CHECK(xrtGetError() == NULL);
    CHECK(xtotp_verify_at("MZ", 2, "000000", 6, 59, 30, 6, 0) == -1 &&
        xrtGetError() != NULL);
    CHECK(xtotp_verify_at(secret, 32, "287082", 6, 59, 30, 6, 0) == 1);
    CHECK(xrtGetError() == NULL);
    CHECK(xtotp_verify_at("MZ", 2, "000000", 6, 59, 30, 6, 0) == -1);
    CHECK(xtotp_verify_at(secret, 32, "999999", 6, 59, 30, 6, 0) == 0);
    CHECK(xrtGetError() == NULL);

    /* 输出分配失败不能伪装成非法密钥，且恢复后可继续生成。 */
    xrtClearError();
    CHECK(xrtMemDebugFailAfter(0u));
    first = (char*)xtotp_hotp(secret, 32, 0, 6);
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(first == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtClearError();
    CHECK(same(xtotp_hotp(secret, 32, 0, 6), "755224"));

#if defined(__linux__)
    /* 非法参数应在读取墙钟前失败，时钟故障不能遮蔽参数错误。 */
    fail_clock_once = 1;
    CHECK(xtotp_generate(secret, 32, 0, 6) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(fail_clock_once == 1);
    fail_clock_once = 1;
    CHECK(xtotp_verify(secret, 32, "000000", 6, 30, 6, 11) == -1);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(fail_clock_once == 1);

    /* 墙钟失败不能被解释成 Unix 纪元时间，也不能丢失 XRT 的 IO 错误。 */
    fail_clock_once = 1;
    CHECK(xtotp_generate(secret, 32, 30, 6) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_IO);
    fail_clock_once = 1;
    CHECK(xtotp_verify(secret, 32, "000000", 6, 30, 6, 0) == -1);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_IO);
    first = (char*)xtotp_generate(secret, 32, 30, 6);
    CHECK(first != NULL && strlen(first) == 6u);
    xrtFree(first);
    CHECK(xrtGetError() == NULL);
#endif

    puts("totp C boundaries: PASS");
    return 0;
}
