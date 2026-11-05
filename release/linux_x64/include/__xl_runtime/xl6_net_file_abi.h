#ifndef XL6_NET_FILE_ABI_H
#define XL6_NET_FILE_ABI_H
#include "xl6_runtime_services.h"
#include "xl6_execution_language.h"
/* Resident linkage ABI. Paths reject embedded NUL; opening is synchronous.
 * Native I/O remains bound to the selected Engine/worker, without fallback. */
XL6_TEXT_API void* xl6NetFileCreateV1(XL6_ExecutionServicesV1,void*,xstrview,int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API void xl6NetFileDestroyV1(void*);
XL6_TEXT_API struct xfuture* xl6NetFileReadV1(XL6_RuntimeServicesV1,void*,uint64_t,int64_t);
XL6_TEXT_API struct xfuture* xl6NetFileReadCancelV1(XL6_RuntimeServicesV1,void*,uint64_t,int64_t,void*);
XL6_TEXT_API struct xfuture* xl6NetFileWriteTextV1(XL6_RuntimeServicesV1,void*,uint64_t,xstrview,int64_t);
XL6_TEXT_API struct xfuture* xl6NetFileWriteTextCancelV1(XL6_RuntimeServicesV1,void*,uint64_t,xstrview,void*,int64_t);
XL6_TEXT_API struct xfuture* xl6NetFileWriteBytesV1(XL6_RuntimeServicesV1,void*,uint64_t,const xvalue*,int64_t);
XL6_TEXT_API struct xfuture* xl6NetFileWriteBytesCancelV1(XL6_RuntimeServicesV1,void*,uint64_t,const xvalue*,void*,int64_t);
XL6_TEXT_API struct xfuture* xl6NetFileWriteBufferV1(XL6_RuntimeServicesV1,void*,uint64_t,void*,int64_t);
XL6_TEXT_API struct xfuture* xl6NetFileWriteBufferCancelV1(XL6_RuntimeServicesV1,void*,uint64_t,void*,void*,int64_t);
XL6_TEXT_API struct xfuture* xl6NetFileCloseV1(XL6_RuntimeServicesV1,void*);
struct xnetfiledataownershipv1;
XL6_TEXT_API const struct xnetfiledataownershipv1* xl6NetFileBytesInputPolicyV1Get(void);
#endif
