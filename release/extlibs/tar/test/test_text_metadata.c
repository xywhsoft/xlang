#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#include "../xrt_tar_common.c"
#ifndef XTAR_TEST_READER
#define XTAR_TEST_READER "../xrt_tar_reader.c"
#endif
#include XTAR_TEST_READER
#include <stdio.h>

static unsigned cases, faults, failures;

/* 独立构造最小头，不用被测 Writer 生成预期，字段空格不属于填充。 */
static void header(unsigned char* data, char type, unsigned size)
{
    unsigned sum = 0;
    memset(data, 0, 512u);
    memcpy(data, "entry", 5u);
    memcpy(data + 157u, "target", 6u);
    memcpy(data + 265u, "owner", 5u);
    memcpy(data + 297u, "group", 5u);
    memcpy(data + 257u, "ustar\00000", 8u);
    data[156u] = (unsigned char)type;
    (void)snprintf((char*)data + 124u, 12u, "%011o", size);
    memset(data + 148u, ' ', 8u);
    for (size_t i = 0; i < 512u; ++i) sum += data[i];
    (void)snprintf((char*)data + 148u, 8u, "%06o", sum);
    data[155u] = ' ';
}

static void checksum(unsigned char* data)
{
    unsigned sum = 0;
    memset(data + 148u, ' ', 8u);
    for (size_t i = 0; i < 512u; ++i) sum += data[i];
    (void)snprintf((char*)data + 148u, 8u, "%06o", sum);
    data[155u] = ' ';
}

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        before->InvalidFreeCount == after.InvalidFreeCount &&
        before->DoubleFreeCount == after.DoubleFreeCount &&
        before->OverflowCount == after.OverflowCount && before->UnderflowCount == after.UnderflowCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount;
}

/* 返回 0 表示预期注入故障，1 表示完整成功，-1 表示合同/内存不符。 */
static int read_case(const unsigned char* data, size_t size, const char* const expected[4], uint64 budget)
{
    xmemdebugsnapshot before;
    xrttarentry entry;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    if (budget != UINT64_MAX && !xrtMemDebugFailAfter(budget)) return -1;
    xrttarreader* reader = xrtTarReaderOpenMemory(data, size, NULL);
    bool next = reader != NULL && xrtTarReaderNext(reader, &entry);
    bool triggered = budget != UINT64_MAX && xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    bool ok;
    if (triggered) {
        ++faults;
        ok = !next && xrtErrorKind(xrtGetError()) == XERR_MEMORY &&
            (reader == NULL || xrtTarReaderError(reader)->iCode == XRT_TAR_ERROR_MEMORY);
    } else {
        ok = next && xrtGetError() == NULL && strcmp(entry.sName, expected[0]) == 0 &&
            strcmp(entry.sLinkName, expected[1]) == 0 && strcmp(entry.sUserName, expected[2]) == 0 &&
            strcmp(entry.sGroupName, expected[3]) == 0;
        ok = ok && !xrtTarReaderNext(reader, &entry) && xrtTarReaderEOF(reader);
    }
    xrtTarReaderDestroy(reader);
    ok = balanced(&before) && ok;
    return ok ? (triggered ? 0 : 1) : -1;
}

static void check_case(const unsigned char* data, size_t size, const char* const expected[4])
{
    ++cases;
    if (read_case(data, size, expected, UINT64_MAX) != 1) {
        fprintf(stderr, "TAR text case %u: metadata or memory differs\n", cases);
        ++failures;
        return;
    }
    bool done = false;
    for (unsigned budget = 0; budget < 64u; ++budget) {
        int result = read_case(data, size, expected, budget);
        if (result < 0 || read_case(data, size, expected, UINT64_MAX) != 1) {
            fprintf(stderr, "TAR text case %u allocation budget %u: failure/retry differs\n", cases, budget);
            ++failures;
            return;
        }
        if (result == 1) { done = budget != 0u; break; }
    }
    if (!done) ++failures;
}

int main(void)
{
    static const char* const values[] = {
        "plain", "tail ", "tail\n", "tail\n\n", "tail\r\n", "tail \n", "a\t", "\xe5\x90\x8d\xe7\xa7\xb0 "
    };
    static const size_t offsets[] = {0u, 157u, 265u, 297u, 345u};
    if (!xrtMemDebugEnabled() || !xrtMemDebugFailAfter(UINT64_MAX)) return 1;
    xrtMemDebugFailClear();
    xrtSetErrorInfo(XERR_MEMORY, "test.tar", 0, "warm error slot");
    xrtClearError();
    for (size_t value = 0; value < sizeof(values) / sizeof(values[0]); ++value) {
        for (unsigned field = 0; field < 5u; ++field) {
            unsigned char packed[2048] = {0};
            const char* expected[] = {"entry", "target", "owner", "group"};
            char joined[128];
            header(packed, '0', 0u);
            memcpy(packed + offsets[field], values[value], strlen(values[value]) + 1u);
            if (field < 4u) expected[field] = values[value];
            else {
                (void)snprintf(joined, sizeof(joined), "%s/entry", values[value]);
                expected[0] = joined;
            }
            checksum(packed);
            check_case(packed, sizeof(packed), expected);
        }
        for (unsigned kind = 0; kind < 2u; ++kind) {
            for (unsigned zeros = 0; zeros < 3u; ++zeros) {
                unsigned char packed[3072] = {0};
                const char* expected[] = {"entry", "target", "owner", "group"};
                size_t trailing = zeros == 2u ? 3u : zeros;
                header(packed, kind == 0u ? 'L' : 'K', strlen(values[value]) + trailing);
                memcpy(packed + 512u, values[value], strlen(values[value]));
                header(packed + 1024u, '0', 0u);
                expected[kind] = values[value];
                check_case(packed, sizeof(packed), expected);
            }
        }
    }
    printf("TAR text metadata: %u cases, %u allocation faults, %u failures\n", cases, faults, failures);
    return failures == 0u ? 0 : 1;
}
