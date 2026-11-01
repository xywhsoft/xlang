/* 版本元数据只参与校验和提取，不应在每个范围比较器中保存副本。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#ifndef XSEMVER_TEST_NATIVE
#define XSEMVER_TEST_NATIVE "../semver_native.c"
#endif
#include XSEMVER_TEST_NATIVE

static unsigned failures, cases, faults;

static void check(bool okay, cstr message)
{
    if (!okay) {
        if (failures < 12u) fprintf(stderr, "semver metadata: %s\n", message);
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

static void version_case(cstr version, bool valid, int64 major, int64 minor, int64 patch,
    cstr pre, cstr build)
{
    xmemdebugsnapshot before;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    check(xsemver_valid(version) == valid, "valid");
    check(xsemver_major(version) == (valid ? major : -1), "major");
    check(xsemver_minor(version) == (valid ? minor : -1), "minor");
    check(xsemver_patch(version) == (valid ? patch : -1), "patch");
    check(xsemver_compare(version, version) == (valid ? 0 : -2), "compare");
    char* actualPre = xsemver_prerelease(version);
    char* actualBuild = xsemver_build(version);
    char* clean = xsemver_clean(version);
    check(actualPre != NULL && strcmp(actualPre, valid ? pre : "") == 0, "prerelease");
    check(actualBuild != NULL && strcmp(actualBuild, valid ? build : "") == 0, "build");
    check(clean != NULL && strcmp(clean, valid ? version : "") == 0, "clean");
    check(xrtGetError() == NULL, "unexpected error");
    xrtFree(clean); xrtFree(actualBuild); xrtFree(actualPre);
    check(balanced(&before), "case memory balance");
    ++cases;
}

static void metadata_matrix(void)
{
    static const unsigned sizes[] = {0, 1, 31, 254, 255, 256};
    static const char* cores[] = {"1.2.3", "9223372036854775807.9223372036854775807.9223372036854775807"};
    static const char* bad[] = {"", ".", "..", ".a", "a.", "a..b", "a+b", "x/y", "a b", "\x7f", "\xc3\xa9"};
    char pre[257], build[257], version[1200];
    for (unsigned c = 0u; c < 2u; ++c) {
        for (unsigned p = 0u; p < 6u; ++p) {
            for (unsigned b = 0u; b < 6u; ++b) {
                memset(pre, 'a', sizes[p]); pre[sizes[p]] = 0;
                memset(build, '0', sizes[b]); build[sizes[b]] = 0;
                snprintf(version, sizeof(version), "%s%s%s%s%s", cores[c],
                    sizes[p] ? "-" : "", pre, sizes[b] ? "+" : "", build);
                version_case(version, sizes[p] <= 255u && sizes[b] <= 255u,
                    c ? INT64_MAX : 1, c ? INT64_MAX : 2, c ? INT64_MAX : 3, pre, build);
            }
        }
    }
    for (unsigned i = 0u; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        snprintf(version, sizeof(version), "1.2.3+%s", bad[i]);
        version_case(version, false, 0, 0, 0, "", "");
        snprintf(version, sizeof(version), "1.2.3-rc+%s", bad[i]);
        version_case(version, false, 0, 0, 0, "", "");
    }
    version_case("1.2.3+meta.000-a.Z", true, 1, 2, 3, "", "meta.000-a.Z");
    version_case("1.2.3-rc.1+meta.000-a.Z", true, 1, 2, 3, "rc.1", "meta.000-a.Z");
}

static void ownership_faults(void)
{
    static char* (*const functions[])(const char*) = {xsemver_prerelease, xsemver_build, xsemver_clean};
    char source[600] = {0}, saved[600];
    snprintf(source, sizeof(source), "1.2.3-%0255d+%0255d", 1, 0);
    /* 数字预发布不允许前导零，改为同长合法标识符；build允许全零。 */
    memset(source + 6, 'a', 255u);
    memcpy(saved, source, sizeof(source));
    for (unsigned f = 0u; f < 3u; ++f) {
        char* expected = functions[f](source);
        check(expected != NULL, "baseline owned result");
        if (expected == NULL) continue;
        for (unsigned budget = 0u; budget <= 1u; ++budget) {
            xmemdebugsnapshot before;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            check(xrtMemDebugFailAfter(budget), "fault setup");
            char* result = functions[f](source);
            bool triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            check(budget == 0u ? result == NULL && triggered && xrtErrorKind(xrtGetError()) == XERR_MEMORY :
                result != NULL && !triggered && strcmp(result, expected) == 0 && xrtGetError() == NULL,
                "owned result allocation contract");
            if (triggered) ++faults;
            xrtFree(result);
            check(memcmp(saved, source, sizeof(source)) == 0 && balanced(&before), "fault mutates input or leaks");
        }
        char* independent = functions[f](source);
        source[6] = 'z';
        check(independent != NULL && strcmp(independent, expected) == 0, "returned text aliases source");
        source[6] = 'a';
        xrtFree(independent); xrtFree(expected);
    }
}

static void range_boundaries(void)
{
    char range[600] = "", before[600];
    for (unsigned i = 0u; i < 32u; ++i) strcat(range, i == 0u ? ">=1.0.0" : " >=1.0.0");
    strcpy(before, range);
    check(xrtMemDebugFailAfter(0u), "range fault setup");
    bool okay = xsemver_satisfies("1.2.3+meta.000", range);
    bool triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    check(okay && !triggered && strcmp(range, before) == 0, "32 comparators or no-allocation range contract");
    strcat(range, " >=1.0.0");
    check(!xsemver_satisfies("1.2.3+meta.000", range) && xrtGetError() == NULL, "33 comparators must reject");
    check(xsemver_satisfies("1.2.3-rc.1+meta.000", ">=1.2.3-rc.1+other <1.2.3"), "metadata affects range");
}

int main(void)
{
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE, "test.semver", 0, "warm diagnostics");
    xrtClearError();
    metadata_matrix();
    ownership_faults();
    range_boundaries();
    check(sizeof(ComparatorSet) <= 12u * 1024u, "range scratch retains unused metadata");
    printf("semver metadata: %u cases, %u allocation faults, %u failures\n", cases, faults, failures);
    printf("semver private layout: version=%zu comparator-set=%zu\n", sizeof(SemVer), sizeof(ComparatorSet));
    return failures == 0u ? 0 : 1;
}
