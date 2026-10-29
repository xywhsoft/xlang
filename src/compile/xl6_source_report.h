#ifndef XL6_SOURCE_REPORT_H
#define XL6_SOURCE_REPORT_H

#include "src/common/xl6_common.h"

#define XL6_SOURCE_REPORT_FILES 512u
#define XL6_SOURCE_REPORT_FILE_BYTES (8u * 1024u * 1024u)
#define XL6_SOURCE_REPORT_TOTAL_BYTES (64u * 1024u * 1024u)

enum { XL6_SOURCE_REPORT_SOURCE = 1, XL6_SOURCE_REPORT_MANIFEST = 2 };
typedef struct XL6_SourceReportItem {
    char* path;
    char* text;
    size_t size;
    size_t rawSize;
    unsigned char textSha256[32];
    unsigned char rawSha256[32];
    xfileinfo info;
    int kind;
    const char* origin; /* Static: disk, overlay, or memory. */
} XL6_SourceReportItem;

typedef struct XL6_ResolutionProbe { char* path; int exists; } XL6_ResolutionProbe;

typedef struct XL6_SourceReport {
    XL6_SourceReportItem* items;
    size_t count;
    size_t capacity;
    size_t bytes;
    int complete;
    char* errorPath;
    char error[192];
    XL6_ResolutionProbe* probes;
    size_t probeCount, probeCapacity, probeBytes;
    char* packagePath;
    int packagePathObserved;
} XL6_SourceReport;

void xl6SourceReportInit(XL6_SourceReport* report);
void xl6SourceReportFree(XL6_SourceReport* report);
/* Fail closed; records a bounded diagnostic even when its path copy fails. */
void xl6SourceReportInvalidate(XL6_SourceReport* report, const char* path, const char* error);
int xl6SourceReportText(XL6_SourceReport* report, const char* path,
                       const char* text, size_t size, const char* origin);
/* Reads, hashes, and decodes the same bytes; caller owns the returned text.
 * Repeated reads of a lexical path use the immutable first snapshot. */
char* xl6SourceReportReadText(XL6_SourceReport* report, const char* path,
                            int kind, size_t* size);
int xl6SourceReportValidate(XL6_SourceReport* report);
/* Observe the regular-file predicate used by xlang import/package resolution.
 * A repeated query sees the first answer; validation rejects later changes. */
int xl6SourceReportExists(XL6_SourceReport* report, const char* path);
char* xl6SourceReportPackagePath(XL6_SourceReport* report);

#endif
