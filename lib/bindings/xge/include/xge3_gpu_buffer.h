#ifndef XL6_XGE3_GPU_BUFFER_H
#define XL6_XGE3_GPU_BUFFER_H
#include "xge3_render_resources.h"
XL6_XGE_RECORD_POLICY(xl6Xge3GpuBufferPolicy,"xge.gpuBuffer",xge_buffer_t,xgeBufferFree)
#define xl6Xge3GpuBufferPolicy (*xl6Xge3GpuBufferPolicyGet())
#endif
