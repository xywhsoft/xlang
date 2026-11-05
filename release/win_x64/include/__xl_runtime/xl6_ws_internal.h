#ifndef XL6_WS_INTERNAL_H
#define XL6_WS_INTERNAL_H
#include "xl6_buffer_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <stdio.h>

/* Private physical records. All returned text/bytes owns its actual length;
 * none of the language entry points imports an arbitrary native pointer. */
typedef struct XL6_WsFrame {xwsframe value;xwsframeconfig config;} XL6_WsFrame;
typedef struct XL6_WsClose {uint16 code;size_t size;char text[];} XL6_WsClose;
typedef struct XL6_WsDeflate {xwsdeflate value;} XL6_WsDeflate;
typedef struct XL6_WsGuard {xatomic32* gate;xrtownershipscope mutation;} XL6_WsGuard;
static inline bool xl6WsEnter(xatomic32* gate,XL6_WsGuard* guard)
{
    if(!xrtOwnershipMutationBegin(&guard->mutation))return false;
    uint32 available=0;
    if(!xrtAtomic32CompareExchange(gate,&available,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED)){
        if(!xrtOwnershipScopeEnd(&guard->mutation))abort();
        xrtSetErrorKind(XERR_STATE);return false;
    }
    guard->gate=gate;return true;
}
static inline void xl6WsLeave(XL6_WsGuard* guard)
{
    xrtAtomic32Store(guard->gate,0,XMEMORY_RELEASE);guard->gate=NULL;
    if(!xrtOwnershipScopeEnd(&guard->mutation))abort();
}
static inline bool xl6WsIndex(int64_t index,size_t count)
{if(index>=0&&(uint64_t)index<count)return true;xrtSetErrorKind(XERR_RANGE);return false;}
static inline bool xl6WsSize(int64_t n,size_t* result)
{if(n>=0&&(uint64_t)n<=SIZE_MAX){*result=(size_t)n;return true;}xrtSetErrorKind(XERR_RANGE);return false;}
static inline bool xl6WsU16(int64_t n,uint16* result)
{if(n>=0&&n<=UINT16_MAX){*result=(uint16)n;return true;}xrtSetErrorKind(XERR_RANGE);return false;}
static inline bool xl6WsMask(int64_t n,uint8 mask[4])
{if(n<0||(uint64_t)n>UINT32_MAX){xrtSetErrorKind(XERR_RANGE);return false;}for(unsigned i=0;i<4;++i)mask[i]=(uint8)((uint64_t)n>>(24-8*i));return true;}
static inline xbytesview xl6WsAsBytes(xstrview s){return (xbytesview){(const uint8*)s.Data,s.Size};}
static inline xstrview xl6WsAsText(xbytesview s){return (xstrview){(const char*)s.Data,s.Size};}
static inline xstrview xl6WsText(xstrview s)
{if(xrtUtf8Count(s)==XRT_NPOS){xrtSetErrorKind(XERR_VALUE);return xl6TextFailure;}return xl6TextCopy(s);}
static inline bool xl6WsAppend(xbuffer* output,xbytesview data)
{if(data.Size>(size_t)INT64_MAX-output->Size){xrtSetErrorKind(XERR_RANGE);return false;}return xl6BufferAllocation(xrtBufferAppend(output,data));}
static inline bool xl6WsOutput(void* raw,xbytesview input,xbuffer** output)
{
    *output=xl6BufferChecked(raw);if(!*output)return false;
    uintptr_t a=(uintptr_t)input.Data,b=(uintptr_t)(*output)->Data;
    if((input.Size&&(!a||input.Size>UINTPTR_MAX-a))||((*output)->Capacity&&(!b||(*output)->Capacity>UINTPTR_MAX-b))||
       (input.Size&&(*output)->Capacity&&a<b+(*output)->Capacity&&b<a+input.Size)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    return true;
}
static inline void xl6WsError(const char* domain,int code,size_t offset)
{
    char message[128];snprintf(message,sizeof(message),"WebSocket error %d at byte %llu",code,(unsigned long long)offset);
    xerrkind kind=xrtErrorKind(xrtGetError());
    xrtSetErrorInfo(kind==XERR_NONE?XERR_PROTOCOL:kind,domain,code,message);
}
static inline bool xl6WsChecked(bool ok)
{if(!ok&&xrtErrorKind(xrtGetError())==XERR_NONE)xrtSetErrorKind(XERR_STATE);return ok;}
#endif
