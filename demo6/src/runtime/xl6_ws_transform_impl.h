/* Typed compile-time bindings. XRT owns the codec and every message state;
 * no function-pointer dispatcher, temporary Buffer or language callback. */
#include "xl6_ws_internal.h"
#if defined(XL6_WS_TRANSFORM_INFLATER)
#define WS_TYPE xwsinflater
#define WS_CONFIG xwsinflaterconfig
#define WS_NAME WsInflater
#define WS_NATIVE xrtWsInflater
#else
#define WS_TYPE xwsdeflater
#define WS_CONFIG xwsdeflaterconfig
#define WS_NAME WsDeflater
#define WS_NATIVE xrtWsDeflater
#endif
#define WS_CAT_INNER(A,B) A##B
#define WS_CAT(A,B) WS_CAT_INNER(A,B)
#define WS_N(Name) WS_CAT(WS_NATIVE,Name)
#define WS_ENTRY_INNER(Type,Name) xl6##Type##Name##V2
#define WS_ENTRY_EXPAND(Type,Name) WS_ENTRY_INNER(Type,Name)
#define WS_ENTRY(Name) WS_ENTRY_EXPAND(WS_NAME,Name)
#define WS_CFG_NAME WS_CAT(WS_NAME,Config)
#define WS_CFG(Name) WS_ENTRY_EXPAND(WS_CFG_NAME,Name)
typedef struct Transform {xatomic32 gate;WS_TYPE* value;} Transform;

void* WS_CFG(Create)(uint64_t limit,int64_t window,bool noContext,bool retain
#if defined(XL6_WS_TRANSFORM_DEFLATER)
    ,int64_t level,int64_t strategy
#endif
)
{
    if(window<8||window>15){xrtSetErrorKind(XERR_RANGE);return NULL;}
    WS_CONFIG config;WS_N(ConfigInit)(&config);
    config.OutputLimit=limit;config.WindowBits=(uint8)window;
    config.NoContextTakeover=noContext;config.Retain=retain;
#if defined(XL6_WS_TRANSFORM_DEFLATER)
    if(level<0||level>10||strategy<XDEFLATE_STRATEGY_DEFAULT||strategy>XDEFLATE_STRATEGY_FIXED){xrtSetErrorKind(XERR_RANGE);return NULL;}
    config.Level=(int32)level;config.Strategy=(xdeflatestrategy)strategy;
#endif
    xwsdeflatedirection direction={config.WindowBits,config.NoContextTakeover};
    if(!WS_N(ConfigApply)(&config,&direction))return NULL;
    WS_CONFIG* result=xrtMalloc(sizeof(*result));if(result)*result=config;return result;
}
void WS_CFG(Destroy)(void* raw){xrtFree(raw);}
void* WS_CFG(WithDirection)(void* raw,void* direction)
{
    WS_CONFIG config=*(WS_CONFIG*)raw;
    if(!WS_N(ConfigApply)(&config,direction))return NULL;
    WS_CONFIG* result=xrtMalloc(sizeof(*result));if(result)*result=config;return result;
}
uint64_t WS_CFG(OutputLimit)(void* raw){return ((WS_CONFIG*)raw)->OutputLimit;}
int64_t WS_CFG(WindowBits)(void* raw){return ((WS_CONFIG*)raw)->WindowBits;}
bool WS_CFG(NoContextTakeover)(void* raw){return ((WS_CONFIG*)raw)->NoContextTakeover;}
bool WS_CFG(Retain)(void* raw){return ((WS_CONFIG*)raw)->Retain;}
#if defined(XL6_WS_TRANSFORM_DEFLATER)
int64_t WS_CFG(Level)(void* raw){return ((WS_CONFIG*)raw)->Level;}
int64_t WS_CFG(Strategy)(void* raw){return ((WS_CONFIG*)raw)->Strategy;}
#endif
void* WS_ENTRY(Create)(void* config)
{
    WS_TYPE* value=WS_N(Create)(config);if(!value)return NULL;
    Transform* result=xrtCalloc(1,sizeof(*result));
    if(!result){WS_N(Destroy)(value);return NULL;}result->value=value;return result;
}
static WS_CONFIG languageDefaults(void)
{
    WS_CONFIG config;WS_N(ConfigInit)(&config);
    config.OutputLimit=XL6_WS_DEFAULT_OUTPUT_LIMIT;return config;
}
void* WS_ENTRY(CreateDefault)(void){WS_CONFIG config=languageDefaults();return WS_ENTRY(Create)(&config);}
void WS_ENTRY(Destroy)(void* raw)
{Transform* self=raw;if(self){WS_N(Destroy)(self->value);xrtFree(self);}}
void WS_ENTRY(Begin)(void* raw,bool compressed)
{
    Transform* self=raw;XL6_WsGuard guard={0};
    if(!xl6WsEnter(&self->gate,&guard))return;
    (void)xl6WsChecked(WS_N(Begin)(self->value,compressed));xl6WsLeave(&guard);
}
static bool output(xbytesview data,ptr raw){return xl6WsAppend(raw,data);}
int64_t WS_ENTRY(WriteIntoSpan)(void* raw,xbytesview input,void* target,int64_t size)
{
    Transform* self=raw;XL6_WsGuard guard={0};xbuffer* out;
    if(!xl6BufferSelect(&input,size)||!xl6WsOutput(target,input,&out)||!xl6WsEnter(&self->gate,&guard))return 0;
    size_t before=out->Size;bool ok=xl6WsChecked(WS_N(Write)(self->value,input,output,out));
    int64_t written=ok?(int64_t)(out->Size-before):0;xl6WsLeave(&guard);return written;
}
int64_t WS_ENTRY(WriteIntoText)(void* raw,xstrview input,void* target,int64_t size)
{return WS_ENTRY(WriteIntoSpan)(raw,xl6WsAsBytes(input),target,size);}
int64_t WS_ENTRY(WriteIntoBytes)(void* raw,xvalue* input,void* target,int64_t size)
{xbytesview view;return xl6BufferBytesInput(input,size,&view)?WS_ENTRY(WriteIntoSpan)(raw,view,target,-1):0;}
int64_t WS_ENTRY(WriteIntoBuffer)(void* raw,void* input,void* target,int64_t size)
{xbytesview view;return xl6BufferInput(input,size,&view)?WS_ENTRY(WriteIntoSpan)(raw,view,target,-1):0;}
int64_t WS_ENTRY(EndInto)(void* raw,void* target)
{
    Transform* self=raw;XL6_WsGuard guard={0};xbuffer* out=xl6BufferChecked(target);
    if(!out||!xl6WsEnter(&self->gate,&guard))return 0;
    size_t before=out->Size;bool ok=xl6WsChecked(WS_N(End)(self->value,output,out));
    int64_t written=ok?(int64_t)(out->Size-before):0;xl6WsLeave(&guard);return written;
}
void WS_ENTRY(Reset)(void* raw,void* config)
{
    Transform* self=raw;XL6_WsGuard guard={0};if(!xl6WsEnter(&self->gate,&guard))return;
    (void)xl6WsChecked(WS_N(Reset)(self->value,config));xl6WsLeave(&guard);
}
void WS_ENTRY(ResetDefault)(void* raw){WS_CONFIG config=languageDefaults();WS_ENTRY(Reset)(raw,&config);}
uint64_t WS_ENTRY(OutputSize)(void* raw)
{
    Transform* self=raw;XL6_WsGuard guard={0};if(!xl6WsEnter(&self->gate,&guard))return 0;
    uint64_t size=WS_N(Size)(self->value);xl6WsLeave(&guard);return size;
}
#if defined(XL6_WS_TRANSFORM_DEFLATER)
void WS_ENTRY(Abort)(void* raw)
{Transform* self=raw;XL6_WsGuard guard={0};if(!xl6WsEnter(&self->gate,&guard))return;(void)xl6WsChecked(WS_N(Abort)(self->value));xl6WsLeave(&guard);}
int64_t WS_ENTRY(FlushInto)(void* raw,void* target)
{
    Transform* self=raw;XL6_WsGuard guard={0};xbuffer* out=xl6BufferChecked(target);
    if(!out||!xl6WsEnter(&self->gate,&guard))return 0;
    size_t before=out->Size;bool ok=xl6WsChecked(WS_N(Flush)(self->value,output,out));
    int64_t written=ok?(int64_t)(out->Size-before):0;xl6WsLeave(&guard);return written;
}
int64_t WS_ENTRY(Bound)(int64_t input)
{size_t n,size=0;if(!xl6WsSize(input,&n)||!xrtWsDeflaterBound(n,&size))return 0;if(size>INT64_MAX){xrtSetErrorKind(XERR_RANGE);return 0;}return (int64_t)size;}
#endif
#define WS_POLICY_SYMBOL_INNER(Type) xl6##Type##HandlePolicyV2Get
#define WS_POLICY_SYMBOL(Type) WS_POLICY_SYMBOL_INNER(Type)
#define WS_DEFINE_POLICY(Getter,Drop) XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(Getter,Drop)
WS_DEFINE_POLICY(WS_POLICY_SYMBOL(WS_CFG_NAME),WS_CFG(Destroy))
WS_DEFINE_POLICY(WS_POLICY_SYMBOL(WS_NAME),WS_ENTRY(Destroy))
