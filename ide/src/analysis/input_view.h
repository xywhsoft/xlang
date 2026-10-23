#ifndef XLANG_ANALYSIS_INPUT_VIEW_H
#define XLANG_ANALYSIS_INPUT_VIEW_H
#include <stddef.h>
/* Captured immutable rows shared by checks and cursor queries. Borrowed data,
 * not validation/I/O or a freshness proof. Text is UTF-8 after BOM removal,
 * preserving original CR/LF bytes. Disk raw fields include BOM; overlays use
 * post-BOM bytes in both fields. Source kind: source=1,manifest=2. Digests are
 * copied. Legacy check structure tags retain source/ABI compatibility. */
typedef struct xls_check_source {
    const char *path,*text,*origin;
    size_t size,rawSize;
    int kind;
    unsigned char textSha256[32],rawSha256[32];
} xls_input_source;
typedef struct xls_check_probe { const char* path;int exists; } xls_input_probe;
typedef struct xls_check_inputs { size_t sources,probes;const char* packagePath;int packagePathObserved; } xls_inputs;
#endif
