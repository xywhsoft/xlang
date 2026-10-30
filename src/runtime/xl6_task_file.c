#define XRT_MODULE_FILE_ASYNC
#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#include "src/common/xl6_xrt.h"
#include "xl6_task.h"
#include <stdlib.h>
/* Resident driver selected only for stateful asynchronous file owners. */
struct XL6_AsyncFile {
    XL6_TaskResourceV1 resource;
    XL6_TaskPool* pool; /* borrowed alias; resource.pool owns the shell reference */
    xmutex* gate;
    xatomic32 references; /* caller plus registry, independently owned */
    xasyncfile* native;
    xfuture* closed;
    uint32 flags;
    const void* claim;
    bool cleared;
};
static void async_file_lock(const XL6_AsyncFile* file) { if(!xrtMutexLock(file->gate))abort(); }
static void async_file_unlock(const XL6_AsyncFile* file) { if(!xrtMutexUnlock(file->gate))abort(); }
static void async_file_drop(XL6_AsyncFile* file)
{
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    bool last=xrtAtomic32FetchSub(&file->references,1,XMEMORY_ACQ_REL)==1;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    if(!last)return;
    if(file->native || file->resource.registered)abort();
    xrtFutureDestroy(file->closed);if(!xrtMutexDestroy(file->gate))abort();
    if(file->resource.pool)xl6TaskResourceReleaseV1(&file->resource);
    xrtFree(file);
}
/* Called with the control gate, but never a native-pool join or callback wait. */
static void async_file_close_locked(XL6_AsyncFile* file)
{
    if(file->native){
        xasyncfile* native=file->native;file->native=NULL;
        file->closed=xrtAsyncFileClose(native);
        /* XRT reserves the close promise during adoption: Close cannot allocate
         * or fail admission. A violated invariant is not a recoverable leak. */
        if(!file->closed)abort();
    }
}
static void async_file_registry_close(void* data)
{ XL6_AsyncFile* file=data;async_file_lock(file);async_file_close_locked(file);async_file_unlock(file); }
static void async_file_registry_drop(void* data){async_file_drop(data);}
static XL6_AsyncFile* async_file_create(XL6_TaskPool* pool,const char* path,const xfileoptions* options,xfile adopted)
{
    XL6_TaskNativeScopeV1 scope={0};XL6_AsyncFile* file=xrtCalloc(1,sizeof(*file));
    if(!file)return NULL;
    file->gate=xrtMutexCreate();if(!file->gate){xrtFree(file);return NULL;}
    if(!xl6TaskNativeBeginV1(pool,&scope)){xrtMutexDestroy(file->gate);xrtFree(file);return NULL;}
    file->native=adopted?xrtAsyncFileAdopt(scope.native,adopted):xrtAsyncFileOpen(scope.native,path,options);
    if(!file->native){xl6TaskNativeEndV1(&scope);xrtMutexDestroy(file->gate);xrtFree(file);return NULL;}
    /* Every fallible allocation precedes adoption. Register while the native
     * admission credit still prevents quiescence from observing a missing file. */
    file->pool=pool;file->flags=xrtAsyncFileFlags(file->native);xrtAtomic32Init(&file->references,2);
    xl6TaskResourceInitV1(&file->resource,&scope,file,async_file_registry_close,async_file_registry_drop);
    xl6TaskNativeEndV1(&scope);return file;
}
XL6_AsyncFile* xl6AsyncFileOpenV1(XL6_TaskPool* pool,const char* path,const xfileoptions* options)
{ if(!path){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}return async_file_create(pool,path,options,NULL); }
XL6_AsyncFile* xl6AsyncFileAdoptV1(XL6_TaskPool* pool,xfile adopted)
{ if(!adopted){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}return async_file_create(pool,NULL,NULL,adopted); }
xfuture* xl6AsyncFileCloseV1(XL6_AsyncFile* file)
{
    if(!file){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    async_file_lock(file);async_file_close_locked(file);xfuture* result=xrtFutureRef(file->closed);
    bool removed=xl6TaskResourceRemoveV1(&file->resource);async_file_unlock(file);if(removed)async_file_drop(file);return result;
}
void xl6AsyncFileDestroyV1(XL6_AsyncFile* file)
{
    if(!file)return;
    if(file->cleared){async_file_drop(file);return;}
    /* Preserve the active diagnostic across allocation-free release. */
    xerror* error=xrtTakeError();xfuture* closed=xl6AsyncFileCloseV1(file);xrtFutureDestroy(closed);
    async_file_drop(file);xrtClearError();xrtSetErrorTake(error);
}
static xfuture* async_file_submit(XL6_AsyncFile* file,uint32 operation,uint64 offset,const uint8* data,size_t size)
{
    if(!file){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_TaskNativeScopeV1 scope={0};xfuture* result=NULL;async_file_lock(file);
    if(!file->native){xrtSetErrorKind(XERR_CLOSED);goto done;}
    if(!xl6TaskNativeBeginV1(file->pool,&scope))goto done;
    switch(operation){
    case 0:result=xrtAsyncFileReadAt(file->native,offset,size);break;
    case 1:result=xrtAsyncFileWriteAt(file->native,offset,(xbytesview){data,size});break;
    case 2:result=xrtAsyncFileFlush(file->native);break;
    case 3:result=xrtAsyncFileSize(file->native);break;
    case 4:result=xrtAsyncFileResize(file->native,offset);break;
    default:abort();
    }
    xl6TaskNativeEndV1(&scope);
done:async_file_unlock(file);return result;
}
xfuture* xl6AsyncFileReadV1(XL6_AsyncFile* file,uint64 offset,size_t size)
{return async_file_submit(file,0,offset,NULL,size);}
xfuture* xl6AsyncFileWriteV1(XL6_AsyncFile* file,uint64 offset,const unsigned char* data,size_t size)
{return async_file_submit(file,1,offset,data,size);}
xfuture* xl6AsyncFileFlushV1(XL6_AsyncFile* file){return async_file_submit(file,2,0,NULL,0);}
xfuture* xl6AsyncFileSizeV1(XL6_AsyncFile* file){return async_file_submit(file,3,0,NULL,0);}
xfuture* xl6AsyncFileResizeV1(XL6_AsyncFile* file,uint64 size){return async_file_submit(file,4,size,NULL,0);}
uint32 xl6AsyncFileFlagsV1(const XL6_AsyncFile* file)
{if(!file){xrtSetErrorKind(XERR_ARGUMENT);return 0;}return file->flags;}
bool xl6AsyncFileClosedV1(const XL6_AsyncFile* file)
{if(!file)return true;async_file_lock(file);bool closed=!file->native;async_file_unlock(file);return closed;}

#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
/* The language handle and the independently retained registry payload are
 * distinct physical nodes. Live native resources report BUSY before Trace;
 * only quiescent payloads expose their real pool and terminal Future owners. */
void xl6_xrt_fs_async_destroy(void* raw){xl6AsyncFileDestroyV1(raw);}
static bool async_file_count(const void* raw,size_t* count)
{
    const XL6_AsyncFile* file=raw;
    if(!file||!count||file->cleared)return false;
    int32_t references=xrtAtomic32Load(&file->references,XMEMORY_ACQUIRE);
    if(references<=0)return false;
    *count=(size_t)references;return true;
}
static bool async_file_busy(const void* raw)
{
    const XL6_AsyncFile* file=raw;size_t count;
    if(!file||file->cleared)return false;
    xrtownershipref pool=xl6TaskPoolOwnership(file->resource.pool);
    /* Pool Count refuses any close/registry publication in progress. Freeze
     * prevents a new mutation after that stable observation; no mutex is taken. */
    return !pool.Data||!pool.Ops->Count(pool.Data,&count)||
        file->resource.registered||file->native;
}
static bool async_file_edges(const void* raw,xrtownershipvisitor visit,void* context)
{
    const XL6_AsyncFile* file=raw;size_t count;
    if(!visit||!async_file_count(raw,&count)||async_file_busy(raw))return false;
    return visit(xl6TaskPoolOwnership(file->resource.pool),context)&&
        (!file->closed||visit(xrtFutureOwnership(file->closed),context));
}
static const xrtownershipops async_file_ops={async_file_count,async_file_edges};
const xrtownershipquiescencev1* xl6AsyncFileQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy={sizeof(policy),&async_file_ops,async_file_busy};return &policy;}
static bool async_file_hold(const void* raw)
{
    XL6_AsyncFile* file=(XL6_AsyncFile*)raw;
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    int32_t count=xrtAtomic32Load(&file->references,XMEMORY_ACQUIRE);
    bool held=count>0&&count<INT32_MAX;
    if(held)xrtAtomic32FetchAdd(&file->references,1,XMEMORY_ACQ_REL);
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    return held;
}
static void async_file_release(const void* raw){async_file_drop((XL6_AsyncFile*)raw);}
static bool async_file_claim(const void* raw,const void* token)
{
    XL6_AsyncFile* file=(XL6_AsyncFile*)raw;
    if(!token||file->cleared||(file->claim&&file->claim!=token))return false;
    file->claim=token;return true;
}
static void async_file_restore(const void* raw,const void* token)
{XL6_AsyncFile* file=(XL6_AsyncFile*)raw;if(!token||file->claim!=token||file->cleared)abort();file->claim=NULL;}
static void async_file_clear(const void* raw,const void* token)
{XL6_AsyncFile* file=(XL6_AsyncFile*)raw;if(!token||file->claim!=token||file->cleared)abort();file->cleared=true;}
static bool async_file_finish(const void* raw,const void* token)
{
    XL6_AsyncFile* file=(XL6_AsyncFile*)raw;xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    if(!token||file->claim!=token||!file->cleared||file->native||file->resource.registered)abort();
    xfuture* closed=file->closed;file->closed=NULL;
    XL6_TaskResourceV1 resource=file->resource;file->resource=(XL6_TaskResourceV1){0};file->pool=NULL;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xrtFutureDestroy(closed);if(resource.pool)xl6TaskResourceReleaseV1(&resource);return true;
}
const xrtownershipadapterv1* xl6AsyncFileOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),async_file_hold,async_file_release,
        async_file_claim,async_file_restore,NULL,async_file_clear,async_file_finish};
    size_t count;return ref.Ops==&async_file_ops&&async_file_count(ref.Data,&count)?&adapter:NULL;
}
static bool async_file_trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!visit||!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||
        ops!=xl6AsyncFileHandlePolicyV1Get()->Ops)return false;
    return visit((xrtownershipref){raw,&async_file_ops},context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6AsyncFileHandlePolicyV1Get,xl6_xrt_fs_async_destroy,async_file_trace)
