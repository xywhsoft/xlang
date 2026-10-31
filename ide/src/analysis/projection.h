#ifndef XLANG_ANALYSIS_PROJECTION_H
#define XLANG_ANALYSIS_PROJECTION_H
#include "analysis/check.h"
typedef struct xls_projection xls_projection;
typedef struct xls_projection_store xls_projection_store;
struct xls_worker;
/* Borrowed optional bank;the worker must outlive calls. Leases survive it. */
xls_projection_store* xlsWorkerProjectionStore(struct xls_worker* worker);
typedef struct xls_projection_stats {size_t entries,bytes,limit;uint64_t hits,misses,published,replaced,declined,retired;} xls_projection_stats;
#define XLS_PROJECTION_KEY_BYTES (1024u*1024u)
#define XLS_PROJECTION_VALUE_BYTES (64u*1024u*1024u)
#define XLS_PROJECTION_STORE_BYTES (66u*1024u*1024u)
/* One worker-owned,thread-safe resident derived view. Full caller key bytes
 * and the EXACT retained check owner define identity,never names/hashes/UIDs.
 * A hit is NOT a source/freshness/publication proof. The caller still validates
 * current inputs before using/publishing the projection. No I/O or callbacks
 * under the metadata lock. Extra owners,allocator metadata,transients and RSS
 * are not the charged resident value/key bytes. A resident pins one check. */
xls_projection_store* xlsProjectionStoreCreate(size_t bytes);
void xlsProjectionStoreDestroy(xls_projection_store* store);
xls_projection* xlsProjectionAcquire(xls_projection_store* store,xls_semantic_check* owner,const void* key,size_t size);
/* Payload ownership transfers ONLY on success. Payload must be immutable,
 * independently owned and must NOT retain the check/worker/store (no cycles).
 * knownBytes describes its owned data,not a guess at whole-model RSS. */
xls_projection* xlsProjectionCreate(xls_semantic_check* owner,const void* key,size_t size,
    void* value,size_t knownBytes,void (*destroy)(void*));
/* Consumes and returns one owned reference. Equal competing publication uses
 * the resident winner. Quota/disabled/known-invalid refusal returns the given
 * independently usable value without caching it. Never changes diagnostics. */
xls_projection* xlsProjectionPublish(xls_projection_store* store,xls_projection* value);
/* Forget a resident for a different exact check owner. Live leases survive.
 * Destruction runs outside the metadata lock. No publication/freshness proof;
 * caller still owns and validates the newly accepted check. */
uint64_t xlsProjectionRetireOther(xls_projection_store* store,const xls_semantic_check* owner);
/* The returned publication revision advances even without an old resident.
 * Delayed work may publish only at that exact revision. Zero is disabled;
 * refusal keeps the caller's independently usable lease,like Publish. */
xls_projection* xlsProjectionPublishAt(xls_projection_store* store,xls_projection* value,uint64_t revision);
xls_projection* xlsProjectionRetain(xls_projection* value);
void xlsProjectionRelease(xls_projection* value);
void* xlsProjectionValue(const xls_projection* value);
void xlsProjectionStoreStats(xls_projection_store* store,xls_projection_stats* stats);
#endif
