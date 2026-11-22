#include "analysis/ir_call_link.h"
#include <xrt.h>
#include <string.h>
#include <stdint.h>

struct xls_ir_call_link {
 XL6_IrModule tables;size_t bytes,peak,retained; xls_ir_table_unit** units;
};
typedef struct call_state {
 const XL6_IrModule* current;const xls_ir_call_binding* binding;
 size_t index,limits[XLS_CALL_DOMAIN_COUNT];xls_ir_call_error* error;
 int(*cancelled)(void*);void* user;int cancelObserved;
} call_state;
typedef struct call_descriptor {
 const char* name;size_t width,alignment;
 const void*(*read)(const XL6_IrModule*,size_t*,size_t*);
 void(*set)(XL6_IrModule*,void*,size_t);
 int(*relocate)(call_state*,void*,const void*);
} call_descriptor;
typedef struct placement {size_t table,row;} placement;
static int add(size_t* n,size_t k){if(k>SIZE_MAX-*n)return 0;*n+=k;return 1;}
static int region(size_t* n,size_t count,size_t width,size_t alignment,size_t* at)
{if(!add(n,(alignment-*n%alignment)%alignment)||count>SIZE_MAX/width)return 0;*at=*n;return add(n,count*width);}
static int fail(call_state* s,xls_ir_call_reason reason,const char* f,xls_ir_call_domain d,uint64_t v)
{if(s->error)*s->error=(xls_ir_call_error){reason,s->index,f,d,v};return 0;}
static int stopped(call_state* s){if(s->cancelObserved)return 1;if(s->cancelled && s->cancelled(s->user))s->cancelObserved=1;return s->cancelObserved;}
static int mapOne(call_state* s,xls_ir_call_domain d,const char* field,uint64_t value,uint64_t* out)
{
 int indexed=d<XLS_CALL_NODE;
 if(!indexed && !value){*out=0;return 1;}
 if(!s->binding->map(s->binding->user,d,value,out))return fail(s,XLS_CALL_UNMAPPED,field,d,value);
 if((indexed ? *out>=s->limits[d] : !*out || *out>s->limits[d]))return fail(s,XLS_CALL_RANGE,field,d,value);
 return 1;
}
static int mapSlice(call_state* s,xls_ir_call_domain d,const char* f,size_t first,size_t count,size_t empty,uint64_t* out)
{
 if(!count){*out=empty;return 1;}
 if(count>s->limits[d] || first>SIZE_MAX-count)return fail(s,XLS_CALL_RANGE,f,d,first);
 int oneBased=d>=XLS_CALL_NODE;uint64_t begin=0;
 for(size_t i=0;i<count;++i){
  if(!(i%64) && stopped(s))return 0;
  uint64_t value;if(!mapOne(s,d,f,(uint64_t)first+i+(unsigned)oneBased,&value))return 0;
  if(oneBased)--value;
  if(!i){begin=value;if(begin>s->limits[d]-count)return fail(s,XLS_CALL_RANGE,f,d,first);}
  else if(value!=begin+i)return fail(s,XLS_CALL_RANGE,f,d,first+i);
 }
 *out=begin;return 1;
}
static int dispatch(call_state* s,XL6_IrCallDispatchKind kind,uint32 value,uint64_t* out)
{
 switch(kind){
 case XL6_IR_CALL_DISPATCH_CLASS:return mapOne(s,XLS_CALL_CLASS_SLOT,"dispatchSlot",value,out);
 case XL6_IR_CALL_DISPATCH_PROTOCOL:return mapOne(s,XLS_CALL_PROTOCOL_SLOT,"dispatchSlot",value,out);
 case XL6_IR_CALL_DISPATCH_DIRECT:case XL6_IR_CALL_DISPATCH_CALLABLE_VALUE:
  if(!value){*out=0;return 1;}
  break;
 }
 return fail(s,XLS_CALL_INPUT,"dispatchKind/slot",XLS_CALL_SITE,value);
}
static int textEqual(const char* a,const char* b){return (!a&&!b)||(a&&b&&!strcmp(a,b));}
#include "ir-call-records.inc"
void xlsIrCallLinkDestroy(xls_ir_call_link* image)
{if(image){for(size_t i=0;i<image->retained;++i)xlsIrTableUnitRelease(image->units[i]);xrtFree(image);}}
xls_ir_call_link* xlsIrCallLink(const XL6_IrModule* current,const xls_ir_call_binding* bindings,
 size_t count,size_t limit,int(*cancelled)(void*),void* user,xls_ir_graph_status* status,xls_ir_call_error* error)
{
 xls_ir_call_link* image=NULL;placement* places=NULL;xls_ir_graph_status result=XLS_IR_GRAPH_INVALID;
 call_state s={.current=current,.cancelled=cancelled,.user=user,.error=error};if(error)*error=(xls_ir_call_error){0};
 if(!current || (count&&!bindings)){fail(&s,XLS_CALL_INPUT,"input",XLS_CALL_SITE,0);goto failed;}
 size_t bytes=sizeof(*image),at[8],starts[8],total=0,units,scratch,peak;
 for(size_t i=0;i<8;++i){size_t n,c;const void* p=descriptors[i].read(current,&n,&c);starts[i]=total;
  if(n>c || (n&&!p)){fail(&s,XLS_CALL_INPUT,"table",(xls_ir_call_domain)i,n);goto failed;}
  s.limits[i]=n;result=XLS_IR_GRAPH_BUDGET;
  if(!add(&total,n)||!region(&bytes,n,descriptors[i].width,descriptors[i].alignment,&at[i]))goto failed;
 }
 result=XLS_IR_GRAPH_INVALID;
 if(total!=count){fail(&s,XLS_CALL_INPUT,"coverage",XLS_CALL_SITE,count);goto failed;}
 s.limits[XLS_CALL_NODE]=current->nodeCount;s.limits[XLS_CALL_TYPE]=current->types.count;s.limits[XLS_CALL_SYMBOL]=UINT32_MAX;
 s.limits[XLS_CALL_CLASS_SLOT]=current->classDispatchSlotCount;s.limits[XLS_CALL_PROTOCOL_SLOT]=current->protocolDispatchSlotCount;
 s.limits[XLS_CALL_RUNTIME_TAG]=current->classLayoutCount;s.limits[XLS_CALL_FUNCTION]=current->functionCount;s.limits[XLS_CALL_CODEC]=current->dynamicMethodScalarCodecCount;
 for(size_t i=XLS_CALL_NODE;i<XLS_CALL_DOMAIN_COUNT;++i)if(i!=XLS_CALL_CODEC && s.limits[i]>UINT32_MAX){fail(&s,XLS_CALL_INPUT,"domain",(xls_ir_call_domain)i,s.limits[i]);goto failed;}
 if(stopped(&s)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
 result=XLS_IR_GRAPH_BUDGET;
 if(!region(&bytes,count,sizeof(xls_ir_table_unit*),_Alignof(xls_ir_table_unit*),&units) || count>SIZE_MAX/sizeof(placement))goto failed;
 scratch=count*sizeof(placement);peak=bytes;if(!add(&peak,scratch)||peak>limit)goto failed;
 result=XLS_IR_GRAPH_MEMORY;image=xrtCalloc(1,bytes);if(!image)goto failed;
 if(scratch){places=xrtMalloc(scratch);if(!places)goto failed;}
 image->bytes=bytes;image->peak=peak;image->units=(void*)((unsigned char*)image+units);
 for(size_t i=0;i<8;++i)descriptors[i].set(&image->tables,s.limits[i]?(unsigned char*)image+at[i]:NULL,s.limits[i]);
 for(size_t i=0;i<count;++i){
  s.index=i;s.binding=&bindings[i];if(!(i%64)&&stopped(&s)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
  const xls_ir_call_binding* b=&bindings[i];result=XLS_IR_GRAPH_INVALID;
  if(!b->unit||!b->map||xlsIrTableUnitRows(b->unit)!=1){fail(&s,XLS_CALL_INPUT,"unit",XLS_CALL_SITE,0);goto failed;}
  size_t table=0;while(table<8 && strcmp(xlsIrTableUnitName(b->unit),descriptors[table].name))++table;
  if(table==8||xlsIrTableUnitRowBytes(b->unit)!=descriptors[table].width){fail(&s,XLS_CALL_INPUT,"schema",XLS_CALL_SITE,0);goto failed;}
  if(!b->producer||b->producer!=xlsIrTableUnitProducer(b->unit)){fail(&s,XLS_CALL_NAMESPACE,"producer",(xls_ir_call_domain)table,0);goto failed;}
  uint64_t dest;if(!mapOne(&s,(xls_ir_call_domain)table,"row",xlsIrTableUnitFirstRow(b->unit),&dest))goto failed;
  size_t flat=starts[table]+(size_t)dest;if(image->units[flat]){fail(&s,XLS_CALL_DUPLICATE,"row",(xls_ir_call_domain)table,dest);goto failed;}
  image->units[flat]=b->unit;places[i]=(placement){table,(size_t)dest};
 }
 for(size_t i=0;i<count;++i){
  s.index=i;s.binding=&bindings[i];if(!(i%64)&&stopped(&s)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
  size_t t=places[i].table,row=places[i].row,n,c;const call_descriptor* d=&descriptors[t];
  const unsigned char* from=xlsIrTableUnitRow(bindings[i].unit,0);const unsigned char* target=d->read(current,&n,&c);
  unsigned char* dest=(unsigned char*)image+at[t]+row*d->width;memcpy(dest,from,d->width);
  if(!d->relocate(&s,dest,target+row*d->width)){result=stopped(&s)?XLS_IR_GRAPH_CANCELLED:XLS_IR_GRAPH_INVALID;goto failed;}
 }
 if(stopped(&s)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
 for(size_t i=0;i<count;++i){
  if(!(i%64)&&stopped(&s)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
  xlsIrTableUnitRetain(image->units[i]);++image->retained;
 }
 xrtFree(places);if(status)*status=XLS_IR_GRAPH_OK;return image;
failed:xrtFree(places);xlsIrCallLinkDestroy(image);if(status)*status=result;return NULL;
}
int xlsIrCallLinkApply(const xls_ir_call_link* image,XL6_IrModule* view)
{if(!image||!view)return 0;for(size_t i=0;i<8;++i){size_t n,c;const void* p=descriptors[i].read(&image->tables,&n,&c);descriptors[i].set(view,(void*)p,n);}return 1;}
size_t xlsIrCallLinkBytes(const xls_ir_call_link* p){return p?p->bytes:0;}
size_t xlsIrCallLinkPeakBytes(const xls_ir_call_link* p){return p?p->peak:0;}
