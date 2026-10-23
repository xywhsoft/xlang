#ifndef XLANG_ANALYSIS_FRONTEND_IMPORT_NAMES_H
#define XLANG_ANALYSIS_FRONTEND_IMPORT_NAMES_H
#include <stddef.h>
#include <stdint.h>
typedef struct xls_import_names_index xls_import_names_index;
typedef struct xls_import_names_work {
    uint64_t indexedRows,lookups,noDotSkips,fallbacks;
    size_t maxBytes;
} xls_import_names_work;
typedef struct xls_import_names_scope {
    struct xls_import_names_scope* previous;
    size_t bytes;
    xls_import_names_work work;
} xls_import_names_scope;
/* Private verification override. Default is 8 MiB of logical key/slot bytes
 * per transient original name map; zero selects the unchanged original scans.
 * XRT buckets are count bounded, not part of an RSS claim. One owner thread;
 * nested scopes restore the previous override. No transport/runtime execution. */
void xlsFrontendImportNamesBegin(xls_import_names_scope* scope,size_t bytes);
void xlsFrontendImportNamesEnd(xls_import_names_scope* scope,xls_import_names_work* work);
#endif
