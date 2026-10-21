#ifndef XLANG_ANALYSIS_SESSION_H
#define XLANG_ANALYSIS_SESSION_H
#include "analysis/text.h"
#include <stdint.h>

typedef struct xls_session xls_session;
typedef struct xls_syntax xls_syntax;
typedef struct xls_session_limits { size_t documents,documentBytes,totalBytes; } xls_session_limits;
typedef struct xls_session_stats {
    uint64_t generation,parses,cacheHits;
    size_t documents,textBytes;
} xls_session_stats;
typedef struct xls_analysis_snapshot {
    xls_syntax* syntax;
    uint64_t identity,revision,generation;
} xls_analysis_snapshot;
/* Compiler coordinates are one-based Unicode scalar positions, not LSP UTF-16. */
typedef struct xls_syntax_diagnostic { int line,column; const char *path,*message; } xls_syntax_diagnostic;

/* A session has one owner thread. Published syntax snapshots can be retained
 * across edits, close and session destruction. No filesystem or transport. */
xls_session* xlsSessionCreate(const xls_session_limits* limits);
void xlsSessionDestroy(xls_session* session);
/* key is a caller-canonicalized document identity, path is its source label.
 * Source is borrowed and retained on success. Same-identity revisions cannot
 * decrease or change text at an existing revision. Validation is atomic. */
int xlsSessionUpdate(xls_session* session,const char* key,const char* path,
                     uint64_t identity,uint64_t revision,xls_text* source);
int xlsSessionClose(xls_session* session,const char* key);
/* Drop cached documents outside an immutable input set. Predicate borrows the
 * exact key bytes (not NUL-terminated); it must not mutate/reenter the session.
 * Removal keys are staged before mutation, so allocation failure preserves it. */
typedef int (*xls_session_live)(const char* key,size_t length,void* user);
int xlsSessionPrune(xls_session* session,xls_session_live live,void* user);
/* Parses only on demand; every successful call transfers one syntax reference.
 * Invalid source syntax is a result with Accepted == 0, not a failed query.
 * A failed query clears the output. This stage does not run semantic analysis. */
int xlsSessionSyntax(xls_session* session,const char* key,xls_analysis_snapshot* result);
void xlsSessionStats(const xls_session* session,xls_session_stats* stats);

xls_syntax* xlsSyntaxRetain(xls_syntax* syntax);
void xlsSyntaxRelease(xls_syntax* syntax);
int xlsSyntaxAccepted(const xls_syntax* syntax);
const char* xlsSyntaxPath(const xls_syntax* syntax);
const xls_text* xlsSyntaxText(const xls_syntax* syntax);
size_t xlsSyntaxDiagnosticCount(const xls_syntax* syntax);
/* Returned strings borrow the syntax snapshot. */
int xlsSyntaxDiagnostic(const xls_syntax* syntax,size_t index,xls_syntax_diagnostic* result);
#endif
