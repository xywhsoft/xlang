#ifndef XLANG_ANALYSIS_CONTEXT_H
#define XLANG_ANALYSIS_CONTEXT_H
#include <stddef.h>
#include <stdint.h>

typedef struct xls_context xls_context;
enum xls_context_kind { XLS_CONTEXT_XLANG=1, XLS_CONTEXT_C, XLS_CONTEXT_PROJECT };
enum xls_context_list {
    XLS_CONTEXT_MEMBERS, XLS_CONTEXT_C_SOURCES, XLS_CONTEXT_RESOURCES,
    XLS_CONTEXT_INCLUDES, XLS_CONTEXT_LIBRARY_PATHS, XLS_CONTEXT_LINK_LIBRARIES,
    XLS_CONTEXT_C_SYSTEM_INCLUDES, XLS_CONTEXT_C_FORCED_INCLUDES,
    XLS_CONTEXT_C_DEFINES, XLS_CONTEXT_C_UNDEFINES, XLS_CONTEXT_LIST_COUNT
};
typedef struct xls_context_strings { const char* const* items; size_t count; } xls_context_strings;
typedef struct xls_context_spec {
    enum xls_context_kind kind;
    const char *project,*configuration,*entry,*directory,*analysisProfile,*targetKind;
    /* NULL means absent; an explicit empty environment value remains distinct. */
    const char* packagePath;
    xls_context_strings lists[XLS_CONTEXT_LIST_COUNT];
} xls_context_spec;
enum xls_context_error { XLS_CONTEXT_OK, XLS_CONTEXT_ARGUMENT, XLS_CONTEXT_ENCODING, XLS_CONTEXT_LIMIT, XLS_CONTEXT_MEMORY };

/* Immutable analysis identity, not a parsed manifest or a checked compilation.
 * Paths/IDs are caller-canonicalized UTF-8. Lists retain order and duplicates.
 * analysisProfile identifies the actual analysis assets/options, not merely the
 * IDE's selected execution compiler. Input contents/overlays need their own
 * existing input proofs. No filesystem, environment, CWD or transport access.
 * One XRT allocation, at most 16 MiB including pointers/metadata; at most 65536
 * list entries in total, each string at most 262144 bytes. Failure owns nothing.
 * This API does not make the current semantic backend honor project options. */
xls_context* xlsContextCreate(const xls_context_spec* spec,size_t byteLimit,enum xls_context_error* error);
xls_context* xlsContextRetain(xls_context* context);
void xlsContextRelease(xls_context* context);
/* Borrowed immutable view, valid while retaining context; no query allocation. */
const xls_context_spec* xlsContextSpec(const xls_context* context);
size_t xlsContextBytes(const xls_context* context);
/* Process-local hash only. Equality checks the complete canonical bytes even
 * on a hash collision. Neither hash nor pointer is a persistent project ID. */
uint64_t xlsContextHash(const xls_context* context);
int xlsContextEqual(const xls_context* left,const xls_context* right);
#endif
