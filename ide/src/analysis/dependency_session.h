#ifndef XLANG_ANALYSIS_DEPENDENCY_SESSION_H
#define XLANG_ANALYSIS_DEPENDENCY_SESSION_H
#include "analysis/session.h"
#include "analysis/dependency_capture.h"
typedef struct xls_dependency_session xls_dependency_session;
typedef struct xls_dependency_session_stats {
    uint64_t builds,hits,invalidations;
    size_t documents,textBytes;
    uint64_t entryFullParses,entryFragmentParses,entryReusedDeclarations,entryParsedBytes,entrySyntaxHits;
    uint64_t inputScopeHits;
} xls_dependency_session_stats;
/* One owner,one retained dependency snapshot and original neutral parse aids.
 * Complete input batches use the same identity/revision/context/limit gates
 * as other sessions. Exact or originally observed-input/topology hits still
 * validate all disk,negative candidate and context facts. A hit does not rerun
 * semantic/IR work;neither does a dependency rebuild. expanded!=program valid.
 * NULL target with a context permits a disk entry,including zero overlays.
 * Failure/cancellation preserves accepted input/model state. Source-error
 * snapshots are ordinary results. Returned references outlive the session.
 * Input limits do not bound all source reports,parse aids,transients or RSS. */
xls_dependency_session* xlsDependencySessionCreate(const xls_session_limits* limits);
void xlsDependencySessionDestroy(xls_dependency_session* session);
void xlsDependencySessionStats(const xls_dependency_session* session,xls_dependency_session_stats* stats);
xls_dependency_capture* xlsDependencySessionCaptureInContext(xls_dependency_session* session,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
#endif
