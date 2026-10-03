#ifndef XL6_PROCESS_PIPELINE_INTERNAL_H
#define XL6_PROCESS_PIPELINE_INTERNAL_H
#include "src/runtime/xl6_process_config.h"

typedef struct XL6_ProcessPipeline {
    XL6_TextThreadOwner owner;
    xprocessconfig* stages;
    size_t count;
    size_t capacity;
} XL6_ProcessPipeline;
static inline XL6_ProcessPipeline* xl6ProcessPipelineCheck(void* handle)
{
    if(!handle){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_ProcessPipeline* pipeline=handle;
    return xl6TextThreadCheck(&pipeline->owner) ? pipeline : NULL;
}
#endif
