#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
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
#define SOURCE_NAMES_LIMIT (4u*1024u*1024u)
#define CONTROL_KEY ((ULONG_PTR)1)
typedef struct directory directory;
typedef struct target { xide_watch_target spec; char* path;wchar_t* key; xide_watch_event last; int known;directory* directory; } target;
typedef struct batch { target* items;target** ordered; size_t count; uint64_t epoch;int sources,sourceIndex; } batch;
struct directory { char* path; HANDLE handle,namesHandle; OVERLAPPED operation,namesOperation; unsigned char buffer[32768],namesBuffer[4096]; int active,namesActive,recursive,retained,retiring; };
_Static_assert(offsetof(directory,buffer)%4==0 && offsetof(directory,namesBuffer)%4==0,"Native notification buffers require DWORD alignment.");
struct xide_file_watch {
    CRITICAL_SECTION mutex; HANDLE thread,port; volatile LONG stop;
    batch pending,suspendRetired; int pendingSet,rescan,sourceGap; uint64_t nextEpoch,sequence,suspendToken;
    xide_watch_event* events; size_t count,capacity,bytes,sourceBytes; xide_watch_stats stats;
    void (*wake)(void*); void* user;
};
static int fail(xide_model_error* e,int code,const char* s) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static wchar_t* wide(const char* p) { int n=p ? MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,-1,NULL,0) : 0; wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL; if(w) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,-1,w,n); return w; }
void xideFileWatchEventClear(xide_watch_event* e) { if(!e) return; free(e->path); free(e->bytes); memset(e,0,sizeof(*e)); }
static void clearBatch(batch* b) { for(size_t i=0;i<b->count;++i) {free(b->items[i].path);free(b->items[i].key);} free(b->ordered);free(b->items); memset(b,0,sizeof(*b)); }
static void retireSuspendedQueue(xide_file_watch* w,uint64_t epoch)
{
    xide_watch_event* events=NULL;size_t count=0;
    EnterCriticalSection(&w->mutex);
    if(w->suspendToken==epoch){events=w->events;count=w->count;w->events=NULL;w->count=w->capacity=w->bytes=w->sourceBytes=0;}
    LeaveCriticalSection(&w->mutex);
    for(size_t i=0;i<count;++i)xideFileWatchEventClear(events+i);
    free(events);
}
static void wake(xide_file_watch* w) { if(w->wake) w->wake(w->user); }
static uint64_t stamp(FILETIME t) { return ((uint64_t)t.dwHighDateTime<<32)|t.dwLowDateTime; }
static int current(xide_file_watch* w,uint64_t epoch)
{ EnterCriticalSection(&w->mutex); int okay=w->stats.epoch==epoch; LeaveCriticalSection(&w->mutex); return okay && !InterlockedCompareExchange(&w->stop,0,0); }
static void readTarget(xide_file_watch* w,const target* t,xide_watch_event* e)
{
    e->observedAt=GetTickCount64();
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
static size_t sourceBytes(const xide_watch_event* e) {return e->state==XIDE_WATCH_SOURCE_CHANGE && e->path ? strlen(e->path)+1:0;}
static int sameEvent(const xide_watch_event* a,const xide_watch_event* b)
{
    if(a->token || b->token)return a->token==b->token;
    if(a->state!=b->state)return 0;
    return a->state==XIDE_WATCH_SOURCE_RESCAN || (a->path && b->path && !strcmp(a->path,b->path));
}
static int enqueue(xide_file_watch* w,xide_watch_event* e)
{
    int okay=0,dropped=0; EnterCriticalSection(&w->mutex);
    if(e->epoch!=w->stats.epoch) goto done;
    size_t i=0; for(;i<w->count;++i) if(w->events[i].epoch==e->epoch && sameEvent(w->events+i,e)) break;
    size_t old=i<w->count ? w->events[i].length : 0;
    size_t oldNames=i<w->count ? sourceBytes(w->events+i):0,newNames=sourceBytes(e);
    if(w->bytes-old+e->length>MAX_BYTES || newNames>SOURCE_NAMES_LIMIT-(w->sourceBytes-oldNames) || (i==w->count && w->count==MAX_EVENTS)) goto dropped;
    if(i==w->capacity) { size_t cap=w->capacity ? w->capacity*2 : 64; xide_watch_event* p=realloc(w->events,cap*sizeof(*p)); if(!p) goto dropped; w->events=p; w->capacity=cap; }
    if(i<w->count) {if(e->state==XIDE_WATCH_SOURCE_CHANGE && w->events[i].changeType==1 && e->changeType==2)e->changeType=1;xideFileWatchEventClear(&w->events[i]);} else ++w->count;
    e->sequence=++w->sequence; w->bytes=w->bytes-old+e->length;w->sourceBytes=w->sourceBytes-oldNames+newNames; w->events[i]=*e; memset(e,0,sizeof(*e)); okay=1; goto done;
dropped:
    ++w->stats.dropped;dropped=1;
done:
    LeaveCriticalSection(&w->mutex); if(okay || dropped) wake(w); return okay;
}
static void sourceRescan(xide_file_watch* w,const batch* b)
{xide_watch_event e={.epoch=b->epoch,.state=XIDE_WATCH_SOURCE_RESCAN,.observedAt=GetTickCount64()};if(!enqueue(w,&e)){EnterCriticalSection(&w->mutex);if(b->sources && w->stats.epoch!=b->epoch)w->sourceGap=1;LeaveCriticalSection(&w->mutex);}}
static int compareTarget(const void* left,const void* right)
{return xidePathNativeKeyCompare((*(target* const*)left)->key,(*(target* const*)right)->key);}
static void prepareSourceIndex(xide_file_watch* w,batch* b)
{
    if(!b->sources)return;
    b->ordered=b->count ? malloc(b->count*sizeof(*b->ordered)):NULL;
    int okay=!b->count || b->ordered!=NULL;
    for(size_t i=0;okay && i<b->count;++i){b->items[i].key=xidePathLexicalWide(b->items[i].path);b->ordered[i]=b->items+i;okay=b->items[i].key!=NULL;}
    if(okay){if(b->count>1)qsort(b->ordered,b->count,sizeof(*b->ordered),compareTarget);b->sourceIndex=1;}else sourceRescan(w,b);
}
static int sourceKnown(const batch* b,const char* path)
{
    if(!b->count)return 0;
    target probe={.key=xidePathLexicalWide(path)};target* pointer=&probe;
    if(!probe.key)return -1;
    int found=bsearch(&pointer,b->ordered,b->count,sizeof(*b->ordered),compareTarget)!=NULL;free(probe.key);return found;
}
static int relativeSourceName(const char* name)
{
    if(!*name || *name=='/' || *name=='\\' || strchr(name,':'))return 0;
    for(const char* p=name;*p;){const char* end=p;while(*end && *end!='/' && *end!='\\')++end;
        if(end==p || (end-p==2 && p[0]=='.' && p[1]=='.'))return 0;
        p=*end ? end+1:end;}
    return 1;
}
/* The IOCP buffer is copied before rearming. Parse only the completed byte
 * count;names are length-delimited UTF16,not guaranteed NUL terminated. */
static void sourceNotifications(xide_file_watch* w,const batch* b,const directory* d,const unsigned char* data,size_t bytes)
{
    if(!b->sourceIndex){sourceRescan(w,b);return;}
    const size_t header=offsetof(FILE_NOTIFY_INFORMATION,FileName);size_t at=0;
    while(at<bytes && current(w,b->epoch)) {
        if(bytes-at<header){sourceRescan(w,b);return;}
        DWORD next=0,action=0,length=0;memcpy(&next,data+at,sizeof(next));memcpy(&action,data+at+4,sizeof(action));memcpy(&length,data+at+8,sizeof(length));
        if(!length || (length%sizeof(wchar_t)) || length>bytes-at-header || (next && (next<header+length || next%4 || next>=bytes-at))){sourceRescan(w,b);return;}
        int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,(const wchar_t*)(data+at+header),(int)(length/sizeof(wchar_t)),NULL,0,NULL,NULL);
        char* relative=size>0 ? malloc((size_t)size+1):NULL;
        if(!relative || !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,(const wchar_t*)(data+at+header),(int)(length/sizeof(wchar_t)),relative,size,NULL,NULL)){free(relative);sourceRescan(w,b);return;}
        relative[size]=0;int invalid=memchr(relative,0,(size_t)size)!=NULL;
        if(invalid || !relativeSourceName(relative)){free(relative);sourceRescan(w,b);return;}
        if(xidePathHasExtension(relative,".xl")) {
            /* Win32 may report an 8.3 spelling,including a deleted name that
               cannot be expanded anymore. Revalidate roots rather than treating
               that spelling as a precise dependency key. */
            if(strchr(relative,'~')){free(relative);sourceRescan(w,b);return;}
            char* path=xidePathResolveLexical(d->path,relative);int known=path?sourceKnown(b,path):-1;
            int type=action==FILE_ACTION_ADDED || action==FILE_ACTION_RENAMED_NEW_NAME ? 1 : action==FILE_ACTION_REMOVED || action==FILE_ACTION_RENAMED_OLD_NAME ? 3 : action==FILE_ACTION_MODIFIED ? 2:0;
            if(known<0 || !type){free(path);free(relative);sourceRescan(w,b);return;}
            if(!known){xide_watch_event e={.epoch=b->epoch,.state=XIDE_WATCH_SOURCE_CHANGE,.changeType=type,.path=path,.observedAt=GetTickCount64()};enqueue(w,&e);xideFileWatchEventClear(&e);}else free(path);
        }
        free(relative);if(!next){if(bytes-at-header-length>3)sourceRescan(w,b);break;}at+=next;
    }
    if(!current(w,b->epoch)){EnterCriticalSection(&w->mutex);w->sourceGap=1;LeaveCriticalSection(&w->mutex);}
}
static void scan(xide_file_watch* w,batch* b,const directory* dir,int force)
{
    unsigned retryBudgetMs=35; /* Shared by the scan, not paid per watched file. */
    for(size_t i=0;i<b->count && current(w,b->epoch);++i) {
        target* t=&b->items[i]; if(dir && t->directory!=dir) continue;
        xide_watch_event e={.epoch=b->epoch}; readTarget(w,t,&e);
        /* Atomic replacement can briefly deny opens or hide the old name.
           Retry on the worker; never sleep or reread file bodies on the UI. */
        for(unsigned retry=0;retry<3 && current(w,b->epoch);++retry) {
            int transient=(e.state==XIDE_WATCH_MISSING && t->known && t->last.state!=XIDE_WATCH_MISSING) || e.state==XIDE_WATCH_UNSTABLE ||
                (e.state==XIDE_WATCH_UNREADABLE && (e.error==ERROR_SHARING_VIOLATION || e.error==ERROR_LOCK_VIOLATION || e.error==ERROR_DELETE_PENDING));
            unsigned delay=5u<<retry;if(!transient || delay>retryBudgetMs) break;
            xideFileWatchEventClear(&e);Sleep(delay);retryBudgetMs-=delay;e.epoch=b->epoch;readTarget(w,t,&e);
        }
        if(force || !t->known || !same(&t->last,&e)) {
            e.path=copy(t->path); xide_watch_event last=e; last.path=NULL; last.bytes=NULL;
            if(e.path && enqueue(w,&e)) { t->last=last; t->known=1; }
        }
        xideFileWatchEventClear(&e);
    }
}
static int arm(directory* d,int names)
{
    HANDLE h=names ? d->namesHandle:d->handle;if(h==INVALID_HANDLE_VALUE)return 0;
    OVERLAPPED* operation=names?&d->namesOperation:&d->operation;memset(operation,0,sizeof(*operation));DWORD unused=0;
    DWORD filter=names ? FILE_NOTIFY_CHANGE_DIR_NAME:FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_CREATION|FILE_NOTIFY_CHANGE_ATTRIBUTES|(d->recursive?0:FILE_NOTIFY_CHANGE_DIR_NAME);
    int active=ReadDirectoryChangesW(h,names?d->namesBuffer:d->buffer,names?sizeof(d->namesBuffer):sizeof(d->buffer),d->recursive,filter,&unused,operation,NULL)!=0;
    if(names)d->namesActive=active;else d->active=active;return active;
}
static void freeDirectory(directory* d)
{if(d->handle!=INVALID_HANDLE_VALUE)CloseHandle(d->handle);if(d->namesHandle!=INVALID_HANDLE_VALUE)CloseHandle(d->namesHandle);free(d->path);free(d);}
static void retireDirectory(directory* d)
{if(d->retiring)return;d->retiring=1;if(d->active)CancelIoEx(d->handle,&d->operation);if(d->namesActive)CancelIoEx(d->namesHandle,&d->namesOperation);}
static void pruneDirectories(xide_file_watch* w,directory** dirs,size_t* count)
{
    size_t live=0;for(size_t i=0;i<*count;){directory* d=dirs[i];if(d->retiring && !d->active && !d->namesActive){freeDirectory(d);dirs[i]=dirs[--*count];}else{if(!d->retiring)++live;++i;}}
    EnterCriticalSection(&w->mutex);w->stats.directories=live;LeaveCriticalSection(&w->mutex);
}
static void clearDirectories(xide_file_watch* w,directory*** dirs,size_t* count)
{
    size_t remaining=0;
    for(size_t i=0;i<*count;++i){directory* d=(*dirs)[i];if(d->active){++remaining;CancelIoEx(d->handle,&d->operation);}if(d->namesActive){++remaining;CancelIoEx(d->namesHandle,&d->namesOperation);}}
    /* Completion keys own directory storage until every pending operation drains. */
    while(remaining) {
        DWORD bytes=0; ULONG_PTR key=0; OVERLAPPED* operation=NULL;
        GetQueuedCompletionStatus(w->port,&bytes,&key,&operation,INFINITE);
        if(operation && key!=CONTROL_KEY) { directory* d=(directory*)key; if(operation==&d->operation && d->active){d->active=0;--remaining;}else if(operation==&d->namesOperation && d->namesActive){d->namesActive=0;--remaining;} }
    }
    for(size_t i=0;i<*count;++i)freeDirectory((*dirs)[i]);
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
        char* p=parent(b->items[i].path); b->items[i].directory=NULL; if(!p) {if(b->sources)sourceRescan(w,b);continue;}
        size_t j=0; for(;j<*count;++j) if(!(*dirs)[j]->retiring && !_stricmp((*dirs)[j]->path,p)) break;
        if(j==*count) {
            if(*count>=2*MAX_TARGETS){free(p);if(b->sources)sourceRescan(w,b);continue;}
            directory** grown=realloc(*dirs,(*count+1)*sizeof(*grown)); directory* d=calloc(1,sizeof(*d));
            if(grown) *dirs=grown;
            if(!grown || !d) { free(d); free(p);if(b->sources)sourceRescan(w,b); continue; }
            d->path=p; p=NULL; d->handle=d->namesHandle=INVALID_HANDLE_VALUE;d->recursive=b->sources;
            wchar_t* name=wide(d->path); if(name) d->handle=CreateFileW(name,FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,NULL); free(name);
            if(d->handle!=INVALID_HANDLE_VALUE && !CreateIoCompletionPort(d->handle,w->port,(ULONG_PTR)d,0)) { CloseHandle(d->handle); d->handle=INVALID_HANDLE_VALUE; }
            if(b->sources){name=wide(d->path);if(name)d->namesHandle=CreateFileW(name,FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,NULL);free(name);if(d->namesHandle!=INVALID_HANDLE_VALUE && !CreateIoCompletionPort(d->namesHandle,w->port,(ULONG_PTR)d,0)){CloseHandle(d->namesHandle);d->namesHandle=INVALID_HANDLE_VALUE;}}
            (*dirs)[(*count)++]=d;int armed=arm(d,0);if(b->sources && (!arm(d,1) || !armed))sourceRescan(w,b);
        }
        b->items[i].directory=(*dirs)[j];(*dirs)[j]->retained=1; free(p);
    }
    EnterCriticalSection(&w->mutex); w->stats.directories=*count; LeaveCriticalSection(&w->mutex);
}
static void reconcileSourceDirectories(xide_file_watch* w,batch* b,directory*** dirs,size_t* count)
{
    for(size_t i=0;i<*count;++i)(*dirs)[i]->retained=0;
    buildDirectories(w,b,dirs,count);
    for(size_t i=0;i<*count;++i)if(!(*dirs)[i]->retained)retireDirectory((*dirs)[i]);
    pruneDirectories(w,*dirs,count);
}
static DWORD WINAPI worker(void* data)
{
    xide_file_watch* w=data; batch active={0}; directory** dirs=NULL; size_t count=0; uint64_t due=GetTickCount64()+2000;
    while(!InterlockedCompareExchange(&w->stop,0,0)) {
        batch next={0},retired={0}; int update=0,force=0;
        EnterCriticalSection(&w->mutex); if(w->pendingSet) { next=w->pending; memset(&w->pending,0,sizeof(w->pending)); retired=w->suspendRetired;memset(&w->suspendRetired,0,sizeof(w->suspendRetired));w->pendingSet=0; update=1; } force=w->rescan; w->rescan=0; LeaveCriticalSection(&w->mutex);
        clearBatch(&retired);
        if(update) {
            int keep=active.sources && next.sources;
            if(!keep)clearDirectories(w,&dirs,&count);
            clearBatch(&active); active=next;prepareSourceIndex(w,&active);
            if(keep)reconcileSourceDirectories(w,&active,&dirs,&count);else buildDirectories(w,&active,&dirs,&count);
            scan(w,&active,NULL,1);
            retireSuspendedQueue(w,active.epoch);
            EnterCriticalSection(&w->mutex); w->stats.readyEpoch=active.epoch; LeaveCriticalSection(&w->mutex); wake(w); due=GetTickCount64()+2000;
        } else if(force) scan(w,&active,NULL,1);
        EnterCriticalSection(&w->mutex);int gap=w->sourceGap && active.sources && w->stats.epoch==active.epoch;if(gap || !active.sources)w->sourceGap=0;LeaveCriticalSection(&w->mutex);
        if(gap)sourceRescan(w,&active);
        if(InterlockedCompareExchange(&w->stop,0,0)) break;
        DWORD bytes=0; ULONG_PTR key=0; OVERLAPPED* operation=NULL;
        BOOL completed=GetQueuedCompletionStatus(w->port,&bytes,&key,&operation,100);
        if(operation && key!=CONTROL_KEY) {
            directory* d=(directory*)key;int names=operation==&d->namesOperation;if(names)d->namesActive=0;else d->active=0;
            EnterCriticalSection(&w->mutex); ++w->stats.notifications; LeaveCriticalSection(&w->mutex);
            if(!d->retiring){
                unsigned char saved[32768];size_t savedBytes=bytes<=sizeof(saved)?bytes:0;if(active.sources && !names && completed && savedBytes)memcpy(saved,d->buffer,savedBytes);
                arm(d,names);
                if(active.sources){if(!current(w,active.epoch)){EnterCriticalSection(&w->mutex);w->sourceGap=1;LeaveCriticalSection(&w->mutex);}else if(names || !completed || !savedBytes)sourceRescan(w,&active);else sourceNotifications(w,&active,d,saved,savedBytes);}
                scan(w,&active,completed && bytes && !names ? d:NULL,0);
            }
            pruneDirectories(w,dirs,&count);
        }
        uint64_t now=GetTickCount64();
        if(now>=due) {
            if(!active.sources){clearDirectories(w,&dirs,&count);buildDirectories(w,&active,&dirs,&count);}
            else {int broken=0;for(size_t i=0;i<count;++i){directory* d=dirs[i];if(!d->retiring && ((!d->active && !arm(d,0)) || (!d->namesActive && !arm(d,1)))){retireDirectory(d);broken=1;}}
                for(size_t i=0;i<active.count;++i)if(!active.items[i].directory)broken=1;
                if(broken){reconcileSourceDirectories(w,&active,&dirs,&count);sourceRescan(w,&active);}}
            scan(w,&active,NULL,0); due=now+2000;
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
{return xideFileWatchSetTargetsWithSources(w,specs,count,0,e);}
static uint64_t setTargets(xide_file_watch* w,const xide_watch_target* specs,size_t count,int enabled,uint64_t resume,xide_model_error* e)
{
    if(!w || count>MAX_TARGETS || (count && !specs)) { fail(e,XIDE_MODEL_INVALID,"Invalid file monitor targets."); return 0; }
    EnterCriticalSection(&w->mutex);
    int allowed=w->nextEpoch!=UINT64_MAX && (w->suspendToken ? resume==w->suspendToken && w->stats.readyEpoch==resume : resume==0);
    LeaveCriticalSection(&w->mutex);
    if(!allowed){fail(e,XIDE_MODEL_CONFLICT,"File monitor lease is held, stale, unacknowledged or exhausted.");return 0;}
    batch next={.sources=!!enabled}; next.items=count ? calloc(count,sizeof(*next.items)) : NULL; next.count=count;
    if(count && !next.items) { fail(e,XIDE_MODEL_MEMORY,"Cannot copy file monitor targets."); return 0; }
    for(size_t i=0;i<count;++i) {
        for(size_t j=0;j<i;++j) if(specs[i].token==specs[j].token) { clearBatch(&next); fail(e,XIDE_MODEL_INVALID,"Duplicate file monitor token."); return 0; }
        if(!specs[i].token || !specs[i].path || specs[i].limit>BODY_LIMIT || !(next.items[i].path=xidePathResolve(NULL,specs[i].path))) { clearBatch(&next); fail(e,XIDE_MODEL_INVALID,"Invalid file monitor token, path or read limit."); return 0; }
        next.items[i].spec=specs[i]; next.items[i].spec.path=next.items[i].path;
    }
    EnterCriticalSection(&w->mutex);
    if(w->nextEpoch==UINT64_MAX || (w->suspendToken ? resume!=w->suspendToken || w->stats.readyEpoch!=resume : resume!=0)){
        LeaveCriticalSection(&w->mutex);clearBatch(&next);fail(e,XIDE_MODEL_CONFLICT,"File monitor lease is held, stale, unacknowledged or exhausted.");return 0;
    }
    clearBatch(&w->pending); next.epoch=++w->nextEpoch; w->pending=next; w->pendingSet=1; w->stats.epoch=next.epoch; w->stats.targets=count;
    if(resume){w->suspendToken=0;if(enabled)w->sourceGap=1;}
    size_t kept=0;w->bytes=w->sourceBytes=0;
    for(size_t i=0;i<w->count;++i){xide_watch_event* event=w->events+i;
        if(enabled && !event->token && (event->state==XIDE_WATCH_SOURCE_CHANGE || event->state==XIDE_WATCH_SOURCE_RESCAN)){event->epoch=next.epoch;w->sourceBytes+=sourceBytes(event);if(kept!=i){w->events[kept]=*event;memset(event,0,sizeof(*event));}++kept;}
        else xideFileWatchEventClear(event);
    }
    w->count=kept;
    LeaveCriticalSection(&w->mutex); CancelSynchronousIo(w->thread); PostQueuedCompletionStatus(w->port,0,CONTROL_KEY,NULL); return next.epoch;
}
uint64_t xideFileWatchSetTargetsWithSources(xide_file_watch* w,const xide_watch_target* specs,size_t count,int enabled,xide_model_error* e)
{return setTargets(w,specs,count,enabled,0,e);}
uint64_t xideFileWatchResume(xide_file_watch* w,uint64_t token,const xide_watch_target* specs,size_t count,int enabled,xide_model_error* e)
{if(!token){fail(e,XIDE_MODEL_INVALID,"Missing file monitor lease.");return 0;}return setTargets(w,specs,count,enabled,token,e);}
uint64_t xideFileWatchSuspend(xide_file_watch* w,xide_model_error* e)
{
    if(!w){fail(e,XIDE_MODEL_INVALID,"Missing file monitor.");return 0;}
    EnterCriticalSection(&w->mutex);
    if(w->suspendToken || w->nextEpoch==UINT64_MAX){LeaveCriticalSection(&w->mutex);fail(e,XIDE_MODEL_CONFLICT,"File monitor lease is held or exhausted.");return 0;}
    uint64_t token=++w->nextEpoch;w->suspendRetired=w->pending;w->pending=(batch){.epoch=token};w->pendingSet=1;w->suspendToken=token;w->stats.epoch=token;w->stats.targets=0;
    LeaveCriticalSection(&w->mutex);CancelSynchronousIo(w->thread);PostQueuedCompletionStatus(w->port,0,CONTROL_KEY,NULL);return token;
}
int xideFileWatchSuspended(xide_file_watch* w,uint64_t token)
{if(!w||!token)return 0;EnterCriticalSection(&w->mutex);int okay=w->suspendToken==token&&w->stats.readyEpoch==token;LeaveCriticalSection(&w->mutex);return okay;}
void xideFileWatchRescan(xide_file_watch* w) { if(w) { EnterCriticalSection(&w->mutex); w->rescan=1; LeaveCriticalSection(&w->mutex); PostQueuedCompletionStatus(w->port,0,CONTROL_KEY,NULL); } }
int xideFileWatchTake(xide_file_watch* w,xide_watch_event* e)
{
    if(!w || !e) return 0;
    int okay=0; EnterCriticalSection(&w->mutex);
    if(!w->suspendToken && w->count) { *e=w->events[0]; --w->count; w->bytes-=e->length;w->sourceBytes-=sourceBytes(e); memmove(w->events,w->events+1,w->count*sizeof(*w->events)); okay=1; }
    LeaveCriticalSection(&w->mutex); return okay;
}
void xideFileWatchStats(xide_file_watch* w,xide_watch_stats* stats)
{ if(!stats) return; memset(stats,0,sizeof(*stats)); if(w) { EnterCriticalSection(&w->mutex); *stats=w->stats; stats->queued=w->count; stats->queuedBytes=w->bytes; LeaveCriticalSection(&w->mutex); } }
void xideFileWatchDestroy(xide_file_watch* w)
{
    if(!w) return;
    InterlockedExchange(&w->stop,1);
    if(w->thread) { PostQueuedCompletionStatus(w->port,0,CONTROL_KEY,NULL); CancelSynchronousIo(w->thread); WaitForSingleObject(w->thread,INFINITE); CloseHandle(w->thread); }
    clearBatch(&w->pending);clearBatch(&w->suspendRetired);for(size_t i=0;i<w->count;++i) xideFileWatchEventClear(&w->events[i]); free(w->events);
    if(w->port) CloseHandle(w->port);
    DeleteCriticalSection(&w->mutex); free(w);
}
