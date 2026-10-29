#include "src/runtime/xl6_process_pipeline_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

void xl6ProcessPipelineDestroyV1(void* handle)
{
    XL6_ProcessPipeline* pipeline=handle;if(!pipeline)return;
    xerror* prior=xrtTakeError();
    for(size_t i=0;i<pipeline->count;++i)xl6ProcessConfigUnit(&pipeline->stages[i]);
    xrtFree(pipeline->stages);xl6TextThreadDrop(&pipeline->owner);xrtFree(pipeline);
    xrtClearError();xrtSetErrorTake(prior);
}
void* xl6ProcessPipelineCreateV1(void)
{
    XL6_ProcessPipeline* pipeline=xrtCalloc(1,sizeof(*pipeline));if(!pipeline)return NULL;
    if(!xl6TextThreadInit(&pipeline->owner)){xl6ProcessPipelineDestroyV1(pipeline);return NULL;}
    return pipeline;
}
void xl6ProcessPipelineAddV1(void* handle,void* commandHandle)
{
    XL6_ProcessPipeline* pipeline=xl6ProcessPipelineCheck(handle);if(!pipeline)return;
    XL6_ProcessCommand* command=xl6ProcessCommandCheck(commandHandle);if(!command)return;
    const xprocessconfig* source=&command->config;
    /* A snapshot cannot borrow native HANDLEs or represent a terminal stage.
     * File/PTY adapters have separate owned-capability contracts. */
    if(source->Terminal || source->Stdin.Mode==XPROCESS_IO_HANDLE ||
       source->Stdout.Mode==XPROCESS_IO_HANDLE || source->Stderr.Mode==XPROCESS_IO_HANDLE){
        xrtSetErrorKind(XERR_UNSUPPORTED);return;
    }
    if(pipeline->count==INT64_MAX){xrtSetErrorKind(XERR_RANGE);return;}
    xprocessconfig copy;if(!xl6ProcessConfigClone(&copy,source))return;
    if(pipeline->count==pipeline->capacity){
        size_t capacity=pipeline->capacity ? pipeline->capacity*2 : 8;
        if(capacity<pipeline->capacity || capacity>SIZE_MAX/sizeof(*pipeline->stages) || capacity>INT64_MAX){
            xrtSetErrorKind(XERR_RANGE);xl6ProcessConfigUnit(&copy);return;
        }
        xprocessconfig* grown=xrtRealloc(pipeline->stages,capacity*sizeof(*grown));
        if(!grown){xl6ProcessConfigUnit(&copy);return;}
        pipeline->stages=grown;pipeline->capacity=capacity;
    }
    pipeline->stages[pipeline->count++]=copy;
}
void xl6ProcessPipelineClearV1(void* handle)
{
    XL6_ProcessPipeline* pipeline=xl6ProcessPipelineCheck(handle);if(!pipeline)return;
    for(size_t i=0;i<pipeline->count;++i)xl6ProcessConfigUnit(&pipeline->stages[i]);
    pipeline->count=0;
}
int64_t xl6ProcessPipelineCountV1(void* handle)
{XL6_ProcessPipeline* pipeline=xl6ProcessPipelineCheck(handle);return pipeline ? (int64_t)pipeline->count : 0;}

/* Resident policy: terminal Drop runs after the phased handle is detached. */
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6ProcessPipelineHandlePolicyV1Get,xl6ProcessPipelineDestroyV1)
