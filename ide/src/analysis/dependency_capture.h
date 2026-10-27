
#ifndef XLANG_ANALYSIS_DEPENDENCY_CAPTURE_H
#define XLANG_ANALYSIS_DEPENDENCY_CAPTURE_H
#include "analysis/check.h"
typedef struct xls_entry_cache xls_entry_cache;
#include "analysis/module_graph.h"
typedef struct xls_dependency_capture xls_dependency_capture;
typedef struct xls_dependency_info {const char *path,*phase,*error;size_t diagnostics;int expanded;} xls_dependency_info;
/* Successful parsing/import/stdlib expansion ONLY,not semantic or IR success.
 * Uses the original checker preparation with the same immutable contexts,
 * input/report/probe guards,cancellation and existing neutral module assets.
 * AST is discarded before return. Owned sources and original diagnostics live
 * until final release;no GUI,transport,DLL,execution or process-global chdir.
 * NULL is cancellation/infrastructure failure. A rejected source can produce
 * expanded=0. Getters borrow this independent owner;Validate permanently
 * invalidates on a changed source/resolution/context. Concurrent readers
 * retain a reference;validation uses the original owner's lock. */
xls_dependency_capture* xlsDependencyCaptureCreate(xls_context* context,xls_entry_cache* cache,
 const char* entry,const xls_semantic_input* inputs,size_t count,
 int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_dependency_capture* xlsDependencyCaptureRetain(xls_dependency_capture*);
void xlsDependencyCaptureRelease(xls_dependency_capture*);
xls_dependency_info xlsDependencyCaptureInfo(const xls_dependency_capture*);
int xlsDependencyCaptureInputs(const xls_dependency_capture*,xls_check_inputs*);
int xlsDependencyCaptureSource(const xls_dependency_capture*,size_t,xls_check_source*);
int xlsDependencyCaptureProbe(const xls_dependency_capture*,size_t,xls_check_probe*);
int xlsDependencyCaptureDiagnostic(const xls_dependency_capture*,size_t,xls_check_diagnostic*);
const xls_module_graph* xlsDependencyCaptureModules(const xls_dependency_capture*);
int xlsDependencyCaptureValidate(xls_dependency_capture*);
#endif
