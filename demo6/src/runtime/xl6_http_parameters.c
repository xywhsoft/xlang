#include "xl6_http_internal.h"
typedef struct XL6_HttpParameters {size_t count;xhttpparam items[];} XL6_HttpParameters;
typedef xhttpnext (*XL6_HttpParamNext)(xstrview,size_t*,xhttpparam*);
static void* parse(xstrview text,XL6_HttpParamNext next,char separator,bool empty)
{
    size_t count=0,offset=0;xhttpparam item;xhttpnext state;
    while((state=next?next(text,&offset,&item):xrtHttpNameValueNext(text,&offset,&item,separator,empty))==XHTTP_NEXT_ITEM)++count;
    if(state==XHTTP_NEXT_ERROR)return NULL;
    if(count>(SIZE_MAX-sizeof(XL6_HttpParameters))/sizeof(xhttpparam)||text.Size>SIZE_MAX-sizeof(XL6_HttpParameters)-count*sizeof(xhttpparam)){
        xrtSetErrorKind(XERR_RANGE);return NULL;
    }
    XL6_HttpParameters* self=xrtMalloc(sizeof(*self)+count*sizeof(xhttpparam)+text.Size);if(!self)return NULL;
    self->count=count;char* copy=(char*)(self->items+count);if(text.Size)memcpy(copy,text.Data,text.Size);
    xstrview source={copy,text.Size};offset=0;
    for(size_t i=0;i<count;++i)if((next?next(source,&offset,self->items+i):xrtHttpNameValueNext(source,&offset,self->items+i,separator,empty))!=XHTTP_NEXT_ITEM){xrtFree(self);return NULL;}
    return self;
}
void* xl6HttpParametersParseV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)?parse(text,xrtHttpParamNext,0,false):NULL;}
void* xl6HttpParametersDirectivesV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)?parse(text,xrtHttpDirectiveNext,0,false):NULL;}
void* xl6HttpParametersFragment(xstrview text)
{return parse(text,NULL,';',true);}
void* xl6HttpParametersNameValuesV2(xstrview text,int64_t separator,bool empty,int64_t size)
{if(separator<0||separator>127){xrtSetErrorKind(XERR_RANGE);return NULL;}return xl6TextSelectRaw(&text,size)?parse(text,NULL,(char)separator,empty):NULL;}
void xl6HttpParametersDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6HttpParametersLengthV2(void* raw){return (int64_t)((XL6_HttpParameters*)raw)->count;}
static const xhttpparam* at(void* raw,int64_t index)
{XL6_HttpParameters* self=raw;return xl6HttpIndex(index,self->count)?self->items+index:NULL;}
xstrview xl6HttpParametersNameAtV2(void* raw,int64_t index)
{const xhttpparam* item=at(raw,index);return item?xl6TextCopy(item->Name):xl6TextFailure;}
xvalue* xl6HttpParametersRawValueAtV2(void* raw,int64_t index)
{const xhttpparam* item=at(raw,index);return item&&(item->Flags&XHTTP_PARAM_HAS_VALUE)?xl6HttpBytes(xl6HttpAsBytes(item->Value)):NULL;}
int64_t xl6HttpParametersFlagsAtV2(void* raw,int64_t index)
{const xhttpparam* item=at(raw,index);return item?item->Flags:0;}
bool xl6HttpParametersTokenValidAtV2(void* raw,int64_t index)
{const xhttpparam* item=at(raw,index);return item&&xrtHttpParamTokenValid(item);}
bool xl6HttpParametersTokenEqualAtV2(void* raw,int64_t index,xstrview token)
{const xhttpparam* item=at(raw,index);return item&&xrtHttpParamTokenEqual(item,token);}
bool xl6HttpParametersHostValidAtV2(void* raw,int64_t index)
{const xhttpparam* item=at(raw,index);return item&&xrtHttpParamHostValid(item);}
int64_t xl6HttpParametersFindV2(void* raw,xstrview name)
{XL6_HttpParameters* self=raw;for(size_t i=0;i<self->count;++i)if(xrtHttpTokenEqual(self->items[i].Name,name))return (int64_t)i;return -1;}
int64_t xl6HttpParametersCopyValueIntoV2(void* raw,int64_t index,void* target)
{
    const xhttpparam* item=at(raw,index);xbuffer* output=xl6BufferChecked(target);size_t size=0;
    if(!item||!output)return 0;
    if(!(item->Flags&XHTTP_PARAM_HAS_VALUE)){xrtSetErrorKind(XERR_STATE);return 0;}
    if(!xrtHttpParamValueWrite(item,NULL,0,&size))return 0;
    if(size>(size_t)INT64_MAX-output->Size){xrtSetErrorKind(XERR_RANGE);return 0;}
    if(!xl6BufferAllocation(xrtBufferReserve(output,output->Size+size))||!xrtHttpParamValueWrite(item,output->Data?output->Data+output->Size:NULL,size,&size))return 0;
    output->Size+=size;return (int64_t)size;
}
xvalue* xl6HttpParametersValueAtV2(void* raw,int64_t index)
{const xhttpparam* item=at(raw,index);if(!item||!(item->Flags&XHTTP_PARAM_HAS_VALUE))return NULL;
 xbuffer output={0};xl6HttpParametersCopyValueIntoV2(raw,index,&output);xvalue* result=xrtGetError()?NULL:xl6HttpTakeBuffer(&output);xrtBufferUnit(&output);return result;}
bool xl6HttpParametersQuotedValidV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)&&xrtHttpQuotedValid(text);}
xstrview xl6HttpParametersQuoteV2(xstrview text,int64_t size)
{if(!xl6TextSelect(&text,size))return xl6TextFailure;size_t count=0;char* data=xrtHttpQuotedBuild(text,&count);return xl6TextOwn(data,count);}
xvalue* xl6HttpParametersUnquoteV2(xstrview text,int64_t size)
{
    if(!xl6TextSelectRaw(&text,size))return NULL;
    size_t count=0;
    if(!xrtHttpQuotedRead(text,NULL,0,&count))return NULL;
    uint8* data=xrtMalloc(count?count:1);if(!data)return NULL;
    xvalue* result=xrtHttpQuotedRead(text,data,count,&count)?xrtValueBytesTake(&data,count):NULL;
    xrtFree(data);return result;
}
xstrview xl6HttpParametersBuildV2(xstrview name,xstrview value,int64_t flags,int64_t size)
{
    if(flags<0||flags>3){xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;}
    if(!xl6TextSelect(&value,size))return xl6TextFailure;
    size_t count=0;char* data=xrtHttpParamBuild(name,value,(uint32)flags,&count);return xl6TextOwn(data,count);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpParametersHandlePolicyV2Get,xl6HttpParametersDestroyV2)
XL6_HTTP_BYTE_PARSERS(xl6HttpParametersParse,parse(view,xrtHttpParamNext,0,false))
XL6_HTTP_BYTE_PARSERS(xl6HttpParametersDirectives,parse(view,xrtHttpDirectiveNext,0,false))
static xvalue* quoted(xstrview view)
{size_t size=0;uint8* data=(uint8*)xrtHttpQuotedBuild(view,&size);if(!data)return NULL;
 xvalue* value=xrtValueBytesTake(&data,size);xrtFree(data);return value;}
xvalue* xl6HttpParametersQuoteSpanV2(xbytesview input,int64_t size)
{return xl6BufferSelect(&input,size)?quoted(xl6HttpAsText(input)):NULL;}
xvalue* xl6HttpParametersQuoteBytesV2(xvalue* source,int64_t size)
{xbytesview input;return xl6BufferBytesInput(source,size,&input)?quoted(xl6HttpAsText(input)):NULL;}
xvalue* xl6HttpParametersUnquoteSpanV2(xbytesview input,int64_t size)
{return xl6BufferSelect(&input,size)?xl6HttpParametersUnquoteV2(xl6HttpAsText(input),-1):NULL;}
xvalue* xl6HttpParametersUnquoteBytesV2(xvalue* source,int64_t size)
{xbytesview input;return xl6BufferBytesInput(source,size,&input)?xl6HttpParametersUnquoteV2(xl6HttpAsText(input),-1):NULL;}
bool xl6HttpParametersQuotedValidSpanV2(xbytesview input,int64_t size)
{return xl6BufferSelect(&input,size)&&xrtHttpQuotedValid(xl6HttpAsText(input));}
bool xl6HttpParametersQuotedValidBytesV2(xvalue* source,int64_t size)
{xbytesview input;return xl6BufferBytesInput(source,size,&input)&&xrtHttpQuotedValid(xl6HttpAsText(input));}
static xvalue* build(xstrview name,xstrview value,int64_t flags)
{
    if(flags<0||flags>3){xrtSetErrorKind(XERR_RANGE);return NULL;}
    size_t size=0;uint8* data=(uint8*)xrtHttpParamBuild(name,value,(uint32)flags,&size);if(!data)return NULL;
    xvalue* result=xrtValueBytesTake(&data,size);xrtFree(data);return result;
}
xvalue* xl6HttpParametersBuildSpanV2(xstrview name,xbytesview value,int64_t flags,int64_t size)
{return xl6BufferSelect(&value,size)?build(name,xl6HttpAsText(value),flags):NULL;}
xvalue* xl6HttpParametersBuildBytesV2(xstrview name,xvalue* source,int64_t flags,int64_t size)
{xbytesview value;return xl6BufferBytesInput(source,size,&value)?build(name,xl6HttpAsText(value),flags):NULL;}
