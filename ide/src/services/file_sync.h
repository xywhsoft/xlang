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
