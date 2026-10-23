#ifndef XIDE_DIAGNOSTIC_DEPENDENCIES_H
#define XIDE_DIAGNOSTIC_DEPENDENCIES_H
#include <stddef.h>
#include <wchar.h>
#include <xrt.h>

typedef struct xide_diagnostic_dependencies xide_diagnostic_dependencies;
typedef struct xide_diagnostic_dependency xide_diagnostic_dependency;
typedef struct xide_dependency_limits {
 size_t dictionaries,encodedBytes,pathsPerDictionary,bindings,bindingKeyBytes;
} xide_dependency_limits;
typedef struct xide_dependency_stats {
 size_t dictionaries,pending,bindings,encodedBytes,bindingKeyBytes;
} xide_dependency_stats;
enum xide_dependency_message {
 XIDE_DEPENDENCY_ERROR=0,XIDE_DEPENDENCY_PART=1,XIDE_DEPENDENCY_COMPLETE=2,
 XIDE_DEPENDENCY_REMOVED=3,XIDE_DEPENDENCY_CANCELLED=4
};

/* One connection, one worker owner. No UI, source text or filesystem access.
 * Defaults allow the old and new32MiB publications to overlap:8192 dictionaries,
 * 64MiB encoded params,163840 paths/set,8192 file bindings,16MiB native keys.
 * These are logical metadata budgets, not heap/RSS or outstanding-lease limits. */
xide_diagnostic_dependencies* xideDiagnosticDependenciesCreate(const xide_dependency_limits* limits);
void xideDiagnosticDependenciesDestroy(xide_diagnostic_dependencies* dependencies);
void xideDiagnosticDependenciesStats(const xide_diagnostic_dependencies* dependencies,xide_dependency_stats* stats);
const char* xideDiagnosticDependenciesError(const xide_diagnostic_dependencies* dependencies);
/* Borrows one xlang/diagnosticDependencies.params. Complete schema, bounded
 * encoding, ordered unique absolute paths, part order and digest are checked.
 * Failure/cancellation preserves all previously accepted contents/bindings.
 * Construction/hash/free are worker-only; no hard cancellation bound promised. */
enum xide_dependency_message xideDiagnosticDependenciesMessage(xide_diagnostic_dependencies* dependencies,
 const xvalue* params,int (*cancelled)(void*),void* user);
/* The file decoder calls this only after validating the publication envelope.
 * id==NULL withdraws that file; an unknown withdrawal is harmless. Native path
 * identity unifies URI spelling/case changes. Live bindings prevent removal.
 * This alone does NOT validate input generation, body or local model identity. */
int xideDiagnosticDependenciesReference(xide_diagnostic_dependencies* dependencies,const char* path,const char* id);
/* Borrowed, complete sets only. Retain before crossing workers or releasing a
 * registry entry; retained immutable sets outlive removal and registry destroy. */
const xide_diagnostic_dependency* xideDiagnosticDependenciesFind(const xide_diagnostic_dependencies* dependencies,const char* id);
xide_diagnostic_dependency* xideDiagnosticDependencyRetain(const xide_diagnostic_dependency* dependency);
/* Heavy cleanup: worker or final shutdown, never interactive UI dismissal. */
void xideDiagnosticDependencyRelease(xide_diagnostic_dependency* dependency);
const char* xideDiagnosticDependencyId(const xide_diagnostic_dependency* dependency);
size_t xideDiagnosticDependencyCount(const xide_diagnostic_dependency* dependency);
const char* xideDiagnosticDependencyPath(const xide_diagnostic_dependency* dependency,size_t index);
/* Prepared NativeWide key: binary search, no normalization, allocation or I/O.
 * Caller can prepare keys with the existing platform/path adapter off-thread. */
int xideDiagnosticDependencyContainsKey(const xide_diagnostic_dependency* dependency,const wchar_t* key);
#endif
