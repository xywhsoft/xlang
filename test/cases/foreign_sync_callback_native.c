#include "foreign_sync_callback.h"
#include "foreign_callback_span_native.c"
#include <assert.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
int sync_scalar(SyncScalar callback,void* user,int seed,int count)
{
    if(!callback){assert(!user);return 0;}
    int result=0;for(int i=0;i<count;++i)result+=callback(seed+i,user);return result;
}
int sync_pair(SyncScalar first,void* a,SyncFirst second,void* b)
{int secondResult=second?second(b,3):0;int firstResult=first?first(2,a):0;return firstResult+secondResult;}
void sync_action(SyncAction callback,void* user,int value)
{if(callback)callback(value,user);else assert(!user);}
int sync_root_apply(SpanRoot* root,SyncRootCallback callback,void* user)
{return callback?callback(root,user):0;}
typedef struct Attempt { SyncEventCallback callback;void* user;const SyncEvent* event;int result;
#if defined(_WIN32)
 void* parent;
#endif
} Attempt;
#if defined(_WIN32)
static DWORD WINAPI worker(void* p){Attempt* a=p;a->result=a->callback(a->event,a->user);return 0;}
static VOID WINAPI fiber(void* p){Attempt* a=p;a->result=a->callback(a->event,a->user);SwitchToFiber(a->parent);}
#else
static void* worker(void* p){Attempt* a=p;a->result=a->callback(a->event,a->user);return NULL;}
#endif
int sync_event(SyncEventCallback callback,void* user,int mode)
{
    if(!callback){assert(!user);return 0;}
    const char text[]={'a',0,'b'};SyncEvent child={12,text,sizeof(text),NULL},event={11,text,sizeof(text),&child};
    Attempt a={0};a.callback=callback;a.user=user;a.event=&event;
    if(mode==2){
#if defined(_WIN32)
        HANDLE thread=CreateThread(NULL,0,worker,&a,0,NULL);assert(thread);assert(WaitForSingleObject(thread,INFINITE)==WAIT_OBJECT_0);CloseHandle(thread);
#else
        pthread_t thread;assert(!pthread_create(&thread,NULL,worker,&a));assert(!pthread_join(thread,NULL));
#endif
    }else if(mode==3){
#if defined(_WIN32)
        bool converted=!IsThreadAFiber();a.parent=converted?ConvertThreadToFiber(NULL):GetCurrentFiber();assert(a.parent);
        void* other=CreateFiber(0,fiber,&a);assert(other);SwitchToFiber(other);DeleteFiber(other);if(converted)assert(ConvertFiberToThread());
#else
        pthread_t thread;assert(!pthread_create(&thread,NULL,worker,&a));assert(!pthread_join(thread,NULL));
#endif
    }else a.result=callback(mode==1?NULL:&event,user);
    memset(&event,0,sizeof(event));memset(&child,0,sizeof(child));return a.result;
}
int sync_root_event(SpanRoot* root,SyncEventCallback callback,void* user)
{assert(root);return sync_event(callback,user,0);}
int sync_event_apply(const SyncEvent* event,SyncEventCallback callback,void* user)
{return callback?callback(event,user):0;}
int sync_bytes(SyncBytes callback,void* user,int mode)
{static const unsigned char text[]={'a',0,'b'};return callback?callback(mode==1?NULL:text,mode==1?3:sizeof(text),user):0;}
static xmemdebugsnapshot syncBefore;
void sync_enable_debug(void){assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));}
void sync_counters_begin(void){assert(xrtMemDebugEnabled());xrtMemDebugSnapshot(&syncBefore);}
void sync_counters_end(void){xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(syncBefore.AllocCount==after.AllocCount && syncBefore.FreeCount==after.FreeCount && syncBefore.ReallocCount==after.ReallocCount);}
void sync_fail_after(int n){assert(xrtMemDebugFailAfter(n));}
bool sync_failure(void){return false;}
static int syncLive,syncRollbacks,syncDestroys;
typedef struct ActionAttempt {SyncAction callback;void* user;} ActionAttempt;
#if defined(_WIN32)
static DWORD WINAPI action_worker(void* raw){ActionAttempt* a=raw;a->callback(1,a->user);return 0;}
#else
static void* action_worker(void* raw){ActionAttempt* a=raw;a->callback(1,a->user);return NULL;}
#endif
static void clear_record(SyncRecord* record)
{if(record->payload){xrtFree(record->payload);--syncLive;}*record=(SyncRecord){0};}
void sync_rollback(SyncRecord* record){++syncRollbacks;clear_record(record);}
static bool destroy_record(void* raw){++syncDestroys;clear_record(raw);xrtFree(raw);return true;}
static bool destroy_unadopted_record(void* raw){sync_rollback(raw);xrtFree(raw);return true;}
const XL6_ForeignViewDescriptorV1 syncRecordPolicy={sizeof(syncRecordPolicy),XL6_FOREIGN_VIEW_V1_TAG,"sync.record",0,1,NULL,NULL,destroy_record,0,1,NULL,NULL,NULL,NULL,0,destroy_unadopted_record,NULL};
void sync_factory(SyncRecord* record,SyncAction callback,void* user,int mode)
{
    assert(record && !record->payload);
    if(callback && mode==2){
        ActionAttempt attempt={callback,user};
#if defined(_WIN32)
        HANDLE thread=CreateThread(NULL,0,action_worker,&attempt,0,NULL);assert(thread);assert(WaitForSingleObject(thread,INFINITE)==WAIT_OBJECT_0);CloseHandle(thread);
#else
        pthread_t thread;assert(!pthread_create(&thread,NULL,action_worker,&attempt));assert(!pthread_join(thread,NULL));
#endif
    }else if(callback)callback(1,user);
    record->payload=xrtMalloc(16);if(record->payload){++syncLive;record->value=42;}
    if(mode)xrtSetErrorKind(XERR_RANGE);
}
void sync_resource_expect(int rollbacks,int destroys)
{assert(!syncLive && syncRollbacks==rollbacks && syncDestroys==destroys);}
char* sync_text(SyncAction callback,void* user)
{if(callback)callback(1,user);char* text=xrtMalloc(3);if(text)memcpy(text,"ok",3);return text;}
int sync_output(SyncAction callback,void* user,int* result)
{*result=7;if(callback)callback(1,user);return 0;}
