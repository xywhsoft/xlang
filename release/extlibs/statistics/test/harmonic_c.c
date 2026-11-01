/* 巨小正负倒数抵消后，小尾项仍须参与调和平均数，且输入排列不应改变结果。 */
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../statistics_native.h"
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../statistics_native.c"
#endif

static unsigned checks, failures, cases, faults;

static void require(bool ok, cstr message)
{
    ++checks;
    if (!ok) { if (failures < 12) fprintf(stderr, "statistics harmonic: %s\n", message); ++failures; }
}

static xvalue* numbers(const double* a, size_t n)
{
    xvalue* result = xrtValueArray();
    for (size_t i = 0; result != NULL && i < n; ++i) {
        if (!xrtValueArrayAppendNew(result, xrtValueFloat(a[i]))) {
            xrtValueRelease(result); return NULL;
        }
    }
    return result;
}

static bool balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    return before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount;
}

static void bits(double value)
{
    uint64_t encoded;
    memcpy(&encoded, &value, sizeof(encoded)); printf("%016" PRIx64, encoded);
}

static void one(const double* a, size_t n, double expected, bool records)
{
    xmemdebugsnapshot before;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    xvalue* input = numbers(a, n);
    require(input != NULL, "prepare input");
    xvalue* result = input != NULL ? xstat_harmmean(input) : NULL;
    const xerror* error = xrtGetError();
    double actual = NAN;
    bool ok = result != NULL && error == NULL && xrtValueGetFloat(result, &actual);
    if (records) {
        printf("%u\t", cases);
        for (size_t i = 0; i < n; ++i) { if (i != 0) putchar(','); bits(a[i]); }
        putchar('\t');
        if (ok) bits(actual);
        else fputs(error != NULL && xrtErrorKind(error) == XERR_VALUE ? "error:value" : "error:unexpected", stdout);
        putchar('\n');
    } else if (isfinite(expected)) {
        require(ok && isfinite(actual) && (actual == expected || fabs(actual - expected) <= fabs(expected) * 2e-13), "finite signed harmonic result");
    } else require(!ok && error != NULL && xrtErrorKind(error) == XERR_VALUE, "zero reciprocal sum or overflow rejected");
    for (size_t i = 0; input != NULL && i < n; ++i) {
        double value = NAN;
        require(xrtValueGetFloat(xrtValueArrayGet(input, i), &value) && memcmp(&value, &a[i], sizeof(value)) == 0, "input bits preserved");
    }
    xrtValueRelease(result); xrtValueRelease(input);
    require(balanced(before), "seven memory counters"); ++cases;
}

static void vectors(bool records)
{
    static const int small[] = {-1074, -1060, -1022, -1000, -800, -500};
    static const int large[] = {-20, 0, 100, 500, 900, 1020};
    for (size_t s = 0; s < sizeof(small) / sizeof(small[0]); ++s)
        for (size_t l = 0; l < sizeof(large) / sizeof(large[0]); ++l)
            for (int sign = -1; sign <= 1; sign += 2) {
                double tiny = ldexp(1.0, small[s]), tail = sign * ldexp(1.0, large[l]);
                double a[] = {tiny, -tiny, tail, tail * 2};
                /* 四值全排列，包含大抵消项分隔、尾项前置及同号对照。 */
                for (size_t i = 0; i < 4; ++i) for (size_t j = 0; j < 4; ++j) if (j != i)
                    for (size_t k = 0; k < 4; ++k) if (k != i && k != j) {
                        size_t last = 6u - i - j - k;
                        double p[] = {a[i], a[j], a[k], a[last]};
                        one(p, 4, tail * (8.0 / 3.0), records);
                    }
                double triple[] = {tiny, -tiny, tail};
                one(triple, 3, tail * 3, records);
                double same[] = {tiny, tiny, fabs(tail), fabs(tail) * 2};
                one(same, 4, tiny * 2, records);
                double nested[] = {tiny, -tiny, ldexp(1.0, -400), -ldexp(1.0, -400), tail, tail * 2};
                one(nested, 6, tail * 4, records);
            }
    double tiniest = nextafter(0.0, 1.0);
    double zero[] = {tiniest, -tiniest};
    double overflow[] = {tiniest, -tiniest, DBL_MAX / 2};
    double invalid[] = {1, 0, 2};
    one(zero, 2, NAN, records); one(overflow, 3, NAN, records); one(invalid, 3, NAN, records);
    double edge[] = {nextafter(DBL_MIN, 0), DBL_MIN, nextafter(DBL_MIN, INFINITY)};
    for (size_t e = 0; e < 3; ++e) {
        double a[] = {edge[e], -edge[e], 0.5, 1};
        for (size_t i = 0; i < 4; ++i) for (size_t j = 0; j < 4; ++j) if (j != i)
            for (size_t k = 0; k < 4; ++k) if (k != i && k != j) {
                double p[] = {a[i], a[j], a[k], a[6u - i - j - k]};
                one(p, 4, 4.0 / 3.0, records);
            }
    }
}

/* 同号尾项含非二次幂分母，交给独立 Fraction 参照，避免 C 自算预期。 */
static void random_records(void)
{
    uint32_t seed = 0x8c39e71bu;
    for (unsigned trial = 0; trial < 1024; ++trial) {
        double a[9];
        size_t count = 0, pairs = 1u + trial % 3u, tails = 1u + (trial / 3u) % 3u;
        for (size_t i = 0; i < pairs; ++i) {
            int exponent = -1074 + (int)((trial * 31u + (unsigned)i * 127u) % 700u);
            double value = ldexp(1.0, exponent);
            a[count++] = value; a[count++] = -value;
        }
        for (size_t i = 0; i < tails; ++i) {
            seed = seed * 1664525u + 1013904223u;
            double fraction = (3u + ((seed >> 16) % 13u)) / 8.0;
            a[count++] = (trial & 1u ? -1.0 : 1.0) * ldexp(fraction, -20 + (int)(trial % 1000u));
        }
        for (size_t i = count; i > 1; --i) {
            seed = seed * 1664525u + 1013904223u;
            size_t j = seed % i;
            double value = a[i - 1u]; a[i - 1u] = a[j]; a[j] = value;
        }
        one(a, count, NAN, true);
    }
}

static void allocations(void)
{
    double tiniest = nextafter(0.0, 1.0);
    double a[] = {tiniest, -tiniest, 1, 2};
    bool success = false;
    for (unsigned budget = 0; budget < 32; ++budget) {
        xmemdebugsnapshot before;
        xrtClearError(); xrtMemDebugSnapshot(&before);
        xvalue* input = numbers(a, 4);
        require(input != NULL, "OOM input");
        require(xrtMemDebugFailAfter(budget), "arm OOM");
        xvalue* result = input != NULL ? xstat_harmmean(input) : NULL;
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        double value = NAN;
        if (triggered) {
            ++faults;
            require(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "OOM keeps MemoryError");
        } else {
            success = true;
            require(xrtGetError() == NULL && xrtValueGetFloat(result, &value) && fabs(value - 8.0 / 3.0) < 1e-14, "no-fault result");
        }
        xrtValueRelease(result);
        result = xstat_harmmean(input);
        require(xrtGetError() == NULL && xrtValueGetFloat(result, &value) && fabs(value - 8.0 / 3.0) < 1e-14, "same-input OOM recovery");
        xrtValueRelease(result); xrtValueRelease(input);
        require(balanced(before), "OOM memory balance");
        if (!triggered) break;
    }
    require(faults != 0 && success, "actual OOM and recovery covered");
}

int main(int argc, char** argv)
{
    bool records = argc == 2 && strcmp(argv[1], "--records") == 0;
    if ((argc != 1 && !records) || !xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_VALUE, "test.statistics", 0, "warm diagnostics"); xrtClearError();
    vectors(records);
    if (records) random_records();
    if (!records) {
        allocations();
        printf("statistics harmonic: %u cases, %u checks, %u OOM, %u failures\n", cases, checks, faults, failures);
    }
    return failures != 0;
}
