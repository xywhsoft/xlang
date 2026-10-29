#include "xl6_execution_resource_internal.h"
#include "xl6_execution_net_engine.h"
#include "xl6_execution_net_file.h"
#include <stdlib.h>

typedef XL6_ExecutionResourceV1 Resource;
typedef struct XL6_NetFileCreateOptionsV1 {
    Resource* engine;
    uint32_t worker;
    const char* path;
    const xfileoptions* options;
} Options;
typedef struct File {Resource base;Resource* parent;xatomicptr native,close;} File;
_Static_assert(offsetof(File,base)==0,"File resource base must be first");
static xnetfile* native(const Resource* resource)
{return xrtAtomicPtrLoad(&((const File*)resource)->native,XMEMORY_ACQUIRE);}
static void parentDrop(File* file)
{
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    Resource* parent=file->parent;file->parent=NULL;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xl6ExecutionResourceReleaseV1(parent);
}
static bool create(Resource* base,const XL6_ExecutionCreateOptionsV1* options)
{
    File* file=(File*)base;const Options* input=options->value.file;
    xrtAtomicPtrInit(&file->native,NULL);xrtAtomicPtrInit(&file->close,NULL);
    if(!input||!input->path){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(input->engine,true,&scope))return false;
    if(input->engine->family!=xl6ExecutionNetEngineFamilyV1Get()||!scope.engine){
        xl6ExecutionResourceLeaveV1(&scope);xrtSetErrorKind(XERR_TYPE);return false;}
    file->parent=xl6ExecutionResourceRetainV1(input->engine);
    xnetfile* result=file->parent?xrtNetFileCreate(scope.engine,input->worker,input->path,input->options):NULL;
    xrtAtomicPtrStore(&file->native,result,XMEMORY_RELEASE);xl6ExecutionResourceLeaveV1(&scope);
    return result!=NULL;
}
static bool dispose(Resource* base)
{
    File* file=(File*)base;xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    xnetfile* handle=xrtAtomicPtrExchange(&file->native,NULL,XMEMORY_ACQ_REL);
    xfuture* close=xrtAtomicPtrExchange(&file->close,NULL,XMEMORY_ACQ_REL);
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xrtFutureDestroy(close);xrtNetFileDestroy(handle);parentDrop(file);return true;
}
static bool rollback(Resource* base)
{
    /* A created File requires asynchronous, checked close. Keep it in the
     * exact source record even when publication loses a source-close race. */
    return native(base)?false:dispose(base);
}
static bool closeFile(Resource* base)
{
    File* file=(File*)base;xnetfile* handle=native(base);
    if(!handle||xrtAtomicPtrLoad(&file->close,XMEMORY_ACQUIRE))return true;
    xfuture* future=xrtNetFileCloseFuture(handle);if(!future)return false;
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    void* expected=NULL;
    bool stored=xrtAtomicPtrCompareExchange(&file->close,&expected,future,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE);
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    if(!stored)xrtFutureDestroy(future);
    return true;
}
static XL6_ModulePrepareResult retire(Resource* base)
{
    File* file=(File*)base;
    if(!native(base))return dispose(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
    if(!closeFile(base))return XL6_MODULE_PREPARE_FAILED;
    xfuture* close=xrtAtomicPtrLoad(&file->close,XMEMORY_ACQUIRE);
    xfuturestate state=xrtFutureState(close);
    if(state==XFUTURE_PENDING)return XL6_MODULE_PREPARE_BUSY;
    if(state!=XFUTURE_RESOLVED){
        /* Physical close already consumed the native handle. Retire those
         * closed slots, then latch its authoritative checked failure. Retry
         * can remove the empty record without repeating the OS side effect. */
        xerror* error=xrtErrorRef(xrtFutureError(close));
        (void)dispose(base);if(!error)xrtSetErrorKind(XERR_STATE);else xrtSetErrorTake(error);
        return XL6_MODULE_PREPARE_FAILED;
    }
    return dispose(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
}
static void drop(Resource* base)
{if(native(base)||((File*)base)->parent||xrtAtomicPtrLoad(&((File*)base)->close,XMEMORY_ACQUIRE))abort();}
static bool present(const Resource* base){return native(base)!=NULL;}
static bool trace(const Resource* base,xrtownershipvisitor visit,void* context)
{
    const File* file=(const File*)base;xnetfile* handle=native(base);
    xfuture* close=xrtAtomicPtrLoad(&file->close,XMEMORY_ACQUIRE);
    return (!handle||visit(xrtNetFileOwnership(handle),context))&&
        (!close||visit(xrtFutureOwnership(close),context))&&
        (!file->parent||visit(xl6ExecutionResourceOwnershipV1(file->parent),context));
}
const XL6_ExecutionFamilyV1* xl6ExecutionNetFileFamilyV1Get(void)
{
    static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,XL6_EXECUTION_NET_FILE_V1,
        create,rollback,retire,dispose,drop,trace,closeFile,closeFile,sizeof(File),present};return &family;
}
Resource* xl6ExecutionNetFileCreateV1(XL6_ExecutionSourceV1* source,Resource* engine,
    uint32_t worker,const char* path,const xfileoptions* options)
{
    Options file={engine,worker,path,options};
    XL6_ExecutionCreateOptionsV1 input={.kind=XL6_EXECUTION_NET_FILE_V1,.deferRollback=true,.value.file=&file};
    return xl6ExecutionCreateInternalV1(source,xl6ExecutionNetFileFamilyV1Get(),&input);
}
static bool enter(Resource* resource,bool submit,XL6_ExecutionScopeV1* scope,xnetfile** output)
{
    if(!xl6ExecutionResourceEnterV1(resource,submit,scope))return false;
    if(resource->family!=xl6ExecutionNetFileFamilyV1Get()||!(*output=native(resource))){
        xl6ExecutionResourceLeaveV1(scope);xrtSetErrorKind(XERR_TYPE);return false;}
    return true;
}
xfuture* xl6ExecutionNetFileReadV1(Resource* resource,uint64_t offset,size_t bytes,xcancel* cancel)
{
    XL6_ExecutionScopeV1 scope={0};xnetfile* file=NULL;
    if(!enter(resource,true,&scope,&file))return NULL;
    xfuture* future=xrtNetFileReadFuture(file,offset,bytes,cancel);
    xl6ExecutionResourceLeaveV1(&scope);return future;
}
xfuture* xl6ExecutionNetFileWriteV1(Resource* resource,uint64_t offset,xbytesview input,xcancel* cancel)
{
    XL6_ExecutionScopeV1 scope={0};xnetfile* file=NULL;
    if(!enter(resource,true,&scope,&file))return NULL;
    xfuture* future=xrtNetFileWriteFuture(file,offset,input,cancel);
    xl6ExecutionResourceLeaveV1(&scope);return future;
}
xfuture* xl6ExecutionNetFileWriteRefV1(Resource* resource,uint64_t offset,xbytesview input,
    void* owner,const xnetfiledataownershipv1* policy,xcancel* cancel)
{
    XL6_ExecutionScopeV1 scope={0};xnetfile* file=NULL;
    if(!enter(resource,true,&scope,&file))return NULL;
    xfuture* future=xrtNetFileWriteRefPolicyV1(file,offset,input,owner,policy,cancel);
    xl6ExecutionResourceLeaveV1(&scope);return future;
}
xfuture* xl6ExecutionNetFileCloseSourceV1(Resource* resource)
{
    if(!xl6ExecutionResourceCloseV1(resource))return NULL;
    XL6_ExecutionScopeV1 scope={0};xnetfile* file=NULL;
    if(!enter(resource,false,&scope,&file))return NULL;
    xfuture* future=xrtFutureRef(xrtAtomicPtrLoad(&((File*)resource)->close,XMEMORY_ACQUIRE));
    xl6ExecutionResourceLeaveV1(&scope);return future;
}
