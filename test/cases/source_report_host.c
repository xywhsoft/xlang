/* Deterministic snapshot tests use a single externally owned XRT DLL and
 * mutate only files created for this test. No embedded XRT implementation. */
#include "src/compile/xl6_source_report.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define REQUIRE(c) do { if (!(c)) { fprintf(stderr, "source_report_host:%d: %s\n", __LINE__, #c); exit(1); } } while (0)
static char* utf8(const wchar_t* wide)
{
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, NULL, 0, NULL, NULL);
    char* text = (char*)malloc((size_t)n); REQUIRE(n > 0 && text);
    REQUIRE(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, text, n, NULL, NULL));
    return text;
}
static void put(const wchar_t* path, const char* data, size_t size)
{
    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD wrote; REQUIRE(file != INVALID_HANDLE_VALUE && size <= MAXDWORD);
    REQUIRE(WriteFile(file, data, (DWORD)size, &wrote, NULL) && wrote == size);
    REQUIRE(CloseHandle(file));
}
int wmain(int argc, wchar_t** argv)
{
    XL6_SourceReport report;
    wchar_t file[4096], replacement[4096];
    char *path, *other, *text, *large;
    size_t size;
    unsigned char hash[32];
    REQUIRE(argc == 2);
    REQUIRE(swprintf(file, 4096, L"%ls\\快照 Ω.xl", argv[1]) > 0);
    REQUIRE(swprintf(replacement, 4096, L"%ls\\replacement.xl", argv[1]) > 0);
    path = utf8(file); other = utf8(replacement);
    put(file, "\xef\xbb\xbf" "hello\r\n", 10);
    xl6SourceReportInit(&report);
    text = xl6SourceReportReadText(&report, path, XL6_SOURCE_REPORT_SOURCE, &size);
    REQUIRE(text && size == 7 && !memcmp(text, "hello\r\n", 7)); xrtFree(text);
    REQUIRE(report.count == 1 && report.items[0].rawSize == 10 && report.items[0].size == 7);
    REQUIRE(xrtSha256("hello\r\n", 7, hash) && !memcmp(hash, report.items[0].textSha256, 32));
    REQUIRE(xrtSha256("\xef\xbb\xbf" "hello\r\n", 10, hash) && !memcmp(hash, report.items[0].rawSha256, 32));
    REQUIRE(xl6SourceReportValidate(&report));
    /* Same path, new bytes: later reads stay on the first snapshot, and the
     * final validation rejects the analysis rather than combining versions. */
    put(file, "later", 5);
    text = xl6SourceReportReadText(&report, path, XL6_SOURCE_REPORT_MANIFEST, &size);
    REQUIRE(text && size == 7 && !memcmp(text, "hello\r\n", 7)); xrtFree(text);
    REQUIRE(report.count == 1 && report.items[0].kind == 3);
    REQUIRE(!xl6SourceReportValidate(&report) && !report.complete && report.errorPath);
    REQUIRE(!xl6SourceReportReadText(&report, path, XL6_SOURCE_REPORT_SOURCE, &size));
    xl6SourceReportFree(&report);
    /* Atomic same-byte replacement must still invalidate physical identity. */
    put(file, "same", 4); put(replacement, "same", 4);
    xl6SourceReportInit(&report);
    text = xl6SourceReportReadText(&report, path, XL6_SOURCE_REPORT_SOURCE, &size); REQUIRE(text); xrtFree(text);
    REQUIRE(MoveFileExW(replacement, file, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH));
    REQUIRE(!xl6SourceReportValidate(&report)); xl6SourceReportFree(&report);
    /* File deletion, malformed UTF-8, NUL, directory, and over-limit reads. */
    xl6SourceReportInit(&report);
    text = xl6SourceReportReadText(&report, path, 1, &size); REQUIRE(text); xrtFree(text);
    REQUIRE(DeleteFileW(file)); REQUIRE(!xl6SourceReportValidate(&report)); xl6SourceReportFree(&report);
    put(file, "\xed\xa0\x80", 3); xl6SourceReportInit(&report);
    REQUIRE(!xl6SourceReportReadText(&report, path, 1, &size) && !report.complete); xl6SourceReportFree(&report);
    put(file, "a\0b", 3); xl6SourceReportInit(&report);
    REQUIRE(!xl6SourceReportReadText(&report, path, 1, &size) && !report.complete); xl6SourceReportFree(&report);
    put(file, "\xff\xfe" "a\0", 4); xl6SourceReportInit(&report);
    REQUIRE(!xl6SourceReportReadText(&report, path, 1, &size)); xl6SourceReportFree(&report);
    xl6SourceReportInit(&report); text = utf8(argv[1]);
    REQUIRE(!xl6SourceReportReadText(&report, text, 1, &size)); free(text); xl6SourceReportFree(&report);
    large = (char*)malloc(XL6_SOURCE_REPORT_FILE_BYTES + 1); REQUIRE(large);
    memset(large, ' ', XL6_SOURCE_REPORT_FILE_BYTES + 1);
    put(file, large, XL6_SOURCE_REPORT_FILE_BYTES + 1); xl6SourceReportInit(&report);
    REQUIRE(!xl6SourceReportReadText(&report, path, 1, &size)); xl6SourceReportFree(&report);
    /* Exact maximum accepted, including empty files and Unicode case aliases. */
    put(file, "", 0); xl6SourceReportInit(&report);
    text = xl6SourceReportReadText(&report, path, 1, &size); REQUIRE(text && !size); xrtFree(text);
    REQUIRE(xl6SourceReportValidate(&report)); xl6SourceReportFree(&report);
    xl6SourceReportInit(&report);
    REQUIRE(xl6SourceReportText(&report, "子/Ω.xl", "text", 4, "overlay"));
    REQUIRE(xl6SourceReportText(&report, "子/ω.XL", "text", 4, "overlay") && report.count == 1);
    REQUIRE(xl6SourceReportValidate(&report));
    REQUIRE(!xl6SourceReportText(&report, "子/Ω.xl", "changed", 7, "overlay")); xl6SourceReportFree(&report);
    xl6SourceReportInit(&report);
    REQUIRE(xl6SourceReportText(&report, path, "", 0, "overlay"));
    REQUIRE(!xl6SourceReportReadText(&report, path, 1, &size)); xl6SourceReportFree(&report);
    xl6SourceReportInit(&report);
    for (unsigned i = 0; i < XL6_SOURCE_REPORT_FILES; ++i) {
        char name[64]; snprintf(name, sizeof(name), "virtual-%u.xl", i);
        REQUIRE(xl6SourceReportText(&report, name, "", 0, "overlay"));
    }
    REQUIRE(report.count == 512 && xl6SourceReportValidate(&report));
    REQUIRE(!xl6SourceReportText(&report, "over-count.xl", "", 0, "overlay")); xl6SourceReportFree(&report);
    xl6SourceReportInit(&report);
    for (int i = 0; i < 8; ++i) {
        char name[64]; snprintf(name, sizeof(name), "large-%d.xl", i);
        REQUIRE(xl6SourceReportText(&report, name, large, XL6_SOURCE_REPORT_FILE_BYTES, "overlay"));
    }
    REQUIRE(report.bytes == XL6_SOURCE_REPORT_TOTAL_BYTES && xl6SourceReportValidate(&report));
    REQUIRE(!xl6SourceReportText(&report, "over-total.xl", " ", 1, "overlay")); xl6SourceReportFree(&report);
    free(large); free(path); free(other);
    puts("source report snapshot contract: passed"); return 0;
}
