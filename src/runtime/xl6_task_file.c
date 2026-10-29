#define XRT_MODULE_FILE_ASYNC
#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#include "src/common/xl6_xrt.h"
#include "xl6_task.h"
#include <stdlib.h>
/* Resident driver selected only for stateful asynchronous file owners. */
struct XL6_AsyncFile {
    XL6_TaskResourceV1 resource;
    XL6_TaskPool* pool; /* actual shell reference, survives module owner release */
    xmutex* gate;
    xatomic32 references; /* caller plus registry, independently owned */
    xasyncfile* native;
    xfuture* closed;
    uint32 flags;
};
static void async_file_lock(const XL6_AsyncFile* file) { if(!xrtMutexLock(file->gate))abort(); }
static void async_file_unlock(const XL6_AsyncFile* file) { if(!xrtMutexUnlock(file->gate))abort(); }
static void async_file_drop(XL6_AsyncFile* file)
{
    if(xrtAtomic32FetchSub(&file->references,1,XMEMORY_ACQ_REL)!=1)return;
    if(file->native || file->resource.registered)abort();
    xrtFutureDestroy(file->closed);if(!xrtMutexDestroy(file->gate))abort();xl6TaskResourceReleaseV1(&file->resource);xrtFree(file);
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
