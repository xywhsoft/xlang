#define XRT_MODULE_ASN1_DER
#define XRT_MODULE_BUFFER_HEX
#define XRT_MODULE_VALUE
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/pkcs_native.h"
#include <stdio.h>
#include <string.h>

static unsigned cases, failures;

static void check(bool ok, const char* text, unsigned route, unsigned seeded)
{
    ++cases;
    if (!ok) {
        ++failures;
        fprintf(stderr, "pkcs OID diagnostic: text=%s route=%u seeded=%u\n", text, route, seeded);
    }
}

int main(void)
{
    static const struct { const char* text; bool valid; } inputs[] = {
        {"1", false}, {"1/2", false}, {"3.1", false}, {"0.40", false},
        {"1.40", false}, {"2.18446744073709551536", false},
        {"1.2x", false}, {"1.2/3", false}, {"", false}, {"1.", false},
        {"1..2", false}, {"1.02", false}, {"1.2.", false},
        {"1.2.bad", false}, {"1.18446744073709551616", false},
        {"0.0", true}, {"0.39", true}, {"1.0", true}, {"1.39", true},
        {"2.0", true}, {"2.18446744073709551535", true},
        {"1.2.18446744073709551615", true}
    };
    static const unsigned char prefix[] = {0x2a, 0x55, 0x12};
    xmemdebugsnapshot before, after;
    // 诊断槽先预热，不把线程首次使用的保留空间误认为泄漏。
    xrtSetErrorInfo(XERR_IO, "pkcs.test.old", 0, "old diagnostic");
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        for (unsigned route = 0; route < 3; ++route) {
            for (unsigned seeded = 0; seeded < 2; ++seeded) {
                bool ok;
                xrtClearError();
                if (route < 2) {
                    xbuffer output = {0};
                    if (!xrtBufferInit(&output) || !xrtBufferAppend(&output,
                        (xbytesview){prefix, sizeof(prefix)})) return 2;
                    if (seeded) xrtSetErrorInfo(XERR_IO, "pkcs.test.old", 0, "old diagnostic");
                    xstrview text = {inputs[i].text, strlen(inputs[i].text)};
                    ok = route == 0 ? xrtDerOidEncode(text, &output) : xrtDerAppendOid(&output, text);
                    const xerror* error = xrtGetError();
                    bool diagnostic = error && xrtErrorKind(error) != XERR_IO;
                    // 失败须保留旧缓冲；成功路径不额外要求 XRT 清理调用前的旧错误。
                    bool matches = ok == inputs[i].valid && output.Size >= sizeof(prefix) &&
                        memcmp(output.Data, prefix, sizeof(prefix)) == 0 &&
                        (inputs[i].valid ? output.Size > sizeof(prefix) :
                            output.Size == sizeof(prefix) && diagnostic);
                    check(matches, inputs[i].text, route, seeded);
                    xrtBufferUnit(&output);
                } else {
                    if (seeded) xrtSetErrorInfo(XERR_IO, "pkcs.test.old", 0, "old diagnostic");
                    char* result = xpkcs_encode_oid(inputs[i].text);
                    const xerror* error = xrtGetError();
                    ok = inputs[i].valid ? result && result[0] && !error :
                        !result && error && xrtErrorKind(error) != XERR_IO;
                    check(ok, inputs[i].text, route, seeded);
                    xrtFree(result);
                }
                xrtClearError();
            }
        }
    }
    xrtMemDebugSnapshot(&after);
    check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount,
        "allocation counters", 3, 0);
    printf("pkcs OID C: %u checks, %u failures\n", cases, failures);
    return failures ? 1 : 0;
}

#ifdef __TINYC__
#include "../pkcs_native.c"
#endif
