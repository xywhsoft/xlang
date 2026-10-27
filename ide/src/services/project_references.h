#ifndef XIDE_PROJECT_REFERENCES_H
#define XIDE_PROJECT_REFERENCES_H
#include "services/language_check.h"
#ifdef XLS_COMPILER_XRT
#include "analysis/module_store.h"
typedef struct xide_project_references xide_project_references;
typedef struct xide_reference_query xide_reference_query;
typedef struct xide_project_reference_stats {size_t roots,captured,reused,locations,records,chargedBytes;
 size_t keyActorBuilds,keyActorHits,keyActorSessions,keyNavigationBuilds,keyNavigationHits;
 /* Available bounded lanes,submitted member jobs,peak unconsumed jobs and
  * distinct successful semantic worker IDs;not simultaneous CPU execution. */
 size_t memberWorkers,memberTasks,memberPeak,memberThreads;uint64_t thread;
 /* Actual metadata requests/cached route facts;actor totals are cumulative.
  * coveredRoots are skipped imported entries,sourceRejections are attempted
  * program roles rejected by original guarded source diagnostics. Pinned
  * coverageCacheRoots still contribute complete checked shards this query. */
 size_t ownerProbes,ownerRouteHits,ownerActorBuilds,ownerActorHits;
 size_t coveredRoots,sourceRejections,coverageCacheRoots;} xide_project_reference_stats;
enum xide_reference_query_state {XIDE_REFERENCES_FAILED=-1,XIDE_REFERENCES_PENDING=0,XIDE_REFERENCES_READY=1,XIDE_REFERENCES_CANCELLED=2};
/* One independent XRT actor with separate retained key/member workers;borrowed toolchain cache outlives
 * Destroy. Context membership defines ALL xlang source members,including unopened and
 * unimported files. Actual successful source vectors cover imported members;
 * those files need not be checked again as executable programs. The explicit
 * configured primary entry always keeps its program check. Nonentry binding
 * ownership uses original dependency expansion or guarded cached full facts,
 * then still performs the selected owner's full semantic/index/IR check. Derived compiler contexts preserve every semantic build
 * option;the scheduler retains membership once,not in every compiler child.
 * Up to65538 byte-budgeted LRU shards,64 shared immutable scopes,
 *128MiB known shard charge,8 queued requests. Key worker keeps at most2
 * sessions/64MiB known inputs shared with dependency owner queries;at most4 member workers split8 sessions/64MiB
 * known retained input storage in total. Optional extra-worker failure uses
 * fewer lanes. At most4 member checks are submitted before ordered capture.
 * Query-local pins can retain the previous bounded shard cache while newer
 * entries are admitted. Active AST/IR/transient work,per-kind input history
 * and caller-held result memory are extra.
 * Each worker also owns a bounded navigation projection. Charged source
 * payloads/context/encoded reports are not a process RSS/transient budget.
 * Destroy cancels/joins work;retained READY result values survive the store. */
xide_project_references* xideProjectReferencesCreate(xide_toolchains* cache,const char* compiler,xls_module_store* modules);
void xideProjectReferencesDestroy(xide_project_references* store);
/* Captures immutable text references and bounded labels only;no caller disk
 * reads or compiler work. Source/config freshness is rechecked on the actor.
 * Caller still gates the result on current input/catalog/publication epochs.
 * No partial successful result on uncovered source errors,quota failure or
 * cancellation. Only original guarded source rejection may wait for a later
 * checked importer;infrastructure/quota/admission failures remain failures.
 * Reference result JSON is bounded to7MiB before the shared8MiB RPC framer;
 * the remainder covers the bounded request ID and protocol envelope. */
xide_reference_query* xideProjectReferencesSubmit(xide_project_references* store,xls_context* context,
 const xls_analysis_document* documents,size_t count,const char* trigger,
 const char* path,int line,int character,int includeDeclaration);
/* Optional negotiated Location.xlang {schema:1,document:...},using the same
 * document proof as guarded definitions. Zero preserves standard locations. */
xide_reference_query* xideProjectReferencesSubmitWithProof(xide_project_references* store,xls_context* context,
 const xls_analysis_document* documents,size_t count,const char* trigger,
 const char* path,int line,int character,int includeDeclaration,int inputProof);
enum xide_reference_query_state xideProjectReferencesResult(xide_reference_query* query,
 const xvalue** locations,xide_project_reference_stats* stats,char* error,size_t capacity);
xvalue* xideProjectReferencesTake(xide_reference_query* query);
int xideProjectReferencesSetWake(xide_reference_query* query,void (*wake)(void*),void* user);
void xideProjectReferencesCancel(xide_reference_query* query);
void xideProjectReferencesQueryDestroy(xide_reference_query* query);
#endif
#endif
