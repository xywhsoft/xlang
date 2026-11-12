#ifndef XLANG_IDE_FILE_SYNC_H
#define XLANG_IDE_FILE_SYNC_H
#include "app/xide_app.h"
#include "platform/file_watch.h"
typedef struct xide_file_sync xide_file_sync;
typedef struct xide_file_change {
    xide_watch_event snapshot;
    uint64_t documentId,lastAttempt;
    int acknowledged,roles;
    char reason[512];
} xide_file_change;
int xideFileSyncStart(xide_app* app,xide_model_error* error);
void xideFileSyncStop(xide_app* app);
void xideFileSyncPoll(xide_app* app);
int xideFileSyncSettled(xide_app* app);
/* Owner-side monitor participant in a source-write barrier. No nested holds.
 * Request returns without joining the worker or freeing queued bodies; wait by
 * polling Suspended with this exact token. Automatic coordination, reload/keep,
 * rebinding and before-save admission are disabled until successful Resume.
 * Resume requires acknowledgement and installs fresh complete subscriptions;
 * cached prior-epoch changes cannot be replayed. Failure retains the hold.
 * Does not suspend other readers, task workers, prepared opens or the LSP. */
uint64_t xideFileSyncSuspend(xide_app*,xide_model_error*);
int xideFileSyncSuspended(xide_app*,uint64_t token);
int xideFileSyncSourceLeaseCurrent(const xide_app*,uint64_t token);
int xideFileSyncResume(xide_app*,uint64_t token,xide_model_error*);
size_t xideFileSyncCount(xide_app* app);
const xide_file_change* xideFileSyncAt(xide_app* app,size_t index);
uint64_t xideFileSyncRevision(xide_app* app);
void xideFileSyncRefresh(xide_app* app);
/* Immediately publish new bindings/epoch without applying queued disk events. */
int xideFileSyncRebind(xide_app* app,xide_model_error* error);
int xideFileSyncKeep(xide_app* app,uint64_t token,uint64_t sequence);
/* The caller must obtain explicit consent before discard=1. Stale selections fail closed. */
int xideFileSyncReload(xide_app* app,uint64_t token,uint64_t sequence,uint32_t documentVersion,int discard,xide_model_error* error);
int xideFileSyncBeforeSave(xide_document* document,const char* target,xide_model_error* error,void* user);
void xideFileSyncAfterSave(xide_document* document,void* user);
#endif
