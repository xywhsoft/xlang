#include "xl6_http_internal.h"
static bool reserve(XL6_HttpHeaders* self,size_t capacity)
{
    if(capacity<=self->capacity)return true;
    if(capacity>SIZE_MAX/(sizeof(xhttpfield)+sizeof(XL6_HttpFieldOffset))){
        xrtSetErrorKind(XERR_RANGE);return false;
    }
    /* Prepare both arrays before publication. Failed reserve preserves contents. */
    XL6_HttpFieldOffset* offsets=xrtMalloc(capacity*(sizeof(*offsets)+sizeof(xhttpfield)));
    if(!offsets)return false;
    xhttpfield* fields=(xhttpfield*)(offsets+capacity);
    if(self->count)memcpy(offsets,self->offsets,self->count*sizeof(*offsets));
    xrtFree(self->offsets);self->offsets=offsets;self->fields=fields;
    self->capacity=capacity;self->dirty=true;return true;
}
static void views(XL6_HttpHeaders* self)
{
    if(!self->dirty)return;
    for(size_t i=0;i<self->count;++i){XL6_HttpFieldOffset* o=&self->offsets[i];
        self->fields[i]=(xhttpfield){{(const char*)self->storage.Data+o->name,o->nameSize},
            {(const char*)self->storage.Data+o->value,o->valueSize}};
    }
    self->dirty=false;
}
bool xl6HttpHeadersBorrow(XL6_HttpHeaders* self,XL6_HttpGuard* guard,const xhttpfield** fields,size_t* count)
{
    if(!self){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xl6HttpEnter(&self->gate,guard))return false;
    views(self);*fields=self->fields;*count=self->count;return true;
}
void xl6HttpHeadersDispose(XL6_HttpHeaders* self)
{if(self){xrtBufferUnit(&self->storage);xrtFree(self->offsets);xrtFree(self);}}
void xl6HttpHeadersDestroyV2(void* raw){xl6HttpHeadersDispose(raw);}
void* xl6HttpHeadersCreateV2(int64_t capacity)
{
    size_t count;if(!xl6HttpSize(capacity,&count))return NULL;
    XL6_HttpHeaders* self=xrtCalloc(1,sizeof(*self));
    if(self){self->parsedBytes=-1;if(!reserve(self,count)){xl6HttpHeadersDispose(self);self=NULL;}}return self;
}
static bool add(XL6_HttpHeaders* self,xstrview name,xstrview value)
{
    if(!xrtHttpTokenValid(name)||!xrtHttpFieldValueValid(value)){xrtSetErrorKind(XERR_PROTOCOL);return false;}
    if(name.Size>SIZE_MAX-value.Size||name.Size+value.Size>(size_t)INT64_MAX-self->storage.Size){
        xrtSetErrorKind(XERR_RANGE);return false;
    }
    if(self->count==self->capacity){size_t next=self->capacity?self->capacity*2:8;
        if(next<self->capacity||!reserve(self,next))return false;}
    size_t start=self->storage.Size,total=name.Size+value.Size;
    if(!xl6BufferAllocation(xrtBufferReserve(&self->storage,start+total)))return false;
    if(name.Size)memcpy(self->storage.Data+start,name.Data,name.Size);
    if(value.Size)memcpy(self->storage.Data+start+name.Size,value.Data,value.Size);
    self->offsets[self->count++]=(XL6_HttpFieldOffset){start,start+name.Size,name.Size,value.Size};
    self->storage.Size+=total;self->parsedBytes=-1;self->dirty=true;return true;
}
XL6_HttpHeaders* xl6HttpHeadersCopyFields(const xhttpfield* fields,size_t count)
{
    if(count>INT64_MAX){xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_HttpHeaders* self=xl6HttpHeadersCreateV2((int64_t)count);if(!self)return NULL;
    size_t total=0;
    for(size_t i=0;i<count;++i){if(fields[i].Name.Size>SIZE_MAX-total||fields[i].Value.Size>SIZE_MAX-total-fields[i].Name.Size){
        xrtSetErrorKind(XERR_RANGE);xl6HttpHeadersDispose(self);return NULL;}
        total+=fields[i].Name.Size+fields[i].Value.Size;}
    if(!xl6BufferAllocation(xrtBufferReserve(&self->storage,total))){xl6HttpHeadersDispose(self);return NULL;}
    for(size_t i=0;i<count;++i)if(!add(self,fields[i].Name,fields[i].Value)){xl6HttpHeadersDispose(self);return NULL;}
    return self;
}
static void removeAt(XL6_HttpHeaders* self,size_t index)
{
    XL6_HttpFieldOffset old=self->offsets[index];size_t size=old.nameSize+old.valueSize,end=old.name+size;
    memmove(self->storage.Data+old.name,self->storage.Data+end,self->storage.Size-end);self->storage.Size-=size;
    memmove(self->offsets+index,self->offsets+index+1,(self->count-index-1)*sizeof(*self->offsets));--self->count;
    for(size_t i=0;i<self->count;++i)if(self->offsets[i].name>=end){self->offsets[i].name-=size;self->offsets[i].value-=size;}
    self->parsedBytes=-1;self->dirty=true;
}
static size_t removeName(XL6_HttpHeaders* self,xstrview name,size_t before)
{
    size_t removed=0;
    for(size_t i=0;i<before;){XL6_HttpFieldOffset* o=&self->offsets[i];
        if(xrtHttpFieldNameEqual((xstrview){(const char*)self->storage.Data+o->name,o->nameSize},name)){
            removeAt(self,i);--before;++removed;
        }else ++i;
    }return removed;
}
static void mutate(void* raw,xstrview name,xstrview value,bool replace)
{
    XL6_HttpHeaders* self=raw;XL6_HttpGuard guard={0};if(!self||!xl6HttpEnter(&self->gate,&guard))return;
    if(add(self,name,value)&&replace)removeName(self,name,self->count-1);
    xl6HttpLeave(&guard);
}
void xl6HttpHeadersAddTextV2(void* self,xstrview name,xstrview text,int64_t size)
{if(xl6TextSelectRaw(&text,size))mutate(self,name,text,false);}
void xl6HttpHeadersSetTextV2(void* self,xstrview name,xstrview text,int64_t size)
{if(xl6TextSelectRaw(&text,size))mutate(self,name,text,true);}
void xl6HttpHeadersAddBytesV2(void* self,xstrview name,xvalue* data,int64_t size)
{xbytesview input;if(xl6BufferBytesInput(data,size,&input))mutate(self,name,xl6HttpAsText(input),false);}
void xl6HttpHeadersSetBytesV2(void* self,xstrview name,xvalue* data,int64_t size)
{xbytesview input;if(xl6BufferBytesInput(data,size,&input))mutate(self,name,xl6HttpAsText(input),true);}
void xl6HttpHeadersAddSpanV2(void* self,xstrview name,xbytesview data,int64_t size)
{if(xl6BufferSelect(&data,size))mutate(self,name,xl6HttpAsText(data),false);}
void xl6HttpHeadersSetSpanV2(void* self,xstrview name,xbytesview data,int64_t size)
{if(xl6BufferSelect(&data,size))mutate(self,name,xl6HttpAsText(data),true);}
int64_t xl6HttpHeadersRemoveV2(void* raw,xstrview name)
{
    XL6_HttpHeaders* self=raw;XL6_HttpGuard guard={0};if(!self||!xl6HttpEnter(&self->gate,&guard))return 0;
    size_t removed=removeName(self,name,self->count);xl6HttpLeave(&guard);return (int64_t)removed;
}
void xl6HttpHeadersReserveV2(void* raw,int64_t capacity)
{
    XL6_HttpHeaders* self=raw;XL6_HttpGuard guard={0};size_t count;
    if(!xl6HttpSize(capacity,&count)||!self||!xl6HttpEnter(&self->gate,&guard))return;
    reserve(self,count);xl6HttpLeave(&guard);
}
void xl6HttpHeadersClearV2(void* raw)
{XL6_HttpHeaders* self=raw;XL6_HttpGuard g={0};if(self&&xl6HttpEnter(&self->gate,&g)){self->storage.Size=0;self->count=0;self->parsedBytes=-1;self->dirty=true;xl6HttpLeave(&g);}}
int64_t xl6HttpHeadersLengthV2(void* raw)
{XL6_HttpHeaders* s=raw;XL6_HttpGuard g={0};if(!s||!xl6HttpEnter(&s->gate,&g))return 0;int64_t n=(int64_t)s->count;xl6HttpLeave(&g);return n;}
int64_t xl6HttpHeadersCapacityV2(void* raw)
{XL6_HttpHeaders* s=raw;XL6_HttpGuard g={0};if(!s||!xl6HttpEnter(&s->gate,&g))return 0;int64_t n=(int64_t)s->capacity;xl6HttpLeave(&g);return n;}
void* xl6HttpHeadersCloneV2(void* raw)
{XL6_HttpGuard g={0};const xhttpfield* fields;size_t count;if(!xl6HttpHeadersBorrow(raw,&g,&fields,&count))return NULL;
 XL6_HttpHeaders* copy=xl6HttpHeadersCopyFields(fields,count);if(copy)copy->parsedBytes=((XL6_HttpHeaders*)raw)->parsedBytes;xl6HttpLeave(&g);return copy;}
static XL6_HttpHeaders* parse(xstrview block)
{
    size_t parsedBytes=block.Size;
    /* FieldBlockWrite includes the terminal empty line; FieldNext takes
     * field lines only. Accept either native representation without treating
     * additional empty lines or trailing message bytes as fields. */
    if(block.Size>=2&&block.Data[block.Size-2]=='\r'&&block.Data[block.Size-1]=='\n'&&
       (block.Size==2||(block.Size>=4&&block.Data[block.Size-4]=='\r'&&block.Data[block.Size-3]=='\n')))
        block.Size-=2;
    size_t count=0;if(!xrtHttpFieldBlockCount(block,&count))return NULL;
    if(count>SIZE_MAX/sizeof(xhttpfield)){xrtSetErrorKind(XERR_RANGE);return NULL;}
    xhttpfield* fields=count?xrtMalloc(count*sizeof(*fields)):NULL;if(count&&!fields)return NULL;
    size_t offset=0,index=0;xhttpnext state;xhttpfield field;
    while((state=xrtHttpFieldNext(block,&offset,&field))==XHTTP_NEXT_ITEM){
        if(index==count){state=XHTTP_NEXT_ERROR;xrtSetErrorKind(XERR_INTERNAL);break;}
        fields[index++]=field;
    }
    XL6_HttpHeaders* result=state==XHTTP_NEXT_END?xl6HttpHeadersCopyFields(fields,count):NULL;
    if(result)result->parsedBytes=(int64_t)parsedBytes;
    xrtFree(fields);return result;
}
void* xl6HttpHeadersParseTextV2(xstrview text,int64_t size)
{return xl6TextSelectRaw(&text,size)?parse(text):NULL;}
void* xl6HttpHeadersParseBytesV2(xvalue* data,int64_t size)
{xbytesview view;return xl6BufferBytesInput(data,size,&view)?parse(xl6HttpAsText(view)):NULL;}
void* xl6HttpHeadersParseSpanV2(xbytesview data,int64_t size)
{return xl6BufferSelect(&data,size)?parse(xl6HttpAsText(data)):NULL;}
void* xl6HttpHeadersParseBufferV2(void* data,int64_t size)
{xbytesview view;return xl6BufferInput(data,size,&view)?parse(xl6HttpAsText(view)):NULL;}
int64_t xl6HttpHeadersFindV2(void* raw,xstrview name,int64_t start)
{
    XL6_HttpGuard g={0};const xhttpfield* f;size_t n,index=0;if(!xl6HttpSize(start,&index)||!xl6HttpHeadersBorrow(raw,&g,&f,&n))return -1;
    index=xrtHttpFieldFind(f,n,name,index);xl6HttpLeave(&g);return index==XRT_NPOS?-1:(int64_t)index;
}
int64_t xl6HttpHeadersCountV2(void* raw,xstrview name)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return 0;size_t count=xrtHttpFieldCount(f,n,name);xl6HttpLeave(&g);return (int64_t)count;}
bool xl6HttpHeadersContainsV2(void* raw,xstrview name){return xl6HttpHeadersFindV2(raw,name,0)>=0;}
xstrview xl6HttpHeadersNameAtV2(void* raw,int64_t index)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return xl6TextFailure;
 xstrview result=xl6HttpIndex(index,n)?xl6TextCopy(f[index].Name):xl6TextFailure;xl6HttpLeave(&g);return result;}
xvalue* xl6HttpHeadersValueAtV2(void* raw,int64_t index)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return NULL;
 xvalue* result=xl6HttpIndex(index,n)?xl6HttpBytes(xl6HttpAsBytes(f[index].Value)):NULL;xl6HttpLeave(&g);return result;}
xstrview xl6HttpHeadersTextAtV2(void* raw,int64_t index)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return xl6TextFailure;
 xstrview result=xl6HttpIndex(index,n)?xl6HttpText(f[index].Value):xl6TextFailure;xl6HttpLeave(&g);return result;}
void xl6HttpHeadersCopyValueIntoV2(void* raw,int64_t index,void* target)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;xbuffer* output=xl6BufferChecked(target);if(!output||!xl6HttpHeadersBorrow(raw,&g,&f,&n))return;
 if(xl6HttpIndex(index,n))xl6HttpAppend(output,xl6HttpAsBytes(f[index].Value));
 xl6HttpLeave(&g);}
xvalue* xl6HttpHeadersGetBytesV2(void* raw,xstrview name)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return NULL;
 const xhttpfield* found=xrtHttpFieldGet(f,n,name);xvalue* result=found?xl6HttpBytes(xl6HttpAsBytes(found->Value)):NULL;xl6HttpLeave(&g);return result;}
xstrview xl6HttpHeadersGetTextV2(void* raw,xstrview name)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return xl6TextFailure;
 const xhttpfield* found=xrtHttpFieldGet(f,n,name);xstrview result=found?xl6HttpText(found->Value):xl6TextFailure;xl6HttpLeave(&g);return result;}
xvalue* xl6HttpHeadersGetUniqueBytesV2(void* raw,xstrview name)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return NULL;
 const xhttpfield* found=NULL;xhttpnext next=xrtHttpFieldGetUnique(f,n,name,&found);
 xvalue* result=next==XHTTP_NEXT_ITEM?xl6HttpBytes(xl6HttpAsBytes(found->Value)):NULL;xl6HttpLeave(&g);return result;}
int64_t xl6HttpHeadersWriteIntoV2(void* raw,void* target)
{
    XL6_HttpGuard g={0};const xhttpfield* f;size_t n,required=0;xbuffer* out=xl6BufferChecked(target);
    if(!out||!xl6HttpHeadersBorrow(raw,&g,&f,&n))return 0;
    bool ok=xrtHttpFieldBlockWrite(f,n,NULL,0,&required);
    if(ok&&required>(size_t)INT64_MAX-out->Size){xrtSetErrorKind(XERR_RANGE);ok=false;}
    if(ok)ok=xl6BufferAllocation(xrtBufferReserve(out,out->Size+required));
    if(ok)ok=xrtHttpFieldBlockWrite(f,n,out->Data+out->Size,required,&required);
    if(ok)out->Size+=required;
    xl6HttpLeave(&g);return ok?(int64_t)required:0;
}
xvalue* xl6HttpHeadersToBytesV2(void* raw)
{xbuffer out={0};xl6HttpHeadersWriteIntoV2(raw,&out);xvalue* value=xrtGetError()?NULL:xl6HttpTakeBuffer(&out);xrtBufferUnit(&out);return value;}
int64_t xl6HttpHeadersParsedBytesV2(void* raw)
{XL6_HttpHeaders* self=raw;XL6_HttpGuard g={0};if(!xl6HttpEnter(&self->gate,&g))return -1;int64_t value=self->parsedBytes;xl6HttpLeave(&g);return value;}
void* xl6HttpHeadersSelectV2(void* raw,xstrview name)
{
    XL6_HttpGuard g={0};const xhttpfield* fields;size_t count;if(!xl6HttpHeadersBorrow(raw,&g,&fields,&count))return NULL;
    size_t selected=xrtHttpFieldCount(fields,count,name);XL6_HttpHeaders* result=xl6HttpHeadersCreateV2((int64_t)selected);
    for(size_t i=0;result&&i<count;++i)if(xrtHttpFieldNameEqual(fields[i].Name,name)&&!add(result,fields[i].Name,fields[i].Value)){
        xl6HttpHeadersDispose(result);result=NULL;
    }
    xl6HttpLeave(&g);return result;
}
int64_t xl6HttpHeadersTokenCountV2(void* raw,xstrview name)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n,count=0;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return 0;
 bool ok=xrtHttpFieldTokenCount(f,n,name,&count);xl6HttpLeave(&g);return ok?(int64_t)count:0;}
bool xl6HttpHeadersHasTokenV2(void* raw,xstrview name,xstrview token)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return false;
 xhttpnext found=xrtHttpFieldTokenFind(f,n,name,token);xl6HttpLeave(&g);return found==XHTTP_NEXT_ITEM;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpHeadersHandlePolicyV2Get,xl6HttpHeadersDestroyV2)
