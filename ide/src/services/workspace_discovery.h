#ifndef XIDE_WORKSPACE_DISCOVERY_H
#define XIDE_WORKSPACE_DISCOVERY_H
#include "protocol/workspace_folders.h"
#include "services/analysis_catalog.h"
#ifdef XLS_COMPILER_XRT
typedef struct xide_workspace_discovery xide_workspace_discovery;
typedef struct xide_discovery_query xide_discovery_query;
typedef struct xide_discovery_snapshot xide_discovery_snapshot;
typedef struct xide_discovery_limits {size_t directories,entries,labels,manifestBytes;} xide_discovery_limits;
typedef struct xide_discovery_work {
 /* First discovery pass;source candidates exclude unenumerated project-only
    members. Final validation repeats directory/manifest observation. */
 size_t directories,entries,sources,manifests,manifestBytes,projects,labelBytes;
 uint64_t workerThread;
} xide_discovery_work;
enum xide_discovery_state {XIDE_DISCOVERY_FAILED=-1,XIDE_DISCOVERY_PENDING=0,
 XIDE_DISCOVERY_READY=1,XIDE_DISCOVERY_CANCELLED=2,XIDE_DISCOVERY_STALE=3};
/* One existing XRT executor,one running/one queued discovery. Zero limits select
 * defaults/maxima:4096 directories,131072 entries,16MiB labels/manifest bytes.
 * No parser or source-body analysis. Reuse IDE .xlw/.xlp decoders and contexts.
 * Submit retains roots/copies package metadata only;all filesystem work is on
 * the worker. NULL package differs from explicit empty. No cwd/env mutation.
 * Normal ignored child directories:.git/.hg/.svn,extlibs,node_modules,build,
 * release,release-materials,.xlang-ide,.xlang-update,update_info,.cache.
 * Explicit roots/manifest references still apply. Unexcluded links and aliased
 * directory roots fail rather than silently dropping members. */
xide_workspace_discovery* xideWorkspaceDiscoveryCreate(const xide_discovery_limits*,xide_model_error*);
void xideWorkspaceDiscoveryDestroy(xide_workspace_discovery*);
/* Host excludes submissions;native private idle predicate includes finalizers.
 * This module belongs to the compiler-XRT service,not the GUI's XGE runtime. */
int xideWorkspaceDiscoveryIdle(xide_workspace_discovery*);
/* Pure lexical watcher filter. Explicit nested roots override ignored parent
 * children; paths outside roots stay relevant for external manifest references.
 * No filesystem, alias or directory-type probes. Allocation failure is relevant. */
int xideWorkspaceDiscoveryRelevantPath(const xide_workspace_folders*,const char*);
xide_discovery_query* xideWorkspaceDiscoverySubmit(xide_workspace_discovery*,xide_workspace_folders*,const char* packagePath);
int xideWorkspaceDiscoveryResult(xide_discovery_query*,const xide_discovery_snapshot**,xide_discovery_work*,char*,size_t);
xide_discovery_snapshot* xideWorkspaceDiscoveryTake(xide_discovery_query*);
int xideWorkspaceDiscoverySetWake(xide_discovery_query*,void (*wake)(void*),void*);
void xideWorkspaceDiscoveryCancel(xide_discovery_query*);
/* Cancellation is cooperative;Result can remain pending while a reader is
 * active. Releasing a query does not replace physical Idle acknowledgement. */
void xideWorkspaceDiscoveryQueryDestroy(xide_discovery_query*);
xide_discovery_snapshot* xideDiscoverySnapshotRetain(xide_discovery_snapshot*);
void xideDiscoverySnapshotRelease(xide_discovery_snapshot*);
/* Borrowed immutable metadata,not source/semantic freshness or write authority. */
xide_analysis_catalog* xideDiscoverySnapshotCatalog(const xide_discovery_snapshot*);
const xvalue* xideDiscoverySnapshotValue(const xide_discovery_snapshot*);
/* Analysis metadata digest only:canonical project order,not raw manifests,
 * current source bytes,membership freshness or publication authority. */
const unsigned char* xideDiscoverySnapshotFingerprint(const xide_discovery_snapshot*);
/* Scalar observation only. Worker proof failures permanently set this flag;
 * observing it performs no filesystem work and does not validate freshness. */
int xideDiscoverySnapshotInvalidated(const xide_discovery_snapshot*);
/* Worker-only. Re-enumerate relevant names/types and rehash original manifest
 * bytes with exact file identities;directory timestamps are not membership
 * proof. Source content edits are left to existing native source guards. A
 * source/IO failure permanently invalidates this snapshot;cancel does not.
 * Caller retains snapshot and fences root/config/overlay revisions separately.
 * No atomic filesystem snapshot or automatic protocol publication is implied. */
int xideDiscoverySnapshotValidate(xide_discovery_snapshot*,int (*cancelled)(void*),void*,xide_model_error*);
#endif
#endif
