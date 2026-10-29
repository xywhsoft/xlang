#ifndef XL6_NET_FILE_INTERNAL_H
#define XL6_NET_FILE_INTERNAL_H
#include "xl6_buffer_internal.h"
#include "xl6_execution_net_file.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
typedef enum XL6_NetFileResultV1 {XL6_NET_FILE_READ,XL6_NET_FILE_WRITE,XL6_NET_FILE_CLOSE} XL6_NetFileResultV1;
struct XL6_NetFileCaptureV1;
struct XL6_NetFileCaptureV1* xl6NetFilePrepareInternalV1(void*,XL6_NetFileResultV1);
xfuture* xl6NetFilePublishInternalV1(XL6_RuntimeServicesV1,struct XL6_NetFileCaptureV1*,xfuture*);
xfuture* xl6NetFileFailedInternalV1(struct XL6_NetFileCaptureV1*,xfuture*);
static inline void xl6NetFileFailedV1(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
#endif
