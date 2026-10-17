/* Private source-identity regression, not a substitute for real compiler
 * product gates. Allocation/path/I/O observers forward to the actual XRT
 * provider except for one explicit NULL result. Parser entry observers count
 * fallback and forward to the actual project parser without replacing it. */
#include "src/common/xl6_common.h"
#include "src/parser/xl6_parser_api.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned calls, failAt, failures, diskReads, parserEntries;
static unsigned failPath, pathCalls;
static void* observedMalloc(size_t size)
{
    if (failAt && ++calls == failAt) { failAt = 0; ++failures; return NULL; }
    return xrtMalloc(size);
}
static char* observedAbs(const char* path)
{
    if (failPath && ++pathCalls == failPath) { failPath = 0; ++failures; return NULL; }
    return xrtPathAbs(path);
}
static bool observedExists(str path) { ++diskReads; return xrtFileExists(path); }
static void armMalloc(void) { calls = 0; failAt = 1; failures = 0; diskReads = 0; }
static void armPath(void) { failPath = 1; pathCalls = 0; failures = 0; diskReads = 0; parserEntries = 0; }

#undef xrtMalloc
#define xrtMalloc observedMalloc
#define xrtPathAbs observedAbs
#define xrtFileExists observedExists
#include "src/common/xl6_path_identity.h"
#ifndef XL6_IDENTITY_REPORT_SOURCE
#define XL6_IDENTITY_REPORT_SOURCE "src/compile/xl6_source_report.c"
#endif
#ifndef XL6_IDENTITY_OVERLAY_SOURCE
#define XL6_IDENTITY_OVERLAY_SOURCE "src/compile/xl6_source_overlay.c"
#endif
#include XL6_IDENTITY_REPORT_SOURCE
#include XL6_IDENTITY_OVERLAY_SOURCE
#include "src/sem/xl6_completion.c"
#ifndef XL6_IDENTITY_IMPORT_SOURCE
#define XL6_IDENTITY_IMPORT_SOURCE "src/compile/xl6_imports.c"
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic push
/* The full import implementation retains two unrelated legacy helpers. */
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include XL6_IDENTITY_IMPORT_SOURCE
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#define xl6ParseFile identityRealParseFile
#define xl6ParseSourceWithCompletion identityRealParseSourceWithCompletion
#include "src/parser/xl6_parser_api.c"
#undef xl6ParseFile
#undef xl6ParseSourceWithCompletion
#undef xrtMalloc
#undef xrtPathAbs
#undef xrtFileExists

int xl6ParseFile(const char* path, XL6_ParseResult* result)
{
    ++parserEntries; return identityRealParseFile(path, result);
}
int xl6ParseSourceWithCompletion(const char* source, const char* path,
    XL6_ParseResult* result, struct XL6_CompletionQuery* query)
{
    ++parserEntries; return identityRealParseSourceWithCompletion(source, path, result, query);
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "source identity check failed at %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

static int lookupFailures(void)
{
    XL6_SourceOverlay overlay = {0};
    XL6_SourceReport report;
    XL6_ParseResult parsed = {0};
    XL6_ImportDeps deps = {0};
    char resolved[512];
    const char* memory = "int memoryValue = 2\n";
    FILE* file = fopen("shadow.xl", "wb");
    CHECK(file && fputs("int diskValue = 1\n", file) >= 0 && !fclose(file));
    CHECK(xl6SourceOverlaySet(&overlay, "shadow.xl", memory, strlen(memory)));
    CHECK(xl6SourceOverlayExists(&overlay, "shadow.xl") == 1);
    armPath();
    int status = xl6SourceOverlayExists(&overlay, "shadow.xl");
    if (status >= 0 || diskReads) fprintf(stderr,
        "unsafe source fallback: status=%d diskReads=%u injected=%u\n", status, diskReads, failures);
    CHECK(status < 0);
    CHECK(failures == 1 && diskReads == 0 && !failPath);
    /* The failure is transient: normal lookup succeeds on the next call. */
    CHECK(xl6SourceOverlayExists(&overlay, "shadow.xl") == 1);
    armPath();
    CHECK(!xl6SourceOverlayParse(&overlay, "shadow.xl", &parsed));
    CHECK(failures == 1 && parserEntries == 0 && diskReads == 0);
    xl6ParseResultFree(&parsed);
    deps.sourceOverlay = &overlay;
    armPath();
    status = xl6ImportsResolveSourcePath(&deps, "entry.xl", "shadow", resolved, sizeof(resolved));
    if (status >= 0) fprintf(stderr, "unsafe import resolution: status=%d injected=%u\n", status, failures);
    CHECK(status < 0 && failures == 1 && diskReads == 0);
    CHECK(xl6ImportsResolveSourcePath(&deps, "entry.xl", "shadow", resolved, sizeof(resolved)) == 1);
    xl6SourceReportInit(&report); overlay.report = &report;
    armPath();
    CHECK(xl6SourceOverlayExists(&overlay, "shadow.xl") < 0 && !report.complete);
    CHECK(!strcmp(report.error, "cannot resolve source overlay identity") && diskReads == 0);
    CHECK(xl6SourceOverlayExists(&overlay, "shadow.xl") < 0);
    overlay.report = NULL; xl6SourceReportFree(&report);
    xl6SourceOverlayClear(&overlay);
    /* Confirm a permitted absence fallback uses the real parser successfully. */
    CHECK(xl6SourceOverlayParse(&overlay, "shadow.xl", &parsed));
    CHECK(parserEntries == 1 && parsed.root); xl6ParseResultFree(&parsed);
    CHECK(!remove("shadow.xl"));
    return 0;
}

static int comparisonFailures(void)
{
#ifdef _WIN32
    XL6_SourceOverlay overlay = {0};
    XL6_SourceReport report;
    char upper[640], lower[640];
    memset(upper, 'A', sizeof(upper) - 1); upper[sizeof(upper) - 1] = 0;
    memcpy(lower, upper, sizeof(upper)); lower[sizeof(lower) - 2] = 'a';
    CHECK(xl6PathIdentityEqual(upper, lower) == 1);
    armMalloc(); CHECK(xl6PathIdentityEqual(upper, lower) < 0 && failures == 1);
    CHECK(xl6PathIdentityEqual(upper, lower) == 1);
    CHECK(xl6PathIdentityEqual("\xed\xa0\x80", "valid") < 0);
    CHECK(xl6SourceOverlaySet(&overlay, upper, "original", 8));
    char* original = overlay.items[0].text;
    armMalloc(); CHECK(!xl6SourceOverlaySet(&overlay, lower, "replacement", 11));
    CHECK(failures == 1 && overlay.count == 1 && overlay.items[0].text == original);
    CHECK(overlay.bytes == 8 && !strcmp(original, "original"));
    armMalloc(); CHECK(!xl6SourceOverlayRemove(&overlay, lower));
    CHECK(failures == 1 && overlay.count == 1 && overlay.items[0].text == original);
    xl6SourceOverlayClear(&overlay);
    xl6SourceReportInit(&report);
    CHECK(xl6SourceReportText(&report, upper, "memory", 6, "memory"));
    armMalloc(); CHECK(!xl6SourceReportText(&report, lower, "memory", 6, "memory"));
    CHECK(failures == 1 && !report.complete && report.count == 1);
    xl6SourceReportFree(&report);
    xl6SourceReportInit(&report); CHECK(!xl6SourceReportExists(&report, upper));
    CHECK(report.complete && report.probeCount == 1);
    armMalloc(); CHECK(!xl6SourceReportExists(&report, lower));
    CHECK(failures == 1 && !report.complete && report.probeCount == 1 && diskReads == 0);
    xl6SourceReportFree(&report);
#endif
    XL6_SourceReport appendReport;
    xl6SourceReportInit(&appendReport);
    armMalloc(); CHECK(!xl6SourceReportText(&appendReport, "memory.xl", "value", 5, "memory"));
    CHECK(failures == 1 && !appendReport.complete && !appendReport.count);
    xl6SourceReportFree(&appendReport);
    return 0;
}

#ifndef XL6_IDENTITY_OLD_CONTROL
static int completionFailures(void)
{
    XL6_SourceReport report;
    XL6_CompletionQuery query = {0};
    XL6_AstSource selectionSource = {0};
    XL6_AstNode selection = {0};
    char* json;
    xl6SourceReportInit(&report);
    CHECK(xl6SourceReportText(&report, "memory.xl", "value", 5, "memory"));
    query.path = report.items[0].path;
    query.available = query.parsed = query.reached = 1;
    selectionSource.path = report.items[0].path; selection.source = &selectionSource;
    CHECK(xl6CompletionAdd(&query, "value", "variable", 1, &selection, NULL));
    armPath();
    json = xl6CompletionJson(&query, &report);
    CHECK(json && !query.available && query.error && failures == 1);
    CHECK(strstr(json, "\"complete\":false") && strstr(json, "\"items\":[]"));
    xrtFree(json);
    /* Root succeeds, but a later item identity fails: no partial list. */
    query.available = 1; query.error = NULL;
    failPath = 2; pathCalls = 0; failures = 0;
    json = xl6CompletionJson(&query, &report);
    CHECK(json && !query.available && query.error && failures == 1 && !failPath);
    CHECK(strstr(json, "\"complete\":false") && strstr(json, "\"items\":[]"));
    xrtFree(json);
    /* A selection with invalid Windows UTF-8 must not serialize as unknown. */
#ifdef _WIN32
    query.available = 1; query.error = NULL;
    selectionSource.path = "\xed\xa0\x80";
    json = xl6CompletionJson(&query, &report);
    CHECK(json && !query.available && query.error);
    CHECK(strstr(json, "\"complete\":false") && strstr(json, "\"items\":[]"));
    xrtFree(json);
#endif
    selectionSource.path = NULL;
    xl6CompletionFree(&query); xl6SourceReportFree(&report);
    return 0;
}
#endif

int main(void)
{
    if (lookupFailures() || comparisonFailures()) return 1;
#ifndef XL6_IDENTITY_OLD_CONTROL
    if (completionFailures()) return 1;
#endif
    puts("source identity: PASS (one-shot failure; no disk/parser fallback; atomic snapshots)");
    return 0;
}
