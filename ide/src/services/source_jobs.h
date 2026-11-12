#ifndef XIDE_SOURCE_JOBS_H
#define XIDE_SOURCE_JOBS_H
#include <stddef.h>
#include <stdint.h>
typedef struct xide_model_error xide_model_error;
typedef struct xide_source_jobs xide_source_jobs;
/* Intrusive owner metadata, not a thread/resource owner. Zero-initialize once;
 * serialize every API on the UI owner. Storage outlives all entries/leases.
 * Neither entries nor tokens cross initialized owner lifetimes. */
typedef struct xide_source_job {
    struct xide_source_job *previous,*next;
    xide_source_jobs* owner;
    void* thread;
} xide_source_job;
struct xide_source_jobs {
    xide_source_job* first;
    uint64_t serial,token;
    size_t count;
    unsigned admissions;
    int invalidated;
};
typedef struct xide_source_jobs_status {int quiet,invalidated;size_t registered;} xide_source_jobs_status;
/* Balanced synchronous construction admission. Begin precedes allocation,
 * path preparation and callbacks. Suspend refuses a reentrant Begin scope. */
int xideSourceJobsBegin(xide_source_jobs*,xide_model_error*);
void xideSourceJobsEnd(xide_source_jobs*);
int xideSourceJobLink(xide_source_jobs*,xide_source_job*);
/* Bind the actual borrowed Win32 worker HANDLE before ending admission.
 * Unlink before closing it, only after its worker has physically stopped. */
int xideSourceJobBind(xide_source_job*,void* thread);
void xideSourceJobUnlink(xide_source_job*);
int xideSourceJobStopped(const xide_source_job*);
int xideSourceJobsHeld(const xide_source_jobs*);
/* Passive/no-allocation request; never cancel/join/drain results implicitly.
 * quiet covers these registered jobs only, not all IDE source readers or
 * complete request/source/project/config/consent policy. */
uint64_t xideSourceJobsSuspend(xide_source_jobs*,xide_model_error*);
int xideSourceJobsState(const xide_source_jobs*,uint64_t,xide_source_jobs_status*);
/* Caller settles all writes/rollback/recovery before resuming admission. */
int xideSourceJobsResume(xide_source_jobs*,uint64_t,xide_model_error*);
#endif
