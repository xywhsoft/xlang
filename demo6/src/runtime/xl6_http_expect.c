#include "xl6_http_internal.h"
typedef struct XL6_HttpExpectations {size_t count;xhttpexpectresult result;xhttpexpectation items[];} XL6_HttpExpectations;
static void* make(const xhttpfield* fields,size_t count)
{
    xhttpexpectresult result=xrtHttpExpectFields(fields,count);if(result==XHTTP_EXPECT_ERROR)return NULL;
    size_t number=0,total=sizeof(XL6_HttpExpectations);xhttpexpectfieldcursor cursor;xhttpexpectation item;xhttpnext next;
    xrtHttpExpectFieldCursorInit(&cursor);
    while((next=xrtHttpExpectFieldNext(fields,count,&cursor,&item))==XHTTP_NEXT_ITEM){
        if(!xl6HttpAccumulate(&total,sizeof(item))||!xl6HttpAccumulate(&total,item.Element.Size))return NULL;
        ++number;
    }
    if(next==XHTTP_NEXT_ERROR)return NULL;
    XL6_HttpExpectations* self=xrtMalloc(total);if(!self)return NULL;self->count=number;self->result=result;
    char* data=(char*)(self->items+number);xrtHttpExpectFieldCursorInit(&cursor);
    for(size_t i=0;i<number;++i){if(xrtHttpExpectFieldNext(fields,count,&cursor,&item)!=XHTTP_NEXT_ITEM){xrtFree(self);return NULL;}
        xstrview copy=xl6HttpStoreView(item.Element,&data);self->items[i]=item;self->items[i].Element=copy;
        self->items[i].Name=xl6HttpRebase(item.Name,item.Element.Data,copy.Data);
        self->items[i].Value=xl6HttpRebase(item.Value,item.Element.Data,copy.Data);
        self->items[i].Parameters=xl6HttpRebase(item.Parameters,item.Element.Data,copy.Data);
    }return self;
}
void* xl6HttpExpectationsParseV2(xstrview text,int64_t size)
{if(!xl6TextSelectRaw(&text,size))return NULL;xhttpfield field={{"Expect",6},text};return make(&field,1);}
void* xl6HttpExpectationsFromHeadersV2(void* raw)
{XL6_HttpGuard g={0};const xhttpfield* fields;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&fields,&n))return NULL;void* result=make(fields,n);xl6HttpLeave(&g);return result;}
void xl6HttpExpectationsDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6HttpExpectationsLengthV2(void* raw){return (int64_t)((XL6_HttpExpectations*)raw)->count;}
int64_t xl6HttpExpectationsResultV2(void* raw){return ((XL6_HttpExpectations*)raw)->result;}
static const xhttpexpectation* at(void* raw,int64_t index)
{XL6_HttpExpectations* self=raw;return xl6HttpIndex(index,self->count)?self->items+index:NULL;}
xstrview xl6HttpExpectationsNameAtV2(void* raw,int64_t index)
{const xhttpexpectation* item=at(raw,index);return item?xl6TextCopy(item->Name):xl6TextFailure;}
xvalue* xl6HttpExpectationsValueAtV2(void* raw,int64_t index)
{const xhttpexpectation* item=at(raw,index);if(!item)return NULL;
 if(!(item->Flags&XHTTP_EXPECT_HAS_VALUE))return NULL;
 xhttpparam param={item->Name,item->Value,item->Flags&(XHTTP_PARAM_HAS_VALUE|XHTTP_PARAM_QUOTED)};
 /* Expectation.Value includes quotation marks; Param.Value is their body.
  * This is a record-view projection. XRT still validates/decodes escapes. */
 if(item->Flags&XHTTP_EXPECT_VALUE_QUOTED){param.Value.Data++;param.Value.Size-=2;}
 size_t size=0;if(!xrtHttpParamValueWrite(&param,NULL,0,&size))return NULL;uint8* bytes=xrtMalloc(size?size:1);if(!bytes)return NULL;
 xvalue* value=xrtHttpParamValueWrite(&param,bytes,size,&size)?xl6HttpBytes((xbytesview){bytes,size}):NULL;xrtFree(bytes);return value;}
void* xl6HttpExpectationsParametersAtV2(void* raw,int64_t index)
{const xhttpexpectation* item=at(raw,index);return item?xl6HttpParametersFragment(item->Parameters):NULL;}
int64_t xl6HttpExpectationsFlagsAtV2(void* raw,int64_t index)
{const xhttpexpectation* item=at(raw,index);return item?item->Flags:0;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpExpectationsHandlePolicyV2Get,xl6HttpExpectationsDestroyV2)
XL6_HTTP_BYTE_PARSERS(xl6HttpExpectationsParse,make(&(xhttpfield){{"Expect",6},view},1))
int64_t xl6HttpHeadersExpectationsV2(void* raw)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return 0;
 xhttpexpectresult result=xrtHttpExpectFields(f,n);xl6HttpLeave(&g);return result==XHTTP_EXPECT_ERROR?0:(int64_t)result;}
