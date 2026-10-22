#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/pkcs_native.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "pkcs C test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

/* 跨 HEX、DER 和文本缓冲的所有权在每个失败点都必须闭合。 */
static int test_text_allocations(void)
{
    static const char* expected[] = {
        "06092a864886f70d010101", "1.2.840.113549.1.1.1",
        "2a864886f70d010101", "0201010500"
    };
    unsigned operation;
    for (operation = 0; operation < 4u; ++operation) {
        unsigned failure, triggered_count = 0u;
        int completed = 0;
        for (failure = 0; failure < 64u; ++failure) {
            xmemdebugsnapshot before, after;
            char* result = NULL;
            bool triggered;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            CHECK(xrtMemDebugFailAfter(failure));
            switch (operation) {
            case 0: result = xpkcs_encode_oid("1.2.840.113549.1.1.1"); break;
            case 1: result = xpkcs_decode_oid("06092a864886f70d010101"); break;
            case 2: result = xpkcs_get_content("06092a864886f70d010101"); break;
            default: result = xpkcs_concat("020101", "0500"); break;
            }
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                ++triggered_count;
                CHECK(result == NULL && xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            } else {
                completed = result != NULL && strcmp(result, expected[operation]) == 0 &&
                    xrtGetError() == NULL;
            }
            xrtFree(result);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            CHECK(after.LiveCount == before.LiveCount &&
                after.LiveBytes == before.LiveBytes);
            if (!triggered) break;
        }
        CHECK(triggered_count > 0u && completed);
    }
    return 0;
}

int main(void)
{
    char* integer = (char*)xpkcs_encode_int(-128);
    char* zero = (char*)xpkcs_encode_int(0);
    char* oid = (char*)xpkcs_encode_oid("1.2.840.113549.1.1.1");
    char* null_value = (char*)xpkcs_encode_null();
    char* content;
    char* sequence;
    char* decoded;
    char* saved;
    int64_t value = 77;
    int i;
    CHECK(integer && zero && oid && null_value);
    CHECK(strcmp(integer, "020180") == 0);
    CHECK(strcmp(zero, "020100") == 0);
    CHECK(xpkcs_decode_int(integer, &value) && value == -128);
    CHECK(xpkcs_decode_int(zero, &value) && value == 0);
    CHECK(strcmp(oid, "06092a864886f70d010101") == 0);
    decoded = (char*)xpkcs_decode_oid(oid);
    CHECK(decoded && strcmp(decoded, "1.2.840.113549.1.1.1") == 0);
    xrtFree(decoded);

    content = (char*)xpkcs_concat(oid, null_value);
    CHECK(content && strcmp(content, "06092a864886f70d0101010500") == 0);
    sequence = (char*)xpkcs_wrap_sequence(content);
    CHECK(sequence && strcmp(sequence, "300d06092a864886f70d0101010500") == 0);
    CHECK(xpkcs_get_tag(sequence) == 0x30 && xpkcs_get_length(sequence) == 13);
    CHECK(xpkcs_der_len(sequence) == 15 && xpkcs_next_offset(sequence, 0) == 15);
    decoded = (char*)xpkcs_get_content(sequence);
    CHECK(decoded && strcmp(decoded, content) == 0);
    xrtFree(decoded);
    decoded = (char*)xpkcs_tag_name(sequence);
    CHECK(decoded && strcmp(decoded, "SEQUENCE") == 0);
    xrtFree(decoded);

    /* 独立结果可在任意后续调用后继续使用，不存在固定数量的复用槽。 */
    saved = (char*)xpkcs_encode_int(12345);
    CHECK(saved && strcmp(saved, "02023039") == 0);
    for (i = 0; i < 128; ++i) {
        char* next = (char*)xpkcs_encode_int(i);
        CHECK(next && strcmp(saved, "02023039") == 0);
        xrtFree(next);
    }
    xrtFree(saved);

    value = 77;
    CHECK(!xpkcs_decode_int("0202007f", &value) && value == 77);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(!xpkcs_decode_int("0500", &value));
    CHECK(!xpkcs_decode_int("020100", NULL));
    CHECK(xpkcs_decode_int("020100", &value) && value == 0);
    decoded = (char*)xpkcs_encode_int(INT64_MIN);
    CHECK(decoded && xpkcs_decode_int(decoded, &value) && value == INT64_MIN);
    xrtFree(decoded);
    decoded = (char*)xpkcs_encode_int(INT64_MAX);
    CHECK(decoded && xpkcs_decode_int(decoded, &value) && value == INT64_MAX);
    xrtFree(decoded);
    CHECK(xpkcs_encode_oid("1.2.bad") == NULL);
    CHECK(xpkcs_concat("abc", "00") == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xpkcs_wrap_set("0500020101") == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    decoded = (char*)xpkcs_wrap_set("020100");
    CHECK(decoded && strcmp(decoded, "3103020100") == 0);
    xrtFree(decoded);
    /* 同标签子项也按完整 DER 字节排序；SEQUENCE 则保留输入顺序。 */
    CHECK(xpkcs_wrap_set("020101020100") == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    decoded = (char*)xpkcs_wrap_set("020100020101");
    CHECK(decoded && strcmp(decoded, "3106020100020101") == 0);
    xrtFree(decoded);
    decoded = (char*)xpkcs_wrap_sequence("020101020100");
    CHECK(decoded && strcmp(decoded, "3006020101020100") == 0);
    xrtFree(decoded);
    CHECK(xpkcs_wrap_sequence("0201") == NULL);
    CHECK(xpkcs_get_tag("zz") == -1);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xpkcs_der_len("0201000500") == -1);
    CHECK(xpkcs_next_offset("0201010500", 0) == 3);
    CHECK(xpkcs_next_offset("0201010500", 3) == 5);
    CHECK(xpkcs_next_offset("0201010500", 1) == -1);
    CHECK(xpkcs_next_offset("0201010500", -1) == -1);
    /* 目标子项有效也不能掩盖后续截断或非规范 SET。 */
    CHECK(xpkcs_next_offset("0201010201", 0) == -1);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xpkcs_next_offset("0201013106020101020100", 0) == -1);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xpkcs_next_offset("0201010500", 0) == 3 && xrtGetError() == NULL);

    /* 构造期间的 OOM 必须保留内存错误，不能伪装成 DER 格式错误。 */
    {
        unsigned failure;
        unsigned injected = 0u;
        for (failure = 0u; failure < 24u; ++failure) {
            int triggered;
            xrtClearError();
            CHECK(xrtMemDebugFailAfter(failure));
            decoded = (char*)xpkcs_wrap_sequence("020100");
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                CHECK(decoded == NULL && xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++injected;
            } else CHECK(decoded && strcmp(decoded, "3003020100") == 0);
            xrtFree(decoded);
        }
        CHECK(injected >= 2u);
        xrtClearError();
    }

    decoded = (char*)xpkcs_encode_octet("");
    CHECK(decoded && strcmp(decoded, "0400") == 0);
    xrtFree(decoded);
    decoded = (char*)xpkcs_encode_bitstring("aabb");
    CHECK(decoded && strcmp(decoded, "030300aabb") == 0);
    xrtFree(decoded);
    decoded = (char*)xpkcs_concat("0102", "0304");
    CHECK(decoded && strcmp(decoded, "01020304") == 0);
    xrtFree(decoded);

    xrtFree(sequence);
    xrtFree(content);
    xrtFree(null_value);
    xrtFree(oid);
    xrtFree(zero);
    xrtFree(integer);
    CHECK(test_text_allocations() == 0);
    puts("pkcs C boundaries: PASS");
    return 0;
}
