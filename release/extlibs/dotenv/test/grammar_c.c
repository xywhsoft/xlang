/* 按公开合同构造输入/预期，不复用被测解析器计算答案。 */
#include "../include/xrt_dotenv.h"
#include "xrt.h"

#include <stdio.h>
#include <string.h>

static unsigned validCount, invalidCount, faultRuns, faultCount, failures;

static void require(int ok, const char* label)
{
    if (!ok) {
        if (failures < 12u) fprintf(stderr, "dotenv grammar: %s\n", label);
        ++failures;
    }
}

static int balanced(const xmemdebugsnapshot* a, const xmemdebugsnapshot* b)
{
    return a->LiveCount == b->LiveCount && a->LiveBytes == b->LiveBytes &&
        a->InvalidFreeCount == b->InvalidFreeCount && a->DoubleFreeCount == b->DoubleFreeCount &&
        a->OverflowCount == b->OverflowCount && a->UnderflowCount == b->UnderflowCount &&
        a->UseAfterFreeCount == b->UseAfterFreeCount;
}

/* 每条输入独立建对象；失败时既保留内容，也保留原来的借用地址。 */
static void one(const char* input, size_t size, size_t limit, const char* key,
    const char* expected, int code, size_t line, int budget)
{
    xmemdebugsnapshot before, after;
    xrtdotenv* config;
    const char* saved;
    int ok, triggered = 0, actualCode;
    size_t actualLine;
    char copy[2048];
    require(size <= sizeof(copy), "fixture too large");
    if (size > sizeof(copy)) return;
    memcpy(copy, input, size);
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    config = xrtDotenvCreate();
    if (config == NULL || !xrtDotenvSet(config, "KEEP", "old")) {
        require(0, "fixture setup");
        xrtDotenvDestroy(config);
        return;
    }
    saved = xrtDotenvGet(config, "KEEP");
    if (budget >= 0) {
        ++faultRuns;
        require(xrtMemDebugFailAfter((unsigned)budget), "inject OOM");
    }
    ok = xrtDotenvParse(config, input, size, limit);
    if (budget >= 0) triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    actualCode = xrtDotenvErrorCode(config);
    actualLine = xrtDotenvErrorLine(config);
    if (triggered) {
        ++faultCount;
        require(!ok && actualCode == XRT_DOTENV_MEMORY, "OOM classification");
    } else {
        if (!(ok == (code == XRT_DOTENV_OK) && actualCode == code && actualLine == line)) {
            if (failures < 12u) fprintf(stderr, "dotenv grammar: key=%s size=%zu code=%d line=%zu\n",
                key == NULL ? "(invalid)" : key, size, actualCode, actualLine);
            require(0, "parse status/code/line");
        }
        if (budget < 0) {
            if (code == XRT_DOTENV_OK) ++validCount;
            else ++invalidCount;
        }
    }
    require(memcmp(copy, input, size) == 0, "input modified");
    if (ok) {
        const char* value = key == NULL ? NULL : xrtDotenvGet(config, key);
        require(xrtDotenvCount(config) == (key == NULL ? 0u : 1u), "replacement count");
        require(key == NULL || (value != NULL && strcmp(value, expected) == 0), "parsed value");
        require(!xrtDotenvHas(config, "KEEP"), "old entry survived success");
    } else {
        require(xrtDotenvCount(config) == 1u && xrtDotenvGet(config, "KEEP") == saved &&
            saved != NULL && strcmp(saved, "old") == 0, "failed parse changed old entry");
    }
    /* 无论语法失败还是OOM，下一次成功都应完整替换并清除两层诊断。 */
    require(xrtDotenvParse(config, "A=recovered\n", 12u, 12u), "retry failed");
    require(xrtDotenvErrorCode(config) == XRT_DOTENV_OK && xrtDotenvErrorLine(config) == 0u &&
        xrtGetError() == NULL, "retry retained diagnostic");
    require(xrtDotenvCount(config) == 1u && !xrtDotenvHas(config, "KEEP") &&
        strcmp(xrtDotenvGet(config, "A"), "recovered") == 0, "retry replacement");
    xrtDotenvDestroy(config);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    require(balanced(&before, &after), "seven memory counters");
}

static void valid_matrix(void)
{
    static const char* keys[] = {"A", "_", "Z9", "export"};
    static const char* starts[] = {"", " ", "\t "};
    static const char* exports[] = {"", "export ", "export\t"};
    static const char* spaces[] = {"", " ", "\t "};
    static const char* endings[] = {"", "\n", "\r\n"};
    static const struct { const char* source; const char* value; } values[] = {
        {"", ""}, {"alpha", "alpha"}, {"a#b", "a#b"}, {"${HOST}", "${HOST}"},
        {"v # ignored", "v"}, {"'a # b\\q'", "a # b\\q"},
        {"\"row\\tvalue\\nnext\\r\\\"\\\\z\"", "row\tvalue\nnext\r\"\\z"},
        {"''", ""}, {"\"\"", ""}, {"你好", "你好"}, {"\"# raw hash\"", "# raw hash"}
    };
    char input[512];
    for (size_t k = 0; k < sizeof(keys) / sizeof(*keys); ++k)
    for (size_t s = 0; s < sizeof(starts) / sizeof(*starts); ++s)
    for (size_t e = 0; e < sizeof(exports) / sizeof(*exports); ++e)
    for (size_t g = 0; g < sizeof(spaces) / sizeof(*spaces); ++g)
    for (size_t l = 0; l < sizeof(endings) / sizeof(*endings); ++l)
    for (size_t v = 0; v < sizeof(values) / sizeof(*values); ++v) {
        int count = snprintf(input, sizeof(input), "%s%s%s%s=%s%s \t# tail%s",
            starts[s], exports[e], keys[k], spaces[g], spaces[g], values[v].source, endings[l]);
        require(count > 0 && (size_t)count < sizeof(input), "fixture formatting");
        one(input, strlen(input), sizeof(input), keys[k], values[v].value, XRT_DOTENV_OK, 0u, -1);
    }
    /* 注释是否开始取决于#前的实际空白，不取决于等号或值的起点。 */
    one("A=#attached", 11u, 11u, "A", "#attached", XRT_DOTENV_OK, 0u, -1);
    one("A= #comment", 11u, 11u, "A", "", XRT_DOTENV_OK, 0u, -1);
    one("A=x\nA=last", 10u, 10u, "A", "last", XRT_DOTENV_OK, 0u, -1);
}

static void invalid_matrix(void)
{
    static const char* bad[] = {
        "export", "export # comment", "A", "=x", "9A=x", "A-B=x", "中文=x", "A B=x",
        "A='open", "A=\"open", "A=\"bad\\q\"", "A=\"tail\\", "A='x'junk", "A=\"x\" junk",
        "A=x\r", "# comment\r", "A=x\ry", "A=\"a\rb\"", "A='a\rb'", "A=\"\\'\""
    };
    static const char* prefixes[] = {"", "B=partial\n", "# note\nB=partial\n"};
    char input[512];
    for (size_t b = 0; b < sizeof(bad) / sizeof(*bad); ++b)
    for (size_t p = 0; p < sizeof(prefixes) / sizeof(*prefixes); ++p) {
        int count = snprintf(input, sizeof(input), "%s%s", prefixes[p], bad[b]);
        require(count > 0 && (size_t)count < sizeof(input), "invalid fixture formatting");
        one(input, strlen(input), sizeof(input), NULL, NULL, XRT_DOTENV_FORMAT, p + 1u, -1);
    }
    {
        static const char nul[] = "A=x\0B=y";
        static const char utf8[] = "B=partial\nA=\xc0\xaf";
        one(nul, sizeof(nul) - 1u, sizeof(nul), NULL, NULL, XRT_DOTENV_ARGUMENT, 0u, -1);
        one(utf8, sizeof(utf8) - 1u, sizeof(utf8), NULL, NULL, XRT_DOTENV_FORMAT, 0u, -1);
        one(nul, sizeof(nul) - 1u, 0u, NULL, NULL, XRT_DOTENV_LIMIT, 0u, -1);
        one(utf8, sizeof(utf8) - 1u, 0u, NULL, NULL, XRT_DOTENV_LIMIT, 0u, -1);
    }
}

static void limits_and_faults(void)
{
    static const struct { const char* input; const char* key; const char* value; } cases[] = {
        {"", NULL, NULL}, {"\xef\xbb\xbf", NULL, NULL}, {"# note\r\n", NULL, NULL},
        {"A=\n", "A", ""}, {"export A='你好' # note\r\n", "A", "你好"},
        {"A=old\nA=last\n", "A", "last"}, {"A=\"a\\n\\t\\r\\\"\\\\\"\n", "A", "a\n\t\r\"\\"},
        {"export \t=worker", "export", "worker"},
        {"export export =first\nexport=last\n", "export", "last"}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        size_t size = strlen(cases[i].input);
        unsigned before = faultCount;
        for (size_t extra = 0; extra < 2u; ++extra)
            one(cases[i].input, size, size + extra, cases[i].key, cases[i].value,
                XRT_DOTENV_OK, 0u, -1);
        if (size != 0u) one(cases[i].input, size, size - 1u, NULL, NULL, XRT_DOTENV_LIMIT, 0u, -1);
        /* 固定上界覆盖首次映射桶、两次键值复制与重复键替换的全部故障点。 */
        for (int budget = 0; budget < 16; ++budget)
            one(cases[i].input, size, size, cases[i].key, cases[i].value,
                XRT_DOTENV_OK, 0u, budget);
        require(faultCount - before < 16u && (cases[i].key == NULL || faultCount > before),
            "OOM scan missed failure or success");
    }
}

int main(void)
{
    /* 预备线程诊断槽，逐条统计不把一次性TLS分配误判成泄漏。 */
    xrtSetErrorInfo(XERR_VALUE, "dotenv.grammar", 0, "warm");
    xrtClearError();
    valid_matrix();
    invalid_matrix();
    limits_and_faults();
    printf("dotenv grammar: %u valid, %u invalid, %u fault runs, %u OOM, %u failures\n",
        validCount, invalidCount, faultRuns, faultCount, failures);
    return failures == 0u ? 0 : 1;
}

#ifdef __TINYC__
#include "../xrt_dotenv.c"
#endif
