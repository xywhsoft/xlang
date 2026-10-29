#ifndef XL6_EXECUTION_NET_FILE_H
#define XL6_EXECUTION_NET_FILE_H
#include "xl6_execution_resource.h"
struct xfileoptions;
struct xnetfile;
struct xbytesview;
struct xnetfiledataownershipv1;
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionNetFileCreateV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,uint32_t,const char*,const struct xfileoptions*);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionNetFileReadV1(XL6_ExecutionResourceV1*,uint64_t,size_t,struct xcancel*);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionNetFileWriteV1(XL6_ExecutionResourceV1*,uint64_t,struct xbytesview,struct xcancel*);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionNetFileWriteRefV1(XL6_ExecutionResourceV1*,uint64_t,
    struct xbytesview,void*,const struct xnetfiledataownershipv1*,struct xcancel*);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionNetFileCloseSourceV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API const struct XL6_ExecutionFamilyV1* xl6ExecutionNetFileFamilyV1Get(void);
#endif
