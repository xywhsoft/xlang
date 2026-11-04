#ifndef FOREIGN_SYNC_CALLBACK_H
#define FOREIGN_SYNC_CALLBACK_H
#include <xrt_decl.h>
#include "test/cases/foreign_callback_span.h"
typedef struct SyncEvent { int code; const char* text; size_t length; const struct SyncEvent* child; } SyncEvent;
typedef int (*SyncScalar)(int,void*);
typedef int (*SyncFirst)(void*,int);
typedef int (*SyncEventCallback)(const SyncEvent*,void*);
typedef int (*SyncBytes)(const unsigned char*,size_t,void*);
typedef void (*SyncAction)(int,void*);
void sync_action(SyncAction,void*,int);
typedef int (*SyncRootCallback)(SpanRoot*,void*);
int sync_root_apply(SpanRoot*,SyncRootCallback,void*);
int sync_root_event(SpanRoot*,SyncEventCallback,void*);
int sync_scalar(SyncScalar,void*,int,int);
int sync_pair(SyncScalar,void*,SyncFirst,void*);
int sync_event(SyncEventCallback,void*,int);
int sync_event_apply(const SyncEvent*,SyncEventCallback,void*);
int sync_bytes(SyncBytes,void*,int);
void sync_counters_begin(void);
void sync_counters_end(void);
void sync_enable_debug(void);
void sync_fail_after(int);
bool sync_failure(void);
typedef struct SyncRecord { void* payload; int value; } SyncRecord;
extern const XL6_ForeignViewDescriptorV1 syncRecordPolicy;
void sync_factory(SyncRecord*,SyncAction,void*,int);
void sync_rollback(SyncRecord*);
void sync_resource_expect(int,int);
char* sync_text(SyncAction,void*);
int sync_output(SyncAction,void*,int*);
#endif
