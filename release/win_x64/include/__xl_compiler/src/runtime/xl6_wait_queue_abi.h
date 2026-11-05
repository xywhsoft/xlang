#ifndef XL6_WAIT_QUEUE_ABI_H
#define XL6_WAIT_QUEUE_ABI_H
#include "xl6_concurrent_queue_abi.h"
#include "xl6_channel_abi.h"
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_WAIT_QUEUE_API __declspec(dllexport)
#else
#define XL6_WAIT_QUEUE_API
#endif
XL6_WAIT_QUEUE_API xvalue* xl6WaitQueueCreateTakeV1(uint64_t,xvalue*,int64_t);
XL6_WAIT_QUEUE_API xqueueresult xl6WaitQueuePushV1(const xvalue*,uint64_t,uint64_t,size_t,const xrttype*,const void*);
XL6_WAIT_QUEUE_API bool xl6WaitQueueReceiveV1(const xvalue*,uint64_t,uint64_t,const xrttype*,
    XL6_ChannelSlotV1,XL6_ChannelWaitV1,int64_t,XL6_ChannelStatusV1*);
XL6_WAIT_QUEUE_API int64_t xl6WaitQueueQueryV1(const xvalue*,uint64_t,unsigned int);
XL6_WAIT_QUEUE_API bool xl6WaitQueueCloseV1(const xvalue*,uint64_t);
XL6_WAIT_QUEUE_API xvalue* xl6SequenceWaitQueueCreateTakeV1(unsigned int,uint64_t,uint64_t,uint64_t,xvalue*,bool);
#undef XL6_WAIT_QUEUE_API
#endif
