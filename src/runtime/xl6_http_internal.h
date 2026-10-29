#ifndef XL6_HTTP_INTERNAL_H
#define XL6_HTTP_INTERNAL_H
#include "xl6_buffer_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <stdio.h>

/* These are private physical records. No language pointer importer or borrowed
 * result exists. XRT implements all grammar, framing and coding operations. */
typedef struct XL6_HttpGuard {xatomic32* gate;xrtownershipscope mutation;} XL6_HttpGuard;
static inline bool xl6HttpEnter(xatomic32* gate,XL6_HttpGuard* guard)
{
    if(!gate||!guard||guard->gate){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xrtOwnershipMutationBegin(&guard->mutation))return false;
    uint32 available=0;
    if(!xrtAtomic32CompareExchange(gate,&available,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED)){
        if(!xrtOwnershipScopeEnd(&guard->mutation))abort();
        xrtSetErrorKind(XERR_STATE);return false;
    }
    guard->gate=gate;return true;
}
static inline void xl6HttpLeave(XL6_HttpGuard* guard)
{
    if(!guard->gate)abort();
    xrtAtomic32Store(guard->gate,0,XMEMORY_RELEASE);guard->gate=NULL;
    if(!xrtOwnershipScopeEnd(&guard->mutation))abort();
}
static inline bool xl6HttpIndex(int64_t index,size_t count)
{if(index>=0&&(uint64_t)index<count)return true;xrtSetErrorKind(XERR_RANGE);return false;}
static inline bool xl6HttpSize(int64_t value,size_t* result)
{if(value>=0&&(uint64_t)value<=SIZE_MAX){*result=(size_t)value;return true;}xrtSetErrorKind(XERR_RANGE);return false;}
static inline xstrview xl6HttpAsText(xbytesview data)
{return (xstrview){(const char*)data.Data,data.Size};}
static inline xbytesview xl6HttpAsBytes(xstrview text)
{return (xbytesview){(const uint8*)text.Data,text.Size};}
static inline xstrview xl6HttpRebase(xstrview view,const void* old,const void* data)
{return view.Data?(xstrview){(const char*)data+((const uint8*)view.Data-(const uint8*)old),view.Size}:(xstrview){NULL,0};}
static inline bool xl6HttpU16(int64_t value,uint16* result)
{if(value>=0&&value<=UINT16_MAX){*result=(uint16)value;return true;}xrtSetErrorKind(XERR_RANGE);return false;}
static inline xstrview xl6HttpText(xstrview text)
{
    if(xrtUtf8Count(text)==XRT_NPOS){xrtSetErrorKind(XERR_VALUE);return xl6TextFailure;}
    return xl6TextCopy(text);
}
static inline xvalue* xl6HttpBytes(xbytesview data)
{return xrtValueBytes(data);}
static inline bool xl6HttpAppend(xbuffer* output,xbytesview data)
{
    if(data.Size>(size_t)INT64_MAX-output->Size){xrtSetErrorKind(XERR_RANGE);return false;}
    return xl6BufferAllocation(xrtBufferAppend(output,data));
}
static inline void xl6HttpParseError(const xhttp1errorinfo* info)
{
    char message[160];snprintf(message,sizeof(message),"HTTP/1 parse error %d at byte %llu, line %llu",
        (int)info->Code,(unsigned long long)info->Offset,(unsigned long long)info->Line);
    xerrkind kind=xrtErrorKind(xrtGetError());
    xrtSetErrorInfo(kind==XERR_NONE?XERR_PROTOCOL:kind,"http1",(int32)info->Code,message);
}
typedef struct XL6_HttpFieldOffset {size_t name,value;size_t nameSize,valueSize;} XL6_HttpFieldOffset;
typedef struct XL6_HttpHeaders {
    xatomic32 gate;xbuffer storage;XL6_HttpFieldOffset* offsets;xhttpfield* fields;
    size_t count,capacity;int64_t parsedBytes;bool dirty;
} XL6_HttpHeaders;
bool xl6HttpHeadersBorrow(XL6_HttpHeaders*,XL6_HttpGuard*,const xhttpfield**,size_t*);
XL6_HttpHeaders* xl6HttpHeadersCopyFields(const xhttpfield*,size_t);
void xl6HttpHeadersDispose(XL6_HttpHeaders*);
typedef struct XL6_HttpAuthority {xhttpauthority value;char data[];} XL6_HttpAuthority;
XL6_HttpAuthority* xl6HttpAuthorityCopy(const xhttpauthority*);
typedef struct XL6_HttpTarget {xhttptarget value;char data[];} XL6_HttpTarget;
void* xl6HttpParametersFragment(xstrview);
typedef struct XL6_HttpTokens {size_t count;xstrview items[];} XL6_HttpTokens;
XL6_HttpTokens* xl6HttpTokensCopyFields(const xhttpfield*,size_t,xstrview);
static inline bool xl6HttpAccumulate(size_t* total,size_t amount)
{if(amount>SIZE_MAX-*total){xrtSetErrorKind(XERR_RANGE);return false;}*total+=amount;return true;}
static inline xstrview xl6HttpStoreView(xstrview value,char** storage)
{if(value.Size)memcpy(*storage,value.Data,value.Size);xstrview copy={*storage,value.Size};*storage+=value.Size;return copy;}
typedef struct XL6_HttpHead {xhttp1head value;xhttpfield fields[];} XL6_HttpHead;
XL6_HttpHead* xl6HttpHeadCopy(const xhttp1head*,xbytesview);
static inline bool xl6HttpHeadLimits(xhttp1limits* limits,int64_t head,int64_t start,int64_t line,int64_t fields)
{
    if(!xl6HttpSize(head,&limits->MaxHead)||!xl6HttpSize(start,&limits->MaxStartLine)||
       !xl6HttpSize(line,&limits->MaxFieldLine)||!xl6HttpSize(fields,&limits->MaxFields))return false;
    if(limits->MaxHead<4||!limits->MaxStartLine||!limits->MaxFieldLine){xrtSetErrorKind(XERR_RANGE);return false;}return true;
}
static inline bool xl6HttpBodyLimits(xhttp1bodylimits* limits,int64_t body,int64_t chunk,int64_t trailer,int64_t line,int64_t fields)
{
    if(body< -1){xrtSetErrorKind(XERR_RANGE);return false;}
    limits->MaxBody=body<0?UINT64_MAX:(uint64)body;
    return xl6HttpSize(chunk,&limits->MaxChunkLine)&&xl6HttpSize(trailer,&limits->MaxTrailer)&&
        xl6HttpSize(line,&limits->MaxTrailerLine)&&xl6HttpSize(fields,&limits->MaxTrailers);
}
static inline bool xl6HttpOutput(void* raw,xbytesview input,xbuffer** output)
{
    *output=xl6BufferChecked(raw);if(!*output)return false;
    uintptr_t a=(uintptr_t)input.Data,b=(uintptr_t)(*output)->Data;size_t cap=(*output)->Capacity;
    if((input.Size&&(!a||input.Size>UINTPTR_MAX-a))||(cap&&(!b||cap>UINTPTR_MAX-b))||
       (input.Size&&cap&&a<b+cap&&b<a+input.Size)){xrtSetErrorKind(XERR_ARGUMENT);return false;}return true;
}
static inline xvalue* xl6HttpTakeBuffer(xbuffer* buffer)
{
    if(!buffer->Data)return xl6HttpBytes((xbytesview){NULL,0});
    bytes owned=buffer->Data;size_t size=buffer->Size;buffer->Data=NULL;buffer->Size=buffer->Capacity=0;
    xvalue* value=xrtValueBytesTake(&owned,size);xrtFree(owned);return value;
}
/* Compile-time input projections only: each entry performs its own exact
 * borrow/size check then calls the same local parser, without a public wrapper
 * call, operation-code dispatcher or intermediate Buffer/value. */
#define XL6_HTTP_BYTE_PARSERS(Prefix,Expression) \
void* Prefix##BytesV2(xvalue* source,int64_t size) \
{xbytesview input;if(!xl6BufferBytesInput(source,size,&input))return NULL;xstrview view=xl6HttpAsText(input);return (Expression);} \
void* Prefix##SpanV2(xbytesview input,int64_t size) \
{if(!xl6BufferSelect(&input,size))return NULL;xstrview view=xl6HttpAsText(input);return (Expression);} \
void* Prefix##BufferV2(void* source,int64_t size) \
{xbytesview input;if(!xl6BufferInput(source,size,&input))return NULL;xstrview view=xl6HttpAsText(input);return (Expression);}
#endif
