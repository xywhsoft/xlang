#ifndef XL6_CONCURRENT_QUEUE_ABI_H
#define XL6_CONCURRENT_QUEUE_ABI_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct xvalue xvalue;
typedef struct xrttype xrttype;
typedef struct xvalueconverter xvalueconverter;

/* Exact XRT queue result declarations for standalone generated C. Native
 * providers use the original XRT declarations; the ABI gate tests both sides
 * of the 16/24-byte returns with GCC and TinyCC on Windows and Linux. No XRT
 * implementation or broad runtime declaration header enters generated C. */
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
typedef enum xqueueresult {
    XQUEUE_ERROR=-1, XQUEUE_OK=0, XQUEUE_EMPTY=1, XQUEUE_FULL=2, XQUEUE_CLOSED=3
} xqueueresult;
typedef struct xqueuebatchresult { xqueueresult Result; size_t Count; } xqueuebatchresult;
#endif

typedef struct XL6_QueueArrayResultV2 {
    xqueueresult Result;
    size_t Count;
    xvalue* Items;
} XL6_QueueArrayResultV2;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_QUEUE_API __declspec(dllexport)
#else
#define XL6_QUEUE_API
#endif
#define XL6_DECLARE_QUEUE_ABI(Name) \
    XL6_QUEUE_API xvalue* xl6##Name##CreateTakeV1(uint64_t,xvalue*,int64_t); \
    XL6_QUEUE_API xqueueresult xl6##Name##PushV1(const xvalue*,uint64_t,uint64_t,size_t,const xrttype*,const void*); \
    XL6_QUEUE_API xqueueresult xl6##Name##PopV1(const xvalue*,uint64_t,uint64_t,size_t,const xrttype*,void*); \
    XL6_QUEUE_API xqueuebatchresult xl6##Name##PushArrayV2(const xvalue*,uint64_t,const xvalue*,uint64_t); \
    XL6_QUEUE_API XL6_QueueArrayResultV2 xl6##Name##PopArrayV2(const xvalue*,uint64_t,uint64_t,int64_t,const xvalueconverter*); \
    XL6_QUEUE_API int64_t xl6##Name##QueryV1(const xvalue*,uint64_t,unsigned int); \
    XL6_QUEUE_API bool xl6##Name##CloseV1(const xvalue*,uint64_t); \
    XL6_QUEUE_API bool xl6##Name##ResetV1(const xvalue*,uint64_t)
XL6_DECLARE_QUEUE_ABI(SpscQueue);
XL6_DECLARE_QUEUE_ABI(MpscQueue);
XL6_DECLARE_QUEUE_ABI(MpmcQueue);
#undef XL6_DECLARE_QUEUE_ABI
#undef XL6_QUEUE_API
enum { XL6_QUEUE_QUERY_COUNT_V1=1, XL6_QUEUE_QUERY_CAPACITY_V1,
       XL6_QUEUE_QUERY_CLOSED_V1, XL6_QUEUE_QUERY_DRAINED_V1 };
#endif
