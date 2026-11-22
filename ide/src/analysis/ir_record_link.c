#include "analysis/ir_record_link.h"
#include <xrt.h>
#include <string.h>
#include <stdint.h>

struct xls_ir_record_link {
 unsigned families;XL6_IrModule tables;size_t bytes,peak,retained; xls_ir_table_unit** units;
};
typedef struct record_state {
 const XL6_IrModule* current;const xls_ir_record_binding* binding;
 size_t index,limits[XLS_RECORD_DOMAIN_COUNT];unsigned char indexed[XLS_RECORD_DOMAIN_COUNT];xls_ir_record_error* error;
 int(*cancelled)(void*);void* user;int cancelObserved;
} record_state;
typedef struct record_descriptor {
 const char* name;xls_ir_record_domain domain;unsigned family;size_t width,alignment;
 const void*(*read)(const XL6_IrModule*,size_t*,size_t*);
 void(*set)(XL6_IrModule*,void*,size_t);
 int(*relocate)(record_state*,void*,const void*);
} record_descriptor;
typedef struct placement {size_t table,row;} placement;
static int add(size_t* n,size_t k){if(k>SIZE_MAX-*n)return 0;*n+=k;return 1;}
static int region(size_t* n,size_t count,size_t width,size_t alignment,size_t* at)
{if(!add(n,(alignment-*n%alignment)%alignment)||count>SIZE_MAX/width)return 0;*at=*n;return add(n,count*width);}
static int fail(record_state* s,xls_ir_record_reason reason,const char* f,xls_ir_record_domain d,uint64_t v)
{if(s->error)*s->error=(xls_ir_record_error){reason,s->index,f,d,v};return 0;}
static int stopped(record_state* s){if(s->cancelObserved)return 1;if(s->cancelled && s->cancelled(s->user))s->cancelObserved=1;return s->cancelObserved;}
static int mapOne(record_state* s,xls_ir_record_domain d,const char* field,uint64_t value,uint64_t* out)
{
 int indexed=s->indexed[d];
 if(!indexed && !value){*out=0;return 1;}
 if(!s->binding->map(s->binding->user,d,value,out))return fail(s,XLS_RECORD_UNMAPPED,field,d,value);
 if((indexed ? *out>=s->limits[d] : !*out || *out>s->limits[d]))return fail(s,XLS_RECORD_RANGE,field,d,value);
 return 1;
}
static int mapSlice(record_state* s,xls_ir_record_domain d,const char* f,size_t first,size_t count,size_t empty,uint64_t* out)
{
 if(!count){*out=empty;return 1;}
 if(count>s->limits[d] || first>SIZE_MAX-count)return fail(s,XLS_RECORD_RANGE,f,d,first);
 int oneBased=!s->indexed[d];uint64_t begin=0;
 for(size_t i=0;i<count;++i){
  if(!(i%64) && stopped(s))return 0;
  uint64_t value;if(!mapOne(s,d,f,(uint64_t)first+i+(unsigned)oneBased,&value))return 0;
  if(oneBased)--value;
  if(!i){begin=value;if(begin>s->limits[d]-count)return fail(s,XLS_RECORD_RANGE,f,d,first);}
  else if(value!=begin+i)return fail(s,XLS_RECORD_RANGE,f,d,first+i);
 }
 *out=begin;return 1;
}
static int dispatch(record_state* s,XL6_IrCallDispatchKind kind,uint32 value,uint64_t* out)
{
 switch(kind){
 case XL6_IR_CALL_DISPATCH_CLASS:return mapOne(s,XLS_RECORD_CLASS_SLOT,"dispatchSlot",value,out);
 case XL6_IR_CALL_DISPATCH_PROTOCOL:return mapOne(s,XLS_RECORD_PROTOCOL_SLOT,"dispatchSlot",value,out);
 case XL6_IR_CALL_DISPATCH_DIRECT:case XL6_IR_CALL_DISPATCH_CALLABLE_VALUE:
  if(!value){*out=0;return 1;}
  break;
 }
 return fail(s,XLS_RECORD_INPUT,"dispatchKind/slot",XLS_RECORD_SITE,value);
}
static int textEqual(const char* a,const char* b){return (!a&&!b)||(a&&b&&!strcmp(a,b));}
#include "ir-record-records.inc"
enum {RECORD_TABLE_COUNT=sizeof(descriptors)/sizeof(descriptors[0])};
void xlsIrRecordLinkDestroy(xls_ir_record_link* image)
{if(image){for(size_t i=0;i<image->retained;++i)xlsIrTableUnitRelease(image->units[i]);xrtFree(image);}}
xls_ir_record_link* xlsIrRecordLink(const XL6_IrModule* current,const xls_ir_record_binding* bindings,
 size_t count,unsigned families,size_t limit,int(*cancelled)(void*),void* user,xls_ir_graph_status* status,xls_ir_record_error* error)
{
 xls_ir_record_link* image=NULL;placement* places=NULL;xls_ir_graph_status result=XLS_IR_GRAPH_INVALID;
 record_state s={.current=current,.cancelled=cancelled,.user=user,.error=error};if(error)*error=(xls_ir_record_error){0};
 if(!current || (count&&!bindings) || !families || (families&~(unsigned)XLS_RECORD_ALL)){fail(&s,XLS_RECORD_INPUT,"input",XLS_RECORD_SITE,0);goto failed;}
 size_t bytes=sizeof(*image),at[RECORD_TABLE_COUNT]={0},starts[RECORD_TABLE_COUNT]={0},total=0,units,scratch,peak;
 for(size_t i=0;i<RECORD_TABLE_COUNT;++i){size_t n,c;const record_descriptor* d=&descriptors[i];const void* p=d->read(current,&n,&c);
  s.limits[d->domain]=n;s.indexed[d->domain]=1;
  if(!(d->family&families))continue;
  starts[i]=total;
  if(n>c || (n&&!p)){fail(&s,XLS_RECORD_INPUT,"table",d->domain,n);goto failed;}
  result=XLS_IR_GRAPH_BUDGET;
  if(!add(&total,n)||!region(&bytes,n,d->width,d->alignment,&at[i]))goto failed;
 }
 result=XLS_IR_GRAPH_INVALID;
 if(total!=count){fail(&s,XLS_RECORD_INPUT,"coverage",XLS_RECORD_SITE,count);goto failed;}
 s.limits[XLS_RECORD_NODE]=current->nodeCount;s.limits[XLS_RECORD_TYPE]=current->types.count;s.limits[XLS_RECORD_SYMBOL]=UINT32_MAX;
 s.limits[XLS_RECORD_CLASS_SLOT]=current->classDispatchSlotCount;s.limits[XLS_RECORD_PROTOCOL_SLOT]=current->protocolDispatchSlotCount;
 s.limits[XLS_RECORD_RUNTIME_TAG]=current->classLayoutCount;s.limits[XLS_RECORD_FUNCTION]=current->functionCount;s.limits[XLS_RECORD_CODEC]=current->dynamicMethodScalarCodecCount;
 s.limits[XLS_RECORD_REGION]=UINT32_MAX;
 for(size_t i=XLS_RECORD_NODE;i<XLS_RECORD_DOMAIN_COUNT;++i)if(!s.indexed[i] && i!=XLS_RECORD_CODEC && s.limits[i]>UINT32_MAX){fail(&s,XLS_RECORD_INPUT,"domain",(xls_ir_record_domain)i,s.limits[i]);goto failed;}
 if(stopped(&s)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
 result=XLS_IR_GRAPH_BUDGET;
 if(!region(&bytes,count,sizeof(xls_ir_table_unit*),_Alignof(xls_ir_table_unit*),&units) || count>SIZE_MAX/sizeof(placement))goto failed;
 scratch=count*sizeof(placement);peak=bytes;if(!add(&peak,scratch)||peak>limit)goto failed;
 result=XLS_IR_GRAPH_MEMORY;image=xrtCalloc(1,bytes);if(!image)goto failed;
 if(scratch){places=xrtMalloc(scratch);if(!places)goto failed;}
 image->families=families;image->bytes=bytes;image->peak=peak;image->units=(void*)((unsigned char*)image+units);
 for(size_t i=0;i<RECORD_TABLE_COUNT;++i){const record_descriptor* d=&descriptors[i];size_t n=s.limits[d->domain];if(d->family&families)d->set(&image->tables,n?(unsigned char*)image+at[i]:NULL,n);}
 for(size_t i=0;i<count;++i){
  s.index=i;s.binding=&bindings[i];if(!(i%64)&&stopped(&s)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
  const xls_ir_record_binding* b=&bindings[i];result=XLS_IR_GRAPH_INVALID;
  if(!b->unit||!b->map||xlsIrTableUnitRows(b->unit)!=1){fail(&s,XLS_RECORD_INPUT,"unit",XLS_RECORD_SITE,0);goto failed;}
  size_t table=0;while(table<RECORD_TABLE_COUNT && strcmp(xlsIrTableUnitName(b->unit),descriptors[table].name))++table;
  if(table==RECORD_TABLE_COUNT||!(descriptors[table].family&families)||xlsIrTableUnitRowBytes(b->unit)!=descriptors[table].width){fail(&s,XLS_RECORD_INPUT,"schema",XLS_RECORD_SITE,0);goto failed;}
  if(!b->producer||b->producer!=xlsIrTableUnitProducer(b->unit)){fail(&s,XLS_RECORD_NAMESPACE,"producer",descriptors[table].domain,0);goto failed;}
  uint64_t dest;if(!mapOne(&s,descriptors[table].domain,"row",xlsIrTableUnitFirstRow(b->unit),&dest))goto failed;
  size_t flat=starts[table]+(size_t)dest;if(image->units[flat]){fail(&s,XLS_RECORD_DUPLICATE,"row",descriptors[table].domain,dest);goto failed;}
  image->units[flat]=b->unit;places[i]=(placement){table,(size_t)dest};
 }
 for(size_t i=0;i<count;++i){
  s.index=i;s.binding=&bindings[i];if(!(i%64)&&stopped(&s)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
  size_t t=places[i].table,row=places[i].row,n,c;const record_descriptor* d=&descriptors[t];
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
failed:xrtFree(places);xlsIrRecordLinkDestroy(image);if(status)*status=result;return NULL;
}
int xlsIrRecordLinkApply(const xls_ir_record_link* image,XL6_IrModule* view)
{if(!image||!view)return 0;for(size_t i=0;i<RECORD_TABLE_COUNT;++i)if(descriptors[i].family&image->families){size_t n,c;const void* p=descriptors[i].read(&image->tables,&n,&c);descriptors[i].set(view,(void*)p,n);}return 1;}
size_t xlsIrRecordLinkBytes(const xls_ir_record_link* p){return p?p->bytes:0;}
size_t xlsIrRecordLinkPeakBytes(const xls_ir_record_link* p){return p?p->peak:0;}

unsigned xlsIrRecordLinkFamilies(const xls_ir_record_link* p){return p?p->families:0;}
