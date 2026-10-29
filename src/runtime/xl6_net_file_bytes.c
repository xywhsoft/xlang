#include "xl6_net_file_internal.h"
static xonce policyOnce=XRT_ONCE_INIT;
static xnetfiledataownershipv1 policy;
static void drop(void* value){xrtValueRelease(value);}
static bool init(void* unused)
{
    (void)unused;
    /* Preserve the canonical physical value Ops. A proxy Ops for this same
     * xvalue would split one physical node into two incompatible identities. */
    policy=(xnetfiledataownershipv1){sizeof(policy),drop,xrtValueOwnership(NULL).Ops};return true;
}
const xnetfiledataownershipv1* xl6NetFileBytesInputPolicyV1Get(void)
{return xrtOnce(&policyOnce,init,NULL)?&policy:NULL;}
xfuture* xl6NetFileWriteBytesCancelV1(XL6_RuntimeServicesV1 services,void* file,uint64_t offset,const xvalue* input,void* cancel,int64_t size)
{
    xbytesview view={0};if(!xl6BufferBytesInput(input,size,&view))return xl6NetFileFailedInternalV1(NULL,NULL);
    const xnetfiledataownershipv1* ownership=xl6NetFileBytesInputPolicyV1Get();
    if(!ownership)return xl6NetFileFailedInternalV1(NULL,NULL);
    struct XL6_NetFileCaptureV1* capture=xl6NetFilePrepareInternalV1(file,XL6_NET_FILE_WRITE);
    if(!capture)return NULL;
    /* Language bytes updates replace their value; no writable view/reference
     * escapes. Retain this exact immutable value, not a second payload copy. */
    xvalue* owner=xrtValueRetain(input);if(!owner)return xl6NetFileFailedInternalV1(capture,NULL);
    xfuture* source=xl6ExecutionNetFileWriteRefV1(file,offset,view,owner,ownership,cancel);
    if(!source)xrtValueRelease(owner); /* native failure consumes nothing */
    return xl6NetFilePublishInternalV1(services,capture,source);
}
xfuture* xl6NetFileWriteBytesV1(XL6_RuntimeServicesV1 services,void* file,uint64_t offset,const xvalue* input,int64_t size)
{return xl6NetFileWriteBytesCancelV1(services,file,offset,input,NULL,size);}
