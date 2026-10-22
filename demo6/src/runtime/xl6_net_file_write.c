#include "xl6_net_file_internal.h"
static xfuture* writeView(XL6_RuntimeServicesV1 services,void* file,uint64_t offset,xbytesview view,void* cancel,int64_t size)
{
    if(!xl6BufferSelect(&view,size))return xl6NetFileFailedInternalV1(NULL,NULL);
    struct XL6_NetFileCaptureV1* capture=xl6NetFilePrepareInternalV1(file,XL6_NET_FILE_WRITE);
    if(!capture)return NULL;
    /* Text views and mutable Buffer cannot escape. Native submission snapshots
     * the selected bytes once before acceptance; no temporary Buffer wrapper. */
    return xl6NetFilePublishInternalV1(services,capture,xl6ExecutionNetFileWriteV1(file,offset,view,cancel));
}
xfuture* xl6NetFileWriteTextCancelV1(XL6_RuntimeServicesV1 services,void* file,uint64_t offset,xstrview text,void* cancel,int64_t size)
{return writeView(services,file,offset,(xbytesview){(const uint8_t*)text.Data,text.Size},cancel,size);}
xfuture* xl6NetFileWriteTextV1(XL6_RuntimeServicesV1 services,void* file,uint64_t offset,xstrview text,int64_t size)
{return xl6NetFileWriteTextCancelV1(services,file,offset,text,NULL,size);}
xfuture* xl6NetFileWriteBufferCancelV1(XL6_RuntimeServicesV1 services,void* file,uint64_t offset,void* input,void* cancel,int64_t size)
{
    xbytesview view={0};if(!xl6BufferInput(input,size,&view))return xl6NetFileFailedInternalV1(NULL,NULL);
    return writeView(services,file,offset,view,cancel,-1);
}
xfuture* xl6NetFileWriteBufferV1(XL6_RuntimeServicesV1 services,void* file,uint64_t offset,void* input,int64_t size)
{return xl6NetFileWriteBufferCancelV1(services,file,offset,input,NULL,size);}
