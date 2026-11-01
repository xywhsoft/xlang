/* 独立字节组帧，比较完整parse结果与不物化的validate；不调用产品build。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../fix_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned cases, failures, faults;
static bool measure;

static void require(bool ok, const char* text)
{
    if (!ok) {
        if (failures < 8u) fprintf(stderr, "FIX validation: %s\n", text);
        ++failures;
    }
}

static bool balanced(xmemdebugsnapshot a, xmemdebugsnapshot b)
{
    return a.LiveCount == b.LiveCount && a.LiveBytes == b.LiveBytes &&
        a.InvalidFreeCount == b.InvalidFreeCount && a.DoubleFreeCount == b.DoubleFreeCount &&
        a.OverflowCount == b.OverflowCount && a.UnderflowCount == b.UnderflowCount &&
        a.UseAfterFreeCount == b.UseAfterFreeCount;
}

static char* frame(unsigned count, const char* value)
{
    size_t width = strlen(value), body = (width + 4u) * count, capacity = body + 96u;
    char* output = malloc(capacity);
    if (output == NULL) return NULL;
    int head = snprintf(output, capacity, "8=FIX.4.4\x01" "9=%zu\x01", body);
    if (head <= 0 || (size_t)head >= capacity) { free(output); return NULL; }
    size_t used = (size_t)head;
    for (unsigned i = 0; i < count; ++i) {
        memcpy(output + used, "58=", 3u); used += 3u;
        memcpy(output + used, value, width); used += width;
        output[used++] = '\x01';
    }
    unsigned sum = 0;
    for (size_t i = 0; i < used; ++i) sum += (unsigned char)output[i];
    snprintf(output + used, capacity - used, "10=%03u\x01", sum & 255u);
    return output;
}

static void observe(char* input, unsigned count, const char* value, bool valid)
{
    xmemdebugsnapshot start, parsed, middle, validated, end;
    size_t length = input == NULL ? 0u : strlen(input);
    char* saved = malloc(length + 1u);
    if (saved == NULL) { require(false, "fixture copy"); return; }
    if (input != NULL) memcpy(saved, input, length + 1u);
    xrtClearError(); xrtMemDebugSnapshot(&start);
    xvalue* result = xfix_parse(input);
    xrtMemDebugSnapshot(&parsed);
    require((xrtValueType(result) == XVALUE_OBJECT) == valid, "parse status");
    if (valid) {
        xvalue* fields = xrtValueObjectGet(result, XRT_STR_LITERAL("fields"));
        require(xrtValueCount(fields) == (size_t)count + 3u, "field count");
        for (unsigned i = 0; i < count; ++i) {
            xvalue* field = xrtValueArrayGet(fields, i + 2u);
            int64 tag = 0; xstrview text = {0};
            require(xrtValueGetInt(xrtValueObjectGet(field, XRT_STR_LITERAL("tag")), &tag) && tag == 58 &&
                xrtValueGetString(xrtValueObjectGet(field, XRT_STR_LITERAL("value")), &text) &&
                text.Size == strlen(value) && memcmp(text.Data, value, text.Size) == 0, "ordered field/value");
        }
    } else require(xrtGetError() != NULL, "parse diagnostic");
    xrtValueRelease(result); xrtClearError(); xrtMemDebugSnapshot(&middle);
    require(xfix_validate(input) == valid && !xfix_has_error(), "validate status/clear diagnostic");
    xrtMemDebugSnapshot(&validated);
    if (valid && !measure) {
        require(validated.AllocCount == middle.AllocCount && validated.ReallocCount == middle.ReallocCount,
            "valid predicate allocated");
        require(xrtMemDebugFailAfter(0u), "inject valid budget");
        bool ok = xfix_validate(input), triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        require(ok && !triggered && !xfix_has_error(), "valid predicate consumed zero budget");
    }
    if (!valid && !measure) {
        /* 错误诊断仍可能分配，不能将诊断OOM当作普通false静默清除。 */
        unsigned before = faults;
        for (unsigned budget = 0; budget < 8u; ++budget) {
            require(xrtMemDebugFailAfter(budget), "inject diagnostic budget");
            bool ok = xfix_validate(input), triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            require(!ok && (triggered ? xfix_memory_error() : !xfix_has_error()), "diagnostic OOM classification");
            if (triggered) ++faults;
            xrtClearError();
        }
        require(faults > before, "diagnostic budgets missed allocation failures");
    }
    require(input == NULL || memcmp(saved, input, length + 1u) == 0, "input modified");
    xrtClearError(); xrtMemDebugSnapshot(&end);
    require(balanced(start, end), "seven memory counters");
    if (measure) printf("%u\t%u\t%zu\t%llu\t%llu\n", count, (unsigned)valid, length,
        (unsigned long long)(parsed.AllocCount - start.AllocCount),
        (unsigned long long)(validated.AllocCount - middle.AllocCount));
    ++cases;
    free(saved);
}

int main(int argc, char** argv)
{
    static const unsigned counts[] = {0, 1, 2, 8, 64, 512, 4096};
    static const char* values[] = {"", "A=B#=x", "中文", "\xff"};
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE, "test.fix", 0, "warm"); xrtClearError();
    if (argc == 4 && strcmp(argv[1], "--bench") == 0) {
        unsigned count = (unsigned)strtoul(argv[2], NULL, 10), repeat = (unsigned)strtoul(argv[3], NULL, 10);
        if (count > 4096u || repeat == 0u || repeat > 10000u) return 2;
        char* input = frame(count, "A=B#=x");
        if (input == NULL) return 2;
        clock_t start = clock();
        for (unsigned i = 0; i < repeat; ++i) if (!xfix_validate(input)) { free(input); return 1; }
        printf("fields=%u iterations=%u milliseconds=%.3f\n", count, repeat, (double)(clock()-start)*1000.0/CLOCKS_PER_SEC);
        free(input); return 0;
    }
    measure = argc == 2 && strcmp(argv[1], "--measure") == 0;
    if (argc != 1 && !measure) return 2;
    for (size_t c = 0; c < sizeof(counts)/sizeof(*counts); ++c)
    for (size_t v = 0; v < sizeof(values)/sizeof(*values); ++v) {
        char* input = frame(counts[c], values[v]);
        if (input == NULL) return 2;
        observe(input, counts[c], values[v], true);
        if (!measure) {
            size_t size = strlen(input);
            input[size - 2u] = 'X';
            observe(input, 0, "", false);
        }
        free(input);
    }
    if (!measure) {
        observe(NULL, 0, "", false);
        char* input = frame(1u, "A=B#=x");
        if (input == NULL) return 2;
        size_t size = strlen(input);
        for (size_t cut = 0; cut < size; ++cut) {
            char saved = input[cut]; input[cut] = 0;
            observe(input, 0, "", false); input[cut] = saved;
        }
        free(input);
        input = frame(4097u, "x");
        if (input == NULL) return 2;
        observe(input, 0, "", false); free(input);
        printf("fix validation: %u cases, %u diagnostic OOM, %u failures\n", cases, faults, failures);
    }
    return failures != 0u || ferror(stdout);
}

#ifdef __TINYC__
#include "../fix_native.c"
#endif
