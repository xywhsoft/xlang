#define _WIN32_WINNT 0x0601
#include "platform/file_watch.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define MAX_TARGETS 20000u
#define MAX_EVENTS 4096u
#define MAX_BYTES (128u*1024u*1024u)
#define BODY_LIMIT (16u*1024u*1024u)
#define CONTROL_KEY ((ULONG_PTR)1)
typedef struct target { xide_watch_target spec; char* path; xide_watch_event last; int known,directory; } target;
typedef struct batch { target* items; size_t count; uint64_t epoch; } batch;
typedef struct directory { char* path; HANDLE handle; OVERLAPPED operation; unsigned char buffer[32768]; int active; } directory;
struct xide_file_watch {
    CRITICAL_SECTION mutex; HANDLE thread,port; volatile LONG stop;
    batch pending; int pendingSet,rescan; uint64_t nextEpoch,sequence;
    xide_watch_event* events; size_t count,capacity,bytes; xide_watch_stats stats;
    void (*wake)(void*); void* user;
};
static int fail(xide_model_error* e,int code,const char* s) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static wchar_t* wide(const char* p) { int n=p ? MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,-1,NULL,0) : 0; wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL; if(w) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,-1,w,n); return w; }
void xideFileWatchEventClear(xide_watch_event* e) { if(!e) return; free(e->path); free(e->bytes); memset(e,0,sizeof(*e)); }
static void clearBatch(batch* b) { for(size_t i=0;i<b->count;++i) free(b->items[i].path); free(b->items); memset(b,0,sizeof(*b)); }
static void wake(xide_file_watch* w) { if(w->wake) w->wake(w->user); }
static uint64_t stamp(FILETIME t) { return ((uint64_t)t.dwHighDateTime<<32)|t.dwLowDateTime; }
static int current(xide_file_watch* w,uint64_t epoch)
{ EnterCriticalSection(&w->mutex); int okay=w->stats.epoch==epoch; LeaveCriticalSection(&w->mutex); return okay && !InterlockedCompareExchange(&w->stop,0,0); }
static void readTarget(xide_file_watch* w,const target* t,xide_watch_event* e)
{
    e->token=t->spec.token; wchar_t* name=wide(t->path);
    HANDLE h=name ? CreateFileW(name,t->spec.limit ? GENERIC_READ : FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL) : INVALID_HANDLE_VALUE;
    DWORD error=GetLastError(); free(name);
    if(h==INVALID_HANDLE_VALUE) { e->error=error; e->state=error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND ? XIDE_WATCH_MISSING : XIDE_WATCH_UNREADABLE; return; }
    BY_HANDLE_FILE_INFORMATION before={0},after={0};
    if(!GetFileInformationByHandle(h,&before)) { e->state=XIDE_WATCH_UNREADABLE; e->error=GetLastError(); goto done; }
    e->size=((uint64_t)before.nFileSizeHigh<<32)|before.nFileSizeLow; e->writeTime=stamp(before.ftLastWriteTime);
    e->fileId=((uint64_t)before.nFileIndexHigh<<32)|before.nFileIndexLow; e->volume=before.dwVolumeSerialNumber;
    if(before.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) { e->state=XIDE_WATCH_DIRECTORY; goto done; }
    if(!t->spec.limit) { e->state=XIDE_WATCH_READY; goto done; }
    if(e->size>t->spec.limit) { e->state=XIDE_WATCH_TOO_LARGE; goto done; }
    e->bytes=malloc((size_t)e->size+1); if(!e->bytes) { e->state=XIDE_WATCH_UNREADABLE; e->error=ERROR_NOT_ENOUGH_MEMORY; goto done; }
    e->hash=UINT64_C(14695981039346656037);
    while(e->length<(size_t)e->size) {
        DWORD n=0,amount=(DWORD)((size_t)e->size-e->length); if(amount>65536) amount=65536;
        if(!current(w,e->epoch) || !ReadFile(h,e->bytes+e->length,amount,&n,NULL) || !n) { e->state=XIDE_WATCH_UNREADABLE; e->error=GetLastError(); goto done; }
        for(DWORD i=0;i<n;++i) { e->hash^=e->bytes[e->length+i]; e->hash*=UINT64_C(1099511628211); } e->length+=n;
    }
    e->bytes[e->length]=0;
    if(!GetFileInformationByHandle(h,&after) || before.nFileSizeHigh!=after.nFileSizeHigh || before.nFileSizeLow!=after.nFileSizeLow || stamp(before.ftLastWriteTime)!=stamp(after.ftLastWriteTime)) e->state=XIDE_WATCH_UNSTABLE;
    else e->state=XIDE_WATCH_READY;
done:
    CloseHandle(h); if(e->state!=XIDE_WATCH_READY) { free(e->bytes); e->bytes=NULL; e->length=0; }
}
static int same(const xide_watch_event* a,const xide_watch_event* b)
{ return a->state==b->state && a->error==b->error && a->size==b->size && a->hash==b->hash && a->writeTime==b->writeTime && a->fileId==b->fileId && a->volume==b->volume; }
static int enqueue(xide_file_watch* w,xide_watch_event* e)
{
    int okay=0; EnterCriticalSection(&w->mutex);
    if(e->epoch!=w->stats.epoch) goto done;
    size_t i=0; for(;i<w->count;++i) if(w->events[i].epoch==e->epoch && w->events[i].token==e->token) break;
    size_t old=i<w->count ? w->events[i].length : 0;
    if(w->bytes-old+e->length>MAX_BYTES || (i==w->count && w->count==MAX_EVENTS)) goto dropped;
    if(i==w->capacity) { size_t cap=w->capacity ? w->capacity*2 : 64; xide_watch_event* p=realloc(w->events,cap*sizeof(*p)); if(!p) goto dropped; w->events=p; w->capacity=cap; }
    if(i<w->count) xideFileWatchEventClear(&w->events[i]); else ++w->count;
    e->sequence=++w->sequence; w->bytes=w->bytes-old+e->length; w->events[i]=*e; memset(e,0,sizeof(*e)); okay=1; goto done;
dropped:
    ++w->stats.dropped;
done:
    LeaveCriticalSection(&w->mutex); if(okay) wake(w); return okay;
}
static void scan(xide_file_watch* w,batch* b,int dir,int force)
{
    for(size_t i=0;i<b->count && current(w,b->epoch);++i) {
        target* t=&b->items[i]; if(dir>=0 && t->directory!=dir) continue;
        xide_watch_event e={.epoch=b->epoch}; readTarget(w,t,&e);
        if(force || !t->known || !same(&t->last,&e)) {
            e.path=copy(t->path); xide_watch_event last=e; last.path=NULL; last.bytes=NULL;
            if(e.path && enqueue(w,&e)) { t->last=last; t->known=1; }
        }
        xideFileWatchEventClear(&e);
    }
}
static int arm(directory* d)
{
    if(d->handle==INVALID_HANDLE_VALUE) return 0;
    memset(&d->operation,0,sizeof(d->operation)); DWORD unused=0;
    d->active=ReadDirectoryChangesW(d->handle,d->buffer,sizeof(d->buffer),FALSE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_CREATION|FILE_NOTIFY_CHANGE_ATTRIBUTES,&unused,&d->operation,NULL)!=0;
    return d->active;
}
static void clearDirectories(xide_file_watch* w,directory*** dirs,size_t* count)
{
    size_t remaining=0;
    for(size_t i=0;i<*count;++i) if((*dirs)[i]->active) { ++remaining; CancelIoEx((*dirs)[i]->handle,&(*dirs)[i]->operation); }
    /* Completion keys own directory storage until every pending operation drains. */
    while(remaining) {
        DWORD bytes=0; ULONG_PTR key=0; OVERLAPPED* operation=NULL;
        GetQueuedCompletionStatus(w->port,&bytes,&key,&operation,INFINITE);
        if(operation && key!=CONTROL_KEY) { directory* d=(directory*)key; if(d->active) { d->active=0; --remaining; } }
    }
    for(size_t i=0;i<*count;++i) { directory* d=(*dirs)[i]; if(d->handle!=INVALID_HANDLE_VALUE) CloseHandle(d->handle); free(d->path); free(d); }
    free(*dirs); *dirs=NULL; *count=0;
}
static char* parent(const char* path)
{
    char* p=xidePathDirectory(path);
    while(p) {
        wchar_t* name=wide(p); DWORD attributes=name ? GetFileAttributesW(name) : INVALID_FILE_ATTRIBUTES; free(name);
        if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_DIRECTORY)) return p;
        char* next=xidePathDirectory(p); if(!next || !_stricmp(next,p)) { free(next); break; } free(p); p=next;
    }
    free(p); return NULL;
}
static void buildDirectories(xide_file_watch* w,batch* b,directory*** dirs,size_t* count)
{
    for(size_t i=0;i<b->count && current(w,b->epoch);++i) {
        char* p=parent(b->items[i].path); b->items[i].directory=-1; if(!p) continue;
        size_t j=0; for(;j<*count;++j) if(!_stricmp((*dirs)[j]->path,p)) break;
        if(j==*count) {
            directory** grown=realloc(*dirs,(*count+1)*sizeof(*grown)); directory* d=calloc(1,sizeof(*d));
            if(grown) *dirs=grown;
            if(!grown || !d) { free(d); free(p); continue; }
            d->path=p; p=NULL; d->handle=INVALID_HANDLE_VALUE;
            wchar_t* name=wide(d->path); if(name) d->handle=CreateFileW(name,FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,NULL); free(name);
            if(d->handle!=INVALID_HANDLE_VALUE && !CreateIoCompletionPort(d->handle,w->port,(ULONG_PTR)d,0)) { CloseHandle(d->handle); d->handle=INVALID_HANDLE_VALUE; }
            (*dirs)[(*count)++]=d; arm(d);
        }
        b->items[i].directory=(int)j; free(p);
    }
    EnterCriticalSection(&w->mutex); w->stats.directories=*count; LeaveCriticalSection(&w->mutex);
}
static DWORD WINAPI worker(void* data)
{
    xide_file_watch* w=data; batch active={0}; directory** dirs=NULL; size_t count=0; uint64_t due=GetTickCount64()+2000;
    while(!InterlockedCompareExchange(&w->stop,0,0)) {
        batch next={0}; int update=0,force=0;
        EnterCriticalSection(&w->mutex); if(w->pendingSet) { next=w->pending; memset(&w->pending,0,sizeof(w->pending)); w->pendingSet=0; update=1; } force=w->rescan; w->rescan=0; LeaveCriticalSection(&w->mutex);
        if(update) {
            clearDirectories(w,&dirs,&count); clearBatch(&active); active=next; buildDirectories(w,&active,&dirs,&count); scan(w,&active,-1,1);
            EnterCriticalSection(&w->mutex); w->stats.readyEpoch=active.epoch; LeaveCriticalSection(&w->mutex); wake(w); due=GetTickCount64()+2000;
        } else if(force) scan(w,&active,-1,1);
        if(InterlockedCompareExchange(&w->stop,0,0)) break;
        DWORD bytes=0; ULONG_PTR key=0; OVERLAPPED* operation=NULL;
        BOOL completed=GetQueuedCompletionStatus(w->port,&bytes,&key,&operation,100);
        if(operation && key!=CONTROL_KEY) {
            directory* d=(directory*)key; d->active=0; int index=-1; for(size_t i=0;i<count;++i) if(dirs[i]==d) { index=(int)i; break; }
            EnterCriticalSection(&w->mutex); ++w->stats.notifications; LeaveCriticalSection(&w->mutex);
            arm(d); scan(w,&active,completed && bytes ? index : -1,0);
        }
        uint64_t now=GetTickCount64();
        if(now>=due) {
            clearDirectories(w,&dirs,&count); buildDirectories(w,&active,&dirs,&count); scan(w,&active,-1,0); due=now+2000;
            EnterCriticalSection(&w->mutex); ++w->stats.sweeps; LeaveCriticalSection(&w->mutex);
        }
    }
    clearDirectories(w,&dirs,&count); clearBatch(&active); return 0;
}
xide_file_watch* xideFileWatchCreate(void (*onWake)(void*),void* user,xide_model_error* e)
{
    xide_file_watch* w=calloc(1,sizeof(*w)); if(!w) { fail(e,XIDE_MODEL_MEMORY,"Cannot allocate file monitor."); return NULL; }
    InitializeCriticalSection(&w->mutex); w->wake=onWake; w->user=user; w->port=CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,1);
    if(w->port) w->thread=CreateThread(NULL,0,worker,w,0,NULL);
    if(!w->port || !w->thread) { fail(e,XIDE_MODEL_IO,"Cannot start native file monitor."); xideFileWatchDestroy(w); return NULL; } return w;
}
uint64_t xideFileWatchSetTargets(xide_file_watch* w,const xide_watch_target* specs,size_t count,xide_model_error* e)
{
    if(!w || count>MAX_TARGETS || (count && !specs)) { fail(e,XIDE_MODEL_INVALID,"Invalid file monitor targets."); return 0; }
    batch next={0}; next.items=count ? calloc(count,sizeof(*next.items)) : NULL; next.count=count;
    if(count && !next.items) { fail(e,XIDE_MODEL_MEMORY,"Cannot copy file monitor targets."); return 0; }
    for(size_t i=0;i<count;++i) {
        for(size_t j=0;j<i;++j) if(specs[i].token==specs[j].token) { clearBatch(&next); fail(e,XIDE_MODEL_INVALID,"Duplicate file monitor token."); return 0; }
        if(!specs[i].token || !specs[i].path || specs[i].limit>BODY_LIMIT || !(next.items[i].path=xidePathResolve(NULL,specs[i].path))) { clearBatch(&next); fail(e,XIDE_MODEL_INVALID,"Invalid file monitor token, path or read limit."); return 0; }
        next.items[i].spec=specs[i]; next.items[i].spec.path=next.items[i].path;
    }
    EnterCriticalSection(&w->mutex); clearBatch(&w->pending); next.epoch=++w->nextEpoch; w->pending=next; w->pendingSet=1; w->stats.epoch=next.epoch; w->stats.targets=count;
    for(size_t i=0;i<w->count;++i) xideFileWatchEventClear(&w->events[i]);
    w->count=w->bytes=0;
    LeaveCriticalSection(&w->mutex); CancelSynchronousIo(w->thread); PostQueuedCompletionStatus(w->port,0,CONTROL_KEY,NULL); return next.epoch;
}
void xideFileWatchRescan(xide_file_watch* w) { if(w) { EnterCriticalSection(&w->mutex); w->rescan=1; LeaveCriticalSection(&w->mutex); PostQueuedCompletionStatus(w->port,0,CONTROL_KEY,NULL); } }
int xideFileWatchTake(xide_file_watch* w,xide_watch_event* e)
{
    if(!w || !e) return 0;
    int okay=0; EnterCriticalSection(&w->mutex);
    if(w->count) { *e=w->events[0]; --w->count; w->bytes-=e->length; memmove(w->events,w->events+1,w->count*sizeof(*w->events)); okay=1; }
    LeaveCriticalSection(&w->mutex); return okay;
}
void xideFileWatchStats(xide_file_watch* w,xide_watch_stats* stats)
{ if(!stats) return; memset(stats,0,sizeof(*stats)); if(w) { EnterCriticalSection(&w->mutex); *stats=w->stats; stats->queued=w->count; stats->queuedBytes=w->bytes; LeaveCriticalSection(&w->mutex); } }
void xideFileWatchDestroy(xide_file_watch* w)
{
    if(!w) return;
    InterlockedExchange(&w->stop,1);
    if(w->thread) { PostQueuedCompletionStatus(w->port,0,CONTROL_KEY,NULL); CancelSynchronousIo(w->thread); WaitForSingleObject(w->thread,INFINITE); CloseHandle(w->thread); }
    clearBatch(&w->pending); for(size_t i=0;i<w->count;++i) xideFileWatchEventClear(&w->events[i]); free(w->events);
    if(w->port) CloseHandle(w->port);
    DeleteCriticalSection(&w->mutex); free(w);
}
