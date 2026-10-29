#include "xl6_net_file_internal.h"

void* xl6NetFileCreateV1(XL6_ExecutionServicesV1 services,void* engine,xstrview path,
    int64_t flags,int64_t mode,int64_t share,int64_t worker)
{
    if((uint64_t)flags>UINT32_MAX||(uint64_t)mode>UINT32_MAX||
        (uint64_t)share>UINT32_MAX||(uint64_t)worker>UINT32_MAX||path.Size>INT64_MAX||path.Size==SIZE_MAX){
        xrtSetErrorKind(XERR_RANGE);xl6NetFileFailedV1();return NULL;}
    if(!path.Data||(path.Size&&memchr(path.Data,0,path.Size))){
        xrtSetErrorKind(XERR_ARGUMENT);xl6NetFileFailedV1();return NULL;}
    /* Preserve explicit length even for resident ABI callers whose view has no
     * trailing terminator. This call-only copy is never retained by XRT. */
    char* name=xrtStrDupView(path);if(!name){xl6NetFileFailedV1();return NULL;}
    xfileoptions options={(uint32_t)flags,(uint32_t)mode,(uint32_t)share};
    void* result=xl6ExecutionNetFileCreateV1(services.source,engine,(uint32_t)worker,name,&options);
    xrtFree(name);if(!result)xl6NetFileFailedV1();return result;
}
void xl6NetFileDestroyV1(void* file){xl6ExecutionResourceReleaseV1(file);}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&
        ops==xl6NetFileHandlePolicyV1Get()->Ops&&visit(xl6ExecutionResourceOwnershipV1(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6NetFileHandlePolicyV1Get,xl6NetFileDestroyV1,trace)
