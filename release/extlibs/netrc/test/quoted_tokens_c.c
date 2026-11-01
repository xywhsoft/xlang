/* 使用合成凭据，不记录密码；逐字段核对引用/转义、输出限额与OOM恢复。 */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_STRING_FORMAT
#include "../xrt_netrc_internal.h"
#ifndef XNETRC_TEST_WRITER
#define XNETRC_TEST_WRITER "../xrt_netrc_writer.c"
#endif

static unsigned failures, vectors, faults;
static const char* patterns[] = {"plain", "has space", "#note", "\"", "\\", "'",
    "\\\"", "\xe7\x94\xa8\xe6\x88\xb7 ", "\t", "machine", "default", "macdef"};
static const unsigned repeats[] = {0, 1, 31, 127, 128, 129, 1024};

static void check(bool okay, cstr message)
{
    if (!okay) {
        if (failures < 12u) fprintf(stderr, "quoted tokens: %s\n", message);
        ++failures;
    }
}

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->OverflowCount == after.OverflowCount && before->UnderflowCount == after.UnderflowCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount;
}

static char* repeat(cstr pattern, unsigned count)
{
    size_t length = strlen(pattern);
    char* text = xrtMalloc(length * count + 1u);
    if (text != NULL) {
        for (unsigned i = 0u; i < count; ++i) memcpy(text + length * i, pattern, length);
        text[length * count] = 0;
    }
    return text;
}

static xvalue* document(cstr text, unsigned field)
{
    static const char source[] = "machine h login user account team password dummy\n"
        "macdef m\n echo ok\n\n";
    static const char* names[] = {"", "login", "account", "password"};
    xrtnetrcerror error;
    xvalue* doc = xrtNetrcParseMemory(source, sizeof(source) - 1u, NULL, &error);
    if (doc == NULL) return NULL;
    xvalue* hosts = xrtValueObjectGet(doc, xrtStrView("hosts"));
    xvalue* macros = xrtValueObjectGet(doc, xrtStrView("macros"));
    xvalue* auth = xrtValueObjectGet(hosts, xrtStrView("h"));
    bool okay = true;
    if (okay && (field == 0u || field == 4u)) {
        xvalue* parent = field == 0u ? hosts : macros;
        xstrview key = xrtStrView(field == 0u ? "h" : "m");
        xvalue* child = xrtValueRetain(xrtValueObjectGet(parent, key));
        okay = child != NULL && xrtValueObjectRemove(parent, key) &&
            xrtValueObjectSetTake(parent, xrtStrView(text), &child);
        xrtValueRelease(child);
    } else if (okay) {
        xvalue* value = xrtValueString(xrtStrView(text));
        okay = value != NULL && xrtValueObjectSetTake(auth, xrtStrView(names[field]), &value);
        xrtValueRelease(value);
    }
    if (!okay) { xrtValueRelease(doc); return NULL; }
    return doc;
}

static bool same(const xbuffer* a, const xbuffer* b)
{
    return a != NULL && b != NULL && a->Size == b->Size && memcmp(a->Data, b->Data, a->Size) == 0;
}

static void fault_sweep(xvalue* doc, const xbuffer* expected)
{
    bool complete = false;
    for (unsigned budget = 0u; budget < 64u; ++budget) {
        xrtnetrcerror error;
        xmemdebugsnapshot before;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        check(xrtMemDebugFailAfter(budget), "fault setup");
        xbuffer* wire = xrtNetrcStringify(doc, NULL, &error);
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        check(triggered ? wire == NULL && error.iCode == XRT_NETRC_ERROR_MEMORY &&
            xrtErrorKind(xrtGetError()) == XERR_MEMORY : same(wire, expected) && error.iCode == XRT_NETRC_OK,
            "allocation failure classification or output changed");
        if (triggered) ++faults;
        xrtBufferDestroy(wire);
        xrtClearError();
        wire = xrtNetrcStringify(doc, NULL, &error);
        check(same(wire, expected) && error.iCode == XRT_NETRC_OK, "immediate recovery changed document");
        xrtBufferDestroy(wire);
        check(balanced(&before), "fault memory balance");
        if (!triggered) { complete = budget != 0u; break; }
    }
    check(complete, "fault sweep incomplete");
}

static void matrix(cstr corpus)
{
    for (unsigned p = 0u; p < sizeof(patterns) / sizeof(patterns[0]); ++p) {
        for (unsigned n = 0u; n < sizeof(repeats) / sizeof(repeats[0]); ++n) {
            char* token = repeat(patterns[p], repeats[n]);
            check(token != NULL, "create token");
            if (token == NULL) return;
            for (unsigned field = 0u; field < 5u; ++field) {
                if (repeats[n] == 0u && (field == 0u || field == 4u)) continue;
                xrtnetrcerror error;
                xvalue* doc = document(token, field);
                check(doc != NULL, "create document");
                if (doc == NULL) continue;
                xbuffer* wire = xrtNetrcStringify(doc, NULL, &error);
                check(wire != NULL && error.iCode == XRT_NETRC_OK, "serialize token");
                if (wire != NULL) {
                    ++vectors;
                    xrtnetrcoptions options;
                    xrtNetrcOptionsInit(&options);
                    for (int delta = -1; delta <= 1; ++delta) {
                        xmemdebugsnapshot before;
                        xrtClearError();
                        xrtMemDebugSnapshot(&before);
                        options.tLimits.iMaxOutputBytes = wire->Size + delta;
                        xbuffer* result = xrtNetrcStringify(doc, &options, &error);
                        check(delta < 0 ? result == NULL && error.iCode == XRT_NETRC_ERROR_LIMIT :
                            same(result, wire) && error.iCode == XRT_NETRC_OK, "encoded byte limit");
                        xrtBufferDestroy(result);
                        check(balanced(&before), "limit memory balance");
                    }
                    xvalue* restored = xrtNetrcParseMemory(wire->Data, wire->Size, NULL, &error);
                    xbuffer* again = restored != NULL ? xrtNetrcStringify(restored, NULL, &error) : NULL;
                    check(same(again, wire), "canonical parse/stringify roundtrip");
                    xrtBufferDestroy(again);
                    xrtValueRelease(restored);
                    if (repeats[n] == 1024u && field == 3u && p < 9u) fault_sweep(doc, wire);
                    if (corpus != NULL) {
                        str path = xrtFormat("%s/p%u-n%u-f%u.netrc", corpus, p, n, field);
                        check(path != NULL && xrtFileWriteAll(path, xrtBufferView(wire)), "write synthetic corpus");
                        xrtFree(path);
                    }
                }
                xrtBufferDestroy(wire);
                xrtValueRelease(doc);
            }
            xrtFree(token);
        }
    }
}

/* 可选成本夹具：整进程计时，含初始化/建输入及每次结果逐字节核对。 */
static int benchmark(unsigned scenario)
{
    static const char* parts[] = {"plain", "long text ", "\\\""};
    char* token = repeat(parts[scenario], 1024u);
    xvalue* doc = token != NULL ? document(token, 3u) : NULL;
    xrtnetrcerror error;
    xbuffer* expected = doc != NULL ? xrtNetrcStringify(doc, NULL, &error) : NULL;
    if (expected == NULL) { xrtValueRelease(doc); xrtFree(token); return 1; }
    for (unsigned i = 0u; i < 2000u; ++i) {
        xbuffer* output = xrtNetrcStringify(doc, NULL, &error);
        bool okay = same(output, expected);
        xrtBufferDestroy(output);
        if (!okay) { xrtBufferDestroy(expected); xrtValueRelease(doc); xrtFree(token); return 1; }
    }
    printf("netrc quoted benchmark %u: 2000 encodes, %zu bytes\n", scenario, expected->Size);
    xrtBufferDestroy(expected);
    xrtValueRelease(doc);
    xrtFree(token);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 3 && strcmp(argv[1], "--bench") == 0 &&
        strlen(argv[2]) == 1u && argv[2][0] >= '0' && argv[2][0] <= '2')
        return benchmark((unsigned)(argv[2][0] - '0'));
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE, "test.netrc", 0, "warm diagnostic slot");
    xrtClearError();
    matrix(argc > 1 ? argv[1] : NULL);
    printf("netrc quoted tokens: %u vectors, %u allocation faults, %u failures\n", vectors, faults, failures);
    return failures == 0u ? 0 : 1;
}

#include "../xrt_netrc_common.c"
#include "../xrt_netrc_parser.c"
#include XNETRC_TEST_WRITER
