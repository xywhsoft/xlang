#ifndef XL6_NATIVE_FRAME_GRAPH_H
#define XL6_NATIVE_FRAME_GRAPH_H
#include "xl6_capture_v3.h"
#include "xl6_continuation_future.h"
#include "xl6_generator.h"
#include "xl6_task.h"
#include "xl6_future_callback.h"
/* Explicit graph admission. Old leaf-only factories remain closed. These
 * entries use the same physical layouts but retain the graph capture engine
 * in the resident allocation, for trace, rollback and terminal cleanup. */
XL6_CODE_LEASE_API void* xl6ContinuationFrameAllocateGraphV1(size_t,const XL6_ContinuationFrameLayoutV3*);
XL6_CODE_LEASE_API void* xl6GeneratorFrameAllocateGraphV1(size_t,const XL6_ActivationCaptureLayoutV2*);
XL6_CODE_LEASE_API XL6_Generator* xl6GeneratorCreateGraphV1(const XL6_CodeOwner*,void*,XL6_GeneratorResume,const XL6_CaptureLayoutV2*);
XL6_CODE_LEASE_API void* xl6TaskFrameAllocateGraphV1(const XL6_CodeOwner*,size_t,XL6_TaskRun,const XL6_CaptureLayoutV2*);
XL6_CODE_LEASE_API void* xl6CallbackFrameAllocateGraphV1(const XL6_CodeOwner*,size_t,XL6_FutureCallbackProc,const XL6_CaptureLayoutV2*);
#endif
