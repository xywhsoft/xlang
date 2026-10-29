#ifndef XL6_CHANNEL_ABI_H
#define XL6_CHANNEL_ABI_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct xvalue xvalue;
typedef struct xrttype xrttype;
/* Declaration-only ABI shared by resident providers and standalone C. */
typedef enum XL6_ChannelStatusV1 {
    XL6_CHANNEL_OK_V1=0, XL6_CHANNEL_EMPTY_V1=1, XL6_CHANNEL_FULL_V1=2,
    XL6_CHANNEL_CLOSED_V1=3, XL6_CHANNEL_TIMEOUT_V1=4, XL6_CHANNEL_CANCELLED_V1=5
} XL6_ChannelStatusV1;
typedef enum XL6_ChannelWaitV1 {
    XL6_CHANNEL_TRY_V1=0, XL6_CHANNEL_WAIT_V1=1, XL6_CHANNEL_FOR_V1=2
} XL6_ChannelWaitV1;
typedef struct XL6_ChannelSlotV1 { void* data; size_t size; } XL6_ChannelSlotV1;
enum {
    XL6_CHANNEL_QUERY_COUNT_V1=0, XL6_CHANNEL_QUERY_CAPACITY_V1=1,
    XL6_CHANNEL_QUERY_CLOSED_V1=2, XL6_CHANNEL_QUERY_DRAINED_V1=3
};
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_CHANNEL_API __declspec(dllexport)
#else
#define XL6_CHANNEL_API
#endif
XL6_CHANNEL_API xvalue* xl6ChannelCreateTakeV1(uint64_t,xvalue*,int64_t);
XL6_CHANNEL_API bool xl6ChannelSendV1(const xvalue*,uint64_t,uint64_t,const xrttype*,
    XL6_ChannelSlotV1,XL6_ChannelWaitV1,int64_t,XL6_ChannelStatusV1*);
XL6_CHANNEL_API bool xl6ChannelReceiveV1(const xvalue*,uint64_t,uint64_t,const xrttype*,
    XL6_ChannelSlotV1,XL6_ChannelWaitV1,int64_t,XL6_ChannelStatusV1*);
XL6_CHANNEL_API int64_t xl6ChannelQueryV1(const xvalue*,uint64_t,unsigned int);
XL6_CHANNEL_API bool xl6ChannelCloseV1(const xvalue*,uint64_t);
#undef XL6_CHANNEL_API
#endif
