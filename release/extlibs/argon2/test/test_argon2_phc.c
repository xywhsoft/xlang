#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../include/argon2_native.h"
#include "../vendor/argon2.h"
#include "../../../include/xrt_decl.h"

static unsigned int malformed_count, accepted_count, version_count;
static int failures;

/* 只用32KiB/t=1生成验证夹具；边界大整数仅做元数据解析，绝不执行高成本哈希。 */
static void make_phc(char* encoded, size_t capacity, argon2_type type, uint32_t version)
{
    uint8_t raw[16];
    assert(argon2_hash(1, 32, 1, "pw", 2, "somesalt", 8,
        raw, sizeof(raw), encoded, capacity, type, version) == ARGON2_OK);
}

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.InvalidFreeCount == before->InvalidFreeCount && after.DoubleFreeCount == before->DoubleFreeCount);
    assert(after.OverflowCount == before->OverflowCount && after.UnderflowCount == before->UnderflowCount);
}

static void rejected(const char* encoded, size_t size)
{
    xmemdebugsnapshot before;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    assert(!xargon2_verify("pw", 2, encoded, size, "", 0, "", 0) && !xrtGetError());
    assert(xargon2_needs_rehash(encoded, size, 1, 32, 1) && !xrtGetError());
    char* info = xargon2_extract(encoded, size);
    assert(info && !*info && !xrtGetError());
    xrtFree(info);
    balanced(&before);
    ++malformed_count;
}

static void accepted(const char* encoded, argon2_type type, uint32_t version)
{
    char expected[64];
    snprintf(expected, sizeof(expected), "%u,1,32,1,16,8", (unsigned int)type);
    assert(xargon2_verify("pw", 2, encoded, strlen(encoded), "", 0, "", 0) && !xrtGetError());
    assert(!xargon2_verify("wrong", 5, encoded, strlen(encoded), "", 0, "", 0) && !xrtGetError());
    assert(xargon2_needs_rehash(encoded, strlen(encoded), 1, 32, 1) ==
        (version != ARGON2_VERSION_13) && !xrtGetError());
    char* info = xargon2_extract(encoded, strlen(encoded));
    assert(info && !strcmp(info, expected) && !xrtGetError());
    xrtFree(info);
    ++accepted_count;
}

static void versions(void)
{
    static const uint32_t values[] = {0, 1, 15, 16, 17, 18, 19, 20, 255, UINT32_MAX};
    for (unsigned int type = 0; type < 3; ++type) {
        for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
            char encoded[512];
            make_phc(encoded, sizeof(encoded), (argon2_type)type, values[i]);
            if (values[i] == ARGON2_VERSION_10 || values[i] == ARGON2_VERSION_13) {
                accepted(encoded, (argon2_type)type, values[i]);
            } else {
                xmemdebugsnapshot before;
                xrtClearError();
                xrtMemDebugSnapshot(&before);
                long long match = xargon2_verify("pw", 2, encoded, strlen(encoded), "", 0, "", 0);
                long long needs = xargon2_needs_rehash(encoded, strlen(encoded), 1, 32, 1);
                char* info = xargon2_extract(encoded, strlen(encoded));
                if (match || !needs || !info || *info || xrtGetError()) {
                    fprintf(stderr, "type=%u unsupported version=%u match=%lld needs=%lld info=%s\n",
                        type, values[i], match, needs, info ? info : "(null)");
                    ++failures;
                }
                xrtFree(info);
                balanced(&before);
                ++version_count;
            }
        }
        /* 省略v字段是参考库规定的16版本编码，不是把未知版本改写成19。 */
        char encoded[512];
        make_phc(encoded, sizeof(encoded), (argon2_type)type, ARGON2_VERSION_10);
        char* at = strstr(encoded, "$v=16");
        assert(at);
        memmove(at, at + 5, strlen(at + 5) + 1);
        accepted(encoded, (argon2_type)type, ARGON2_VERSION_10);
    }
}

static void malformed(void)
{
    static const char* const invalid[] = {"", "-1", "+1", "01", "1 ", " 1", "1.0",
        "0x20", "1_0", "4294967296", "4294967297", "4294967315", "4294967328",
        "18446744073709551616", "99999999999999999999999999999999", "0"};
    static const char* const forms[] = {
        "$argon2id$v=19$m=%s,t=1,p=1$c29tZXNhbHQ$AAAAAA",
        "$argon2id$v=19$m=32,t=%s,p=1$c29tZXNhbHQ$AAAAAA",
        "$argon2id$v=19$m=32,t=1,p=%s$c29tZXNhbHQ$AAAAAA",
        "$argon2id$v=%s$m=32,t=1,p=1$c29tZXNhbHQ$AAAAAA"
    };
    char text[4098];
    for (size_t field = 0; field < 4; ++field) {
        for (size_t value = 0; value < sizeof(invalid) / sizeof(invalid[0]); ++value) {
            /* 版本0本来就在上一组专门核查；其余坏格式在所有字段同样拒绝。 */
            if (field == 3 && !strcmp(invalid[value], "0")) continue;
            snprintf(text, sizeof(text), forms[field], invalid[value]);
            rejected(text, strlen(text));
        }
    }
    static const char* const invalid_phc[] = {
        "$ARGON2ID$v=19$m=32,t=1,p=1$c29tZXNhbHQ$AAAAAA",
        "$argon2x$v=19$m=32,t=1,p=1$c29tZXNhbHQ$AAAAAA",
        "$argon2id$v=19$t=1,m=32,p=1$c29tZXNhbHQ$AAAAAA",
        "$argon2id$v=19$m=32,t=1,p=1,p=1$c29tZXNhbHQ$AAAAAA",
        "$argon2id$v=19$m=32,t=1,p=1$c29tZXNhbHQ=$AAAAAA",
        "$argon2id$v=19$m=32,t=1,p=1$c29tZXNhbHR$AAAAAA",
        "$argon2id$v=19$m=32,t=1,p=1$c29tZXNhbHQ$AAAAAB",
        "$argon2id$v=19$m=32,t=1,p=1$c29tZXNhbHQ$AAAAAA=",
        "$argon2id$v=19$m=32,t=1,p=1$c29tZXNhbHQ$AAAAA",
        "$argon2id$v=19$m=32,t=1,p=1$c29tZXNhbHQ$",
        "$argon2id$v=19$m=32,t=1,p=1$$AAAAAA",
        "$argon2id$v=19$m=32,t=1,p=1$c29tZXNhbHQ$AAAAAA$",
        "$argon2id$v=19$m=32,t=1,p=1$c29tZXNhbHQ$AAAAAA\n",
        "$argon2id$v=19$m=32,t=1,p=16777216$c29tZXNhbHQ$AAAAAA"
    };
    for (size_t i = 0; i < sizeof(invalid_phc) / sizeof(invalid_phc[0]); ++i)
        rejected(invalid_phc[i], strlen(invalid_phc[i]));
    make_phc(text, sizeof(text), Argon2_id, ARGON2_VERSION_13);
    size_t size = strlen(text);
    memcpy(text + size, "\0tail", 5);
    rejected(text, size + 5);
    rejected(NULL, 0);
    rejected(NULL, 1);
    static const size_t sizes[] = {0, 1, 4095, 4096, 4097};
    memset(text, 'x', sizeof(text));
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) rejected(text, sizes[i]);
}

/* 解码是否接受与是否执行计算分开：UINT32上界可精确检查，不申请GiB或运行数十亿轮。 */
static void metadata_boundaries(void)
{
    static const struct { const char* phc; const char* info; } cases[] = {
        {"$argon2id$v=19$m=32,t=4294967295,p=1$c29tZXNhbHQ$AAAAAA", "2,4294967295,32,1,4,8"},
        {"$argon2id$v=19$m=4294967295,t=1,p=1$c29tZXNhbHQ$AAAAAA", "2,1,4294967295,1,4,8"},
        {"$argon2id$v=19$m=134217720,t=1,p=16777215$c29tZXNhbHQ$AAAAAA", "2,1,134217720,16777215,4,8"}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char* info = xargon2_extract(cases[i].phc, strlen(cases[i].phc));
        assert(info && !strcmp(info, cases[i].info) && !xrtGetError());
        xrtFree(info);
        assert(xargon2_needs_rehash(cases[i].phc, strlen(cases[i].phc), 1, 32, 1) && !xrtGetError());
    }
}

static int query(const char* encoded, unsigned int api, bool valid)
{
    size_t size = strlen(encoded);
    if (api == 0) return xargon2_verify("pw", 2, encoded, size, "", 0, "", 0) == valid;
    if (api == 1) return xargon2_needs_rehash(encoded, size, 1, 32, 1) == !valid;
    char* info = xargon2_extract(encoded, size);
    int ok = info && (valid ? !strcmp(info, "2,1,32,1,16,8") : !*info);
    xrtFree(info);
    return ok;
}

static void allocations(void)
{
    char valid[512], invalid[512];
    unsigned int failures_seen = 0, combinations = 0;
    make_phc(valid, sizeof(valid), Argon2_id, ARGON2_VERSION_13);
    make_phc(invalid, sizeof(invalid), Argon2_id, ARGON2_VERSION_13);
    char* version = strstr(invalid, "$v=19");
    assert(version);
    version[4] = '7';
    const char* inputs[] = {valid, invalid, "not a PHC"};
    for (unsigned int input = 0; input < 3; ++input) {
        for (unsigned int api = 0; api < 3; ++api) {
            xmemdebugsnapshot before;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            bool completed = false;
            for (unsigned int budget = 0; budget < 16; ++budget) {
                assert(xrtMemDebugFailAfter(budget));
                int ok = query(inputs[input], api, input == 0);
                bool triggered = xrtMemDebugFailTriggered();
                xrtMemDebugFailClear();
                if (triggered) {
                    assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                    ++failures_seen;
                } else {
                    assert(ok && !xrtGetError());
                    completed = true;
                }
                xrtClearError();
                balanced(&before);
                assert(query(inputs[input], api, input == 0) && !xrtGetError());
                balanced(&before);
                if (completed) break;
            }
            assert(completed);
            ++combinations;
        }
    }
    printf("argon2 PHC allocations: combinations=%u failure-points=%u\n", combinations, failures_seen);
}

int main(void)
{
    assert(xrtMemDebugEnable(true));
    versions();
    malformed();
    metadata_boundaries();
    /* 旧代码先报告全部未知版本问题，不让随后空结果假设中断红例统计。 */
    if (!failures) allocations();
    xrtClearError();
    xrtTempUnit(xrtTempCurrent());
    printf("argon2 PHC: accepted=%u unknown-versions=%u malformed=%u metadata=3 failures=%d\n",
        accepted_count, version_count, malformed_count, failures);
    return failures ? 1 : 0;
}
