#ifndef XIDE_SOURCE_BARRIER_H
#define XIDE_SOURCE_BARRIER_H
#include "model/workspace.h"
#include "services/edit_request.h"
#include <stdint.h>
typedef struct xide_app xide_app;
enum xide_source_participant {
 XIDE_SOURCE_DOCUMENTS=1u,XIDE_SOURCE_JOBS=2u,XIDE_SOURCE_TASKS=4u,
 XIDE_SOURCE_TOOLCHAINS=8u,XIDE_SOURCE_WATCH=16u,XIDE_SOURCE_LANGUAGE=32u,
 XIDE_SOURCE_ALL=63u
};
/* Owner-only caller storage, zero-initialize once. No resource ownership or
 * allocation. Keep app, its context, participant graph and this storage alive
 * and unchanged until Release completes. Never copy an active barrier or share
 * its participant tokens with another caller. Not a global source-write lock. */
typedef struct xide_source_barrier {
 xide_app* app;
 xide_edit_request* request;
 void* identities[7];
 uint64_t serial,token,leases[6];
 unsigned held;
 int failed,invalidated,resuming;
 xide_model_error failure;
} xide_source_barrier;
typedef struct xide_source_barrier_status {
 unsigned held,pending,lost;
 int quiet,ready,failed,invalidated,resuming;
} xide_source_barrier_status;
/* A nonzero token owns an admitted operation, including a FAILED prefix which
 * MUST be explicitly resumed/released. Zero means no acquired holds. Partial
 * acquisition never resumes automatically. Begin/State do not wait or dispatch
 * UI events. State.ready proves these participants only; request/project/config,
 * full source/physical-alias policy, consent and recovery are still required. */
uint64_t xideSourceBarrierBegin(xide_source_barrier*,xide_app*,xide_model_error*);
/* Edit-specific admission first validates the exact captured request scope.
 * Owns a shared immutable request until Release; invalidation/End while held
 * permanently prevents ready, without changing the participant-release duties.
 * Full source/physical/consent/recovery policies are still mandatory. */
uint64_t xideSourceBarrierBeginEdit(xide_source_barrier*,xide_app*,xide_edit_request*,xide_model_error*);
int xideSourceBarrierState(xide_source_barrier*,uint64_t,xide_source_barrier_status*);
/* Call only after writes, rollback or explicit recovery are settled. Caller
 * invalidates affected task inputs first. May abandon a still-pending Begin.
 * This starts the asynchronous language resume but keeps other gates closed. */
int xideSourceBarrierResume(xide_source_barrier*,uint64_t,xide_model_error*);
/* Owner retry: 0 still pending, 1 all owned gates released, -1 error/retry.
 * Waits for watch acknowledgement and language resume without joining. Monitor
 * resubscription failure preserves every unopened gate; successful prefix steps
 * are retained for retry. No source writes or implicit cancellation/rollback.
 * Continue ordinary language Poll while waiting. Failed original admission does
 * not become valid when its gates finish reopening. A broken participant graph
 * or stolen token is an error, never authority to release a different hold. */
int xideSourceBarrierRelease(xide_source_barrier*,uint64_t,xide_model_error*);
#endif
