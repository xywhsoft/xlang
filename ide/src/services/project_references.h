#ifndef XIDE_PROJECT_REFERENCES_H
#define XIDE_PROJECT_REFERENCES_H
#include "services/language_check.h"
#ifdef XLS_COMPILER_XRT
#include "analysis/module_store.h"
typedef struct xide_project_references xide_project_references;
typedef struct xide_reference_query xide_reference_query;
typedef struct xide_project_reference_stats {size_t roots,captured,reused,locations,records,chargedBytes;uint64_t thread;} xide_project_reference_stats;
enum xide_reference_query_state {XIDE_REFERENCES_FAILED=-1,XIDE_REFERENCES_PENDING=0,XIDE_REFERENCES_READY=1,XIDE_REFERENCES_CANCELLED=2};
/* One independent XRT actor/analysis worker;borrowed toolchain cache outlives
 * Destroy. Context membership defines ALL xlang roots,including unopened and
 * unimported members. Derived compiler contexts preserve every semantic build
 * option;the scheduler retains membership once,not in every compiler child.
 * Up to65538 byte-budgeted LRU shards,64 shared immutable scopes,
 *128MiB known retained charge,8 queued requests. Charged source
 * payloads/context/encoded reports are not a process RSS/transient budget.
 * Destroy cancels/joins work;retained READY result values survive the store. */
xide_project_references* xideProjectReferencesCreate(xide_toolchains* cache,const char* compiler,xls_module_store* modules);
void xideProjectReferencesDestroy(xide_project_references* store);
/* Captures immutable text references and bounded labels only;no caller disk
 * reads or compiler work. Source/config freshness is rechecked on the actor.
 * Caller still gates the result on current input/catalog/publication epochs.
 * No partial successful result on source errors,quota failure or cancellation.
 * Reference result JSON is bounded to7MiB before the shared8MiB RPC framer;
 * the remainder covers the bounded request ID and protocol envelope. */
xide_reference_query* xideProjectReferencesSubmit(xide_project_references* store,xls_context* context,
 const xls_analysis_document* documents,size_t count,const char* trigger,
 const char* path,int line,int character,int includeDeclaration);
enum xide_reference_query_state xideProjectReferencesResult(xide_reference_query* query,
 const xvalue** locations,xide_project_reference_stats* stats,char* error,size_t capacity);
xvalue* xideProjectReferencesTake(xide_reference_query* query);
int xideProjectReferencesSetWake(xide_reference_query* query,void (*wake)(void*),void* user);
void xideProjectReferencesCancel(xide_reference_query* query);
void xideProjectReferencesQueryDestroy(xide_reference_query* query);
#endif
#endif
