#ifndef XL6_NET_BUFFER_INTERNAL_H
#define XL6_NET_BUFFER_INTERNAL_H
#include "xl6_buffer_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
/* No borrowed spans, external releasers, pool or language references. Blocks
 * own storage or immutable native NetBytes references. A framer embeds this record as its first
 * member and never exposes mutations of the retained search prefix. */
typedef struct XL6_NetBufferV1 {
    xatomic32 gate;
    xnetbuf native;
    size_t limit;
    bool faulted;
} XL6_NetBufferV1;
typedef struct XL6_NetBufferGuardV1 {
    XL6_NetBufferV1* owner;
    xrtownershipscope mutation;
} XL6_NetBufferGuardV1;
/* Reject overlapping/reentrant use before touching data; never spin or wait
 * behind a potentially large copy/allocation. Caller synchronization can
 * serialize shared use. This guard also excludes ownership Freeze. */
bool xl6NetBufferEnterV1(XL6_NetBufferV1*,XL6_NetBufferGuardV1*);
void xl6NetBufferLeaveV1(XL6_NetBufferGuardV1*);
bool xl6NetBufferInitializeV1(XL6_NetBufferV1*,int64_t);
bool xl6NetBufferSelectRangeV1(const XL6_NetBufferV1*,int64_t,int64_t,size_t*);
bool xl6NetBufferAppendViewV1(XL6_NetBufferV1*,xbytesview,bool);
void xl6NetBufferMoveInputV1(XL6_NetBufferV1*,XL6_NetBufferV1*);
/* Caller owns the buffer guard. Logical output/source change only on success. */
bool xl6NetBufferWriteIntoV1(const XL6_NetBufferV1*,xbuffer*,size_t,size_t);
static inline void xl6NetBufferFailedV1(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static inline bool xl6NetBufferCanAppendV1(const XL6_NetBufferV1* buffer,size_t size)
{
    if(buffer->faulted){xrtSetErrorKind(XERR_STATE);return false;}
    if(size>buffer->limit-buffer->native.Size){xrtSetErrorKind(XERR_RANGE);return false;}
    return true;
}
/* Every owner of an embedded buffer must expose its actual native edges.
 * Freeze excludes mutation; tracing must never acquire a mutation guard. */
#define XL6_DEFINE_NET_BUFFER_HANDLE_POLICY(Getter,Destroy,Buffer) \
    static bool Getter##Trace(const xvalue* value,xrtownershipvisitor visit,void* context) { \
        void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL; \
        if(!visit||!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||ops!=Getter()->Ops)return false; \
        const XL6_NetBufferV1* buffer=(Buffer); \
        return !xrtAtomic32Load(&buffer->gate,XMEMORY_ACQUIRE)&& \
            xrtNetBufOwnershipTrace(&buffer->native,visit,context); \
    } \
    XL6_DEFINE_NATIVE_HANDLE_POLICY(Getter,Destroy,Getter##Trace)
#endif
