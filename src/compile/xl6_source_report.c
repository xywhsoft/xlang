#include "src/compile/xl6_source_report.h"
#include "src/common/xl6_path_identity.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif

static char* xl6ReportCopy(const char* text, size_t size)
{
    char* copy = (char*)xrtMalloc(size + 1);
    if (copy) { memcpy(copy, text, size); copy[size] = 0; }
    return copy;
}

static int xl6ReportFail(XL6_SourceReport* report, const char* path, const char* error)
{
    if (report && report->complete) {
        report->complete = 0;
        if (path) report->errorPath = xl6ReportCopy(path, strlen(path));
        snprintf(report->error, sizeof(report->error), "%s", error);
    }
    return 0;
}

void xl6SourceReportInvalidate(XL6_SourceReport* report, const char* path, const char* error)
{
    xl6ReportFail(report, path, error);
}

static XL6_SourceReportItem* xl6ReportFind(XL6_SourceReport* report, const char* path)
{
    size_t i;
    for (i = 0; i < report->count; ++i) {
        int equal = xl6PathIdentityEqual(report->items[i].path, path);
        if (equal < 0) { xl6ReportFail(report, path, "cannot compare source snapshot identity"); return NULL; }
        if (equal) return &report->items[i];
    }
    return NULL;
}

/* Required identity/size/time are read from the same handle as the bytes.
 * Checking the path too detects replacement while that handle remains open. */
static int xl6ReportInfoEqual(const xfileinfo* a, const xfileinfo* b)
{
    uint32 required = XFILE_INFO_SIZE | XFILE_INFO_MODIFY_TIME | XFILE_INFO_IDENTITY;
    return a->Type == XFILE_TYPE_FILE && b->Type == XFILE_TYPE_FILE &&
           (a->Available & required) == required && (b->Available & required) == required &&
           a->Size == b->Size && a->Device == b->Device && a->Identity == b->Identity &&
           a->Modified == b->Modified && a->Attributes == b->Attributes &&
           a->Available == b->Available &&
           (!(a->Available & XFILE_INFO_CHANGE_TIME) || a->Changed == b->Changed);
}

static char* xl6ReportReadRaw(const char* path, xfileinfo* info, size_t* size)
{
    xfile file = xrtOpen(path, XFILE_READ);
    xfileinfo first, last, named;
    char* raw = NULL;
    size_t done = 0, probeSize = 0;
    unsigned char probe;
    int ok = file != NULL && xrtFileStat(file, &first) &&
             xl6ReportInfoEqual(&first, &first) &&
             first.Size <= XL6_SOURCE_REPORT_FILE_BYTES;
    if (ok) { raw = (char*)xrtMalloc((size_t)first.Size + 1); ok = raw != NULL; }
    if (ok) ok = xrtReadFull(file, raw, (size_t)first.Size, &done) && done == first.Size &&
                 xrtRead(file, &probe, 1, &probeSize) && probeSize == 0 &&
                 xrtFileStat(file, &last) && xl6ReportInfoEqual(&first, &last) &&
                 xrtPathStat(path, true, &named) && xl6ReportInfoEqual(&first, &named);
    if (file && !xrtClose(file)) ok = 0;
    if (!ok) { xrtFree(raw); return NULL; }
    raw[done] = 0; *size = done; *info = first;
    return raw;
}

static XL6_SourceReportItem* xl6ReportAppend(XL6_SourceReport* report, const char* path,
    const char* text, size_t size, size_t rawSize, const unsigned char rawHash[32],
    const xfileinfo* info, int kind, const char* origin)
{
    XL6_SourceReportItem item;
    size_t capacity;
    if (report->count >= XL6_SOURCE_REPORT_FILES || size > XL6_SOURCE_REPORT_FILE_BYTES ||
        size > XL6_SOURCE_REPORT_TOTAL_BYTES - report->bytes) {
        xl6ReportFail(report, path, "source report snapshot limit exceeded"); return NULL;
    }
    memset(&item, 0, sizeof(item));
    item.path = xl6ReportCopy(path, strlen(path));
    item.text = xl6ReportCopy(text, size);
    item.size = size; item.rawSize = rawSize; item.origin = origin; item.kind = kind;
    if (info) item.info = *info;
    if (!item.path || !item.text || !xrtSha256(text, size, item.textSha256)) {
        xrtFree(item.path); xrtFree(item.text);
        xl6ReportFail(report, path, "cannot retain source report snapshot"); return NULL;
    }
    memcpy(item.rawSha256, rawHash ? rawHash : item.textSha256, 32);
    if (report->count == report->capacity) {
        XL6_SourceReportItem* items;
        capacity = report->capacity ? report->capacity * 2 : 16;
        items = (XL6_SourceReportItem*)xrtRealloc(report->items, capacity * sizeof(*items));
        if (!items) {
            xrtFree(item.path); xrtFree(item.text);
            xl6ReportFail(report, path, "cannot grow source report"); return NULL;
        }
        report->items = items; report->capacity = capacity;
    }
    report->items[report->count] = item;
    report->bytes += size;
    return &report->items[report->count++];
}

void xl6SourceReportInit(XL6_SourceReport* report)
{
    if (report) { memset(report, 0, sizeof(*report)); report->complete = 1; }
}

void xl6SourceReportFree(XL6_SourceReport* report)
{
    size_t i;
    if (!report) return;
    for (i = 0; i < report->count; ++i) {
        xrtFree(report->items[i].path); xrtFree(report->items[i].text);
    }
    for (i = 0; i < report->probeCount; ++i) xrtFree(report->probes[i].path);
    xrtFree(report->probes); xrtFree(report->packagePath);
    xrtFree(report->items); xrtFree(report->errorPath);
    memset(report, 0, sizeof(*report));
}

int xl6SourceReportText(XL6_SourceReport* report, const char* path,
                       const char* text, size_t size, const char* origin)
{
    char* absolute;
    XL6_SourceReportItem* item;
    int ok;
    if (!report) return 1;
    if (!report->complete) return 0;
    if (!path || !*path || !text || !origin ||
        (strcmp(origin, "overlay") && strcmp(origin, "memory")))
        return xl6ReportFail(report, path, "invalid source report text");
    if (size > XL6_SOURCE_REPORT_FILE_BYTES || memchr(text, 0, size) ||
        !xrtUtf8Valid(xrtStrViewN(text, size), NULL))
        return xl6ReportFail(report, path, "source report text must be bounded strict UTF-8 without NUL");
    absolute = xrtPathAbs(path);
    if (!absolute) return xl6ReportFail(report, path, "cannot resolve source report path");
    item = xl6ReportFind(report, absolute);
    if (!report->complete) { xrtFree(absolute); return 0; }
    if (item) ok = item->size == size && !strcmp(item->origin, origin) &&
                   !memcmp(item->text, text, size);
    else ok = xl6ReportAppend(report, absolute, text, size, size, NULL, NULL,
                             XL6_SOURCE_REPORT_SOURCE, origin) != NULL;
    if (!ok) xl6ReportFail(report, absolute, "source path was read from different snapshots");
    xrtFree(absolute); return ok;
}

char* xl6SourceReportReadText(XL6_SourceReport* report, const char* path, int kind, size_t* size)
{
    char *absolute, *raw, *copy;
    size_t rawSize, bomSize = 0;
    xfileinfo info;
    xencoding bom;
    unsigned char rawHash[32];
    XL6_SourceReportItem* item;
    if (size) *size = 0;
    if (!report) return xrtFileReadText(path, XENCODING_UTF8, XUTF_STRICT, size);
    if (!report->complete) return NULL;
    if (!path || !*path || !(kind & (XL6_SOURCE_REPORT_SOURCE | XL6_SOURCE_REPORT_MANIFEST))) {
        xl6ReportFail(report, path, "invalid source report disk read"); return NULL;
    }
    absolute = xrtPathAbs(path);
    if (!absolute) { xl6ReportFail(report, path, "cannot resolve source report path"); return NULL; }
    item = xl6ReportFind(report, absolute);
    if (!report->complete) { xrtFree(absolute); return NULL; }
    if (item) {
        if (strcmp(item->origin, "disk")) {
            xl6ReportFail(report, absolute, "disk read conflicts with text snapshot"); xrtFree(absolute); return NULL;
        }
        item->kind |= kind;
        copy = xl6ReportCopy(item->text, item->size);
        if (copy && size) *size = item->size;
        if (!copy) xl6ReportFail(report, absolute, "cannot copy source snapshot");
        xrtFree(absolute); return copy;
    }
    if (report->count >= XL6_SOURCE_REPORT_FILES) {
        xl6ReportFail(report, absolute, "source report file limit exceeded"); xrtFree(absolute); return NULL;
    }
    raw = xl6ReportReadRaw(absolute, &info, &rawSize);
    if (!raw) { xl6ReportFail(report, absolute, "cannot read a stable bounded regular source file"); xrtFree(absolute); return NULL; }
    bom = xrtEncodingBom((xbytesview){(bytes)raw, rawSize}, &bomSize);
    if ((bom != XENCODING_UNKNOWN && bom != XENCODING_UTF8) ||
        memchr(raw + bomSize, 0, rawSize - bomSize) ||
        !xrtUtf8Valid(xrtStrViewN(raw + bomSize, rawSize - bomSize), NULL) ||
        !xrtSha256(raw, rawSize, rawHash)) {
        xl6ReportFail(report, absolute, "source file must be strict UTF-8 without NUL");
        xrtFree(raw); xrtFree(absolute); return NULL;
    }
    item = xl6ReportAppend(report, absolute, raw + bomSize, rawSize - bomSize,
                          rawSize, rawHash, &info, kind, "disk");
    xrtFree(raw);
    copy = item ? xl6ReportCopy(item->text, item->size) : NULL;
    if (copy && size) *size = item->size;
    if (item && !copy) xl6ReportFail(report, absolute, "cannot copy source snapshot");
    xrtFree(absolute); return copy;
}

int xl6SourceReportValidate(XL6_SourceReport* report)
{
    size_t i;
    if (!report || !report->complete) return 0;
    for (i = 0; i < report->probeCount; ++i) {
        XL6_ResolutionProbe* probe = &report->probes[i];
        if ((xrtFileExists(probe->path) != 0) != probe->exists)
            return xl6ReportFail(report, probe->path, "import candidate changed during analysis");
    }
    if (report->packagePathObserved) {
        char* value = (char*)xrtEnvGet((str)"XLANG_PACKAGE_PATH");
        int same = value && report->packagePath ? !strcmp(value, report->packagePath) : value == report->packagePath;
        xrtFree(value);
        if (!same) return xl6ReportFail(report, NULL, "XLANG_PACKAGE_PATH changed during analysis");
    }
    for (i = 0; i < report->count; ++i) {
        XL6_SourceReportItem* item = &report->items[i];
        xfileinfo info;
        size_t size;
        unsigned char hash[32];
        char* raw;
        int ok;
        if (strcmp(item->origin, "disk")) continue;
        raw = xl6ReportReadRaw(item->path, &info, &size);
        ok = raw && size == item->rawSize && xl6ReportInfoEqual(&item->info, &info) &&
             xrtSha256(raw, size, hash) && !memcmp(item->rawSha256, hash, 32);
        xrtFree(raw);
        if (!ok) return xl6ReportFail(report, item->path, "source file changed during analysis");
    }
    return 1;
}

int xl6SourceReportExists(XL6_SourceReport* report, const char* path)
{
    char* absolute;
    size_t i, size;
    int exists;
    if (!report) return xrtFileExists((char*)path);
    if (!report->complete || !path || !*path) return 0;
    absolute = xrtPathAbs(path);
    if (!absolute) return xl6ReportFail(report, path, "cannot resolve import candidate");
    for (i = 0; i < report->probeCount; ++i) {
        int equal = xl6PathIdentityEqual(report->probes[i].path, absolute);
        if (equal < 0) {
            xl6ReportFail(report, absolute, "cannot compare import candidate identity");
            xrtFree(absolute); return 0;
        }
        if (equal) {
            exists = report->probes[i].exists; xrtFree(absolute); return exists;
        }
    }
    size = strlen(absolute) + 1;
    if (report->probeCount >= 2048u || size > 1024u * 1024u - report->probeBytes) {
        xl6ReportFail(report, absolute, "import candidate report limit exceeded"); xrtFree(absolute); return 0;
    }
    if (report->probeCount == report->probeCapacity) {
        size_t capacity = report->probeCapacity ? report->probeCapacity * 2 : 32;
        XL6_ResolutionProbe* probes = (XL6_ResolutionProbe*)xrtRealloc(report->probes, capacity * sizeof(*probes));
        if (!probes) { xl6ReportFail(report, absolute, "cannot retain import candidate"); xrtFree(absolute); return 0; }
        report->probes = probes; report->probeCapacity = capacity;
    }
    exists = xrtFileExists(absolute) != 0;
    report->probes[report->probeCount++] = (XL6_ResolutionProbe){absolute, exists};
    report->probeBytes += size;
    return exists;
}

char* xl6SourceReportPackagePath(XL6_SourceReport* report)
{
    char* value;
    if (!report) return (char*)xrtEnvGet((str)"XLANG_PACKAGE_PATH");
    if (!report->complete) return NULL;
    if (!report->packagePathObserved) {
        value = (char*)xrtEnvGet((str)"XLANG_PACKAGE_PATH");
        if (value && (strlen(value) > 32767u || !xrtUtf8Valid(xrtStrView(value), NULL))) {
            xrtFree(value); xl6ReportFail(report, NULL, "invalid XLANG_PACKAGE_PATH"); return NULL;
        }
        report->packagePath = value; report->packagePathObserved = 1;
    }
    value = report->packagePath ? xl6ReportCopy(report->packagePath, strlen(report->packagePath)) : NULL;
    if (report->packagePath && !value) xl6ReportFail(report, NULL, "cannot retain XLANG_PACKAGE_PATH");
    return value;
}
