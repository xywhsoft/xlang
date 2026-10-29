#include "xl6_ws_internal.h"
typedef struct Extensions {size_t count;xwsextension items[];} Extensions;
typedef struct Extension {xwsextension value;size_t count;xhttpparam params[];} Extension;

static void* parse(xstrview input)
{
    size_t count=0;
    if(!xrtWsExtensionCount(input,&count))return NULL;
    if(count>(SIZE_MAX-sizeof(Extensions))/sizeof(xwsextension)||
       input.Size>SIZE_MAX-sizeof(Extensions)-count*sizeof(xwsextension)){
        xrtSetErrorKind(XERR_RANGE);return NULL;
    }
    Extensions* result=xrtMalloc(sizeof(*result)+count*sizeof(xwsextension)+input.Size);
    if(!result)return NULL;
    result->count=count;char* data=(char*)(result->items+count);
    if(input.Size)memcpy(data,input.Data,input.Size);
    xstrview copy={data,input.Size};size_t offset=0;
    for(size_t i=0;i<count;++i){
        if(xrtWsExtensionNext(copy,&offset,result->items+i)!=XHTTP_NEXT_ITEM){xrtFree(result);return NULL;}
    }
    return result;
}
void* xl6WsExtensionsParseTextV2(xstrview input,int64_t size)
{return xl6TextSelectRaw(&input,size)?parse(input):NULL;}
void* xl6WsExtensionsParseSpanV2(xbytesview input,int64_t size)
{return xl6BufferSelect(&input,size)?parse(xl6WsAsText(input)):NULL;}
void* xl6WsExtensionsParseBytesV2(xvalue* input,int64_t size)
{xbytesview view;return xl6BufferBytesInput(input,size,&view)?parse(xl6WsAsText(view)):NULL;}
void* xl6WsExtensionsParseBufferV2(void* input,int64_t size)
{xbytesview view;return xl6BufferInput(input,size,&view)?parse(xl6WsAsText(view)):NULL;}
void xl6WsExtensionsDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6WsExtensionsLengthV2(void* raw){return (int64_t)((Extensions*)raw)->count;}

static Extension* copyExtension(const xwsextension* input)
{
    size_t count=0,offset=0;xhttpparam param;xhttpnext status;
    while((status=xrtWsExtensionParamNext(input,&offset,&param))==XHTTP_NEXT_ITEM)++count;
    if(status==XHTTP_NEXT_ERROR)return NULL;
    size_t total=sizeof(Extension);
    if(count>(SIZE_MAX-total)/sizeof(xhttpparam))goto range;
    total+=count*sizeof(xhttpparam);
    if(input->Name.Size>SIZE_MAX-total)goto range;
    total+=input->Name.Size;
    if(input->Parameters.Size>SIZE_MAX-total)goto range;
    total+=input->Parameters.Size;
    Extension* result=xrtMalloc(total);if(!result)return NULL;
    result->count=count;char* data=(char*)(result->params+count);
    result->value.Name=(xstrview){data,input->Name.Size};
    if(input->Name.Size)memcpy(data,input->Name.Data,input->Name.Size);
    data+=input->Name.Size;result->value.Parameters=(xstrview){data,input->Parameters.Size};
    if(input->Parameters.Size)memcpy(data,input->Parameters.Data,input->Parameters.Size);
    offset=0;
    for(size_t i=0;i<count;++i){
        if(xrtWsExtensionParamNext(&result->value,&offset,result->params+i)!=XHTTP_NEXT_ITEM){xrtFree(result);return NULL;}
    }
    return result;
range:
    xrtSetErrorKind(XERR_RANGE);return NULL;
}
void* xl6WsExtensionsAtV2(void* raw,int64_t index)
{Extensions* s=raw;return xl6WsIndex(index,s->count)?copyExtension(s->items+index):NULL;}
void* xl6WsExtensionMakeV2(xstrview name,xstrview parameters)
{size_t size=0;if(!xrtWsExtensionWrite(name,parameters,NULL,0,&size))return NULL;xwsextension value={name,parameters};return copyExtension(&value);}
void xl6WsExtensionDestroyV2(void* raw){xrtFree(raw);}
xstrview xl6WsExtensionNameV2(void* raw){return xl6TextCopy(((Extension*)raw)->value.Name);}
xstrview xl6WsExtensionParametersV2(void* raw){return xl6TextCopy(((Extension*)raw)->value.Parameters);}
int64_t xl6WsExtensionParameterCountV2(void* raw){return (int64_t)((Extension*)raw)->count;}
static const xhttpparam* at(Extension* self,int64_t index)
{return xl6WsIndex(index,self->count)?self->params+index:NULL;}
xstrview xl6WsExtensionParameterNameAtV2(void* raw,int64_t index)
{const xhttpparam* value=at(raw,index);return value?xl6TextCopy(value->Name):xl6TextFailure;}
int64_t xl6WsExtensionParameterFlagsAtV2(void* raw,int64_t index)
{const xhttpparam* value=at(raw,index);return value?value->Flags:0;}
xvalue* xl6WsExtensionParameterValueAtV2(void* raw,int64_t index)
{
    const xhttpparam* value=at(raw,index);size_t size=0;
    if(!value||!(value->Flags&XHTTP_PARAM_HAS_VALUE)||!xrtHttpParamValueWrite(value,NULL,0,&size))return NULL;
    uint8* data=xrtMalloc(size?size:1);if(!data)return NULL;
    xvalue* result=xrtHttpParamValueWrite(value,data,size,&size)?xrtValueBytesTake(&data,size):NULL;
    xrtFree(data);return result;
}
int64_t xl6WsExtensionWriteIntoV2(void* raw,void* output)
{
    Extension* s=raw;xbuffer* out=xl6BufferChecked(output);size_t size=0;
    if(!out||!xrtWsExtensionWrite(s->value.Name,s->value.Parameters,NULL,0,&size))return 0;
    if(size>(size_t)INT64_MAX-out->Size){xrtSetErrorKind(XERR_RANGE);return 0;}
    if(!xl6BufferAllocation(xrtBufferReserve(out,out->Size+size))||
       !xrtWsExtensionWrite(s->value.Name,s->value.Parameters,out->Data?out->Data+out->Size:NULL,size,&size))return 0;
    out->Size+=size;return (int64_t)size;
}
xstrview xl6WsExtensionToStringV2(void* raw)
{
    Extension* s=raw;size_t size=0;
    if(!xrtWsExtensionWrite(s->value.Name,s->value.Parameters,NULL,0,&size))return xl6TextFailure;
    if(size>INT64_MAX||size==SIZE_MAX){xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;}
    char* text=xrtMalloc(size+1);if(!text)return xl6TextFailure;
    if(!xrtWsExtensionWrite(s->value.Name,s->value.Parameters,text,size,&size)){xrtFree(text);return xl6TextFailure;}
    text[size]='\0';return xl6TextOwn(text,size);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsExtensionsHandlePolicyV2Get,xl6WsExtensionsDestroyV2)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsExtensionHandlePolicyV2Get,xl6WsExtensionDestroyV2)
