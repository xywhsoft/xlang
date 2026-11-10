/* Included beside the original compact navigation rows, never reconstruct
 * binding by identifier spelling or by compilation-local semantic IDs. */
#include "services/semantic_binding_check.h"
#ifndef XIDE_BINDING_ALLOC
#define XIDE_BINDING_ALLOC xrtMalloc
#endif
#ifndef XIDE_BINDING_PATH
#define XIDE_BINDING_PATH xidePathNativeWide
#endif
typedef struct binding_path {wchar_t* key;size_t index;unsigned group;} binding_path;
typedef struct binding_source {uint32_t after;size_t plan;} binding_source;
typedef struct binding_stop {int (*test)(void*);void* user;int stopped;} binding_stop;
static int bindingStopped(binding_stop* stop)
{if(!stop->stopped && stop->test && stop->test(stop->user))stop->stopped=1;return stop->stopped;}
static int bindingPathOrder(const void* a,const void* b)
{
 const binding_path *x=a,*y=b;int c=xidePathNativeKeyCompare(x->key,y->key);
 if(!c)c=unsignedOrder(x->group,y->group);
 return c ? c : (x->index>y->index)-(x->index<y->index);
}
static int bindingTextEqual(const char* a,size_t an,const char* b,size_t bn,
 binding_stop* stop,xide_binding_check_info* info)
{
 if(an!=bn)return 0;
 for(size_t i=0;i<an;){if(bindingStopped(stop))return 0;size_t n=an-i;if(n>65536)n=65536;
  if(memcmp(a+i,b+i,n))return 0;
  i+=n;info->sourceBytesCompared+=n;}
 return 1;
}
static xstrview bindingPostBom(xstrview text)
{if(text.Size>=3 && !memcmp(text.Data,"\xef\xbb\xbf",3)){text.Data+=3;text.Size-=3;}return text;}
static int bindingMapOffset(const xide_rename_plan* plan,size_t document,uint32_t before,uint32_t* after)
{
 size_t mapped=before;
 if(document!=SIZE_MAX && !xideRenamePlanMapOffset(plan,document,before,&mapped))return 0;
 if(mapped>UINT32_MAX)return 0;
 *after=(uint32_t)mapped;return 1;
}
enum xide_binding_check_result xideSemanticBindingsPreserved(
 const xide_reference_shard* before,const xide_reference_shard* after,
 const xide_rename_plan* plan,size_t limit,int (*cancelled)(void*),void* user,xide_binding_check_info* out)
{
 enum xide_binding_check_result why=XIDE_BINDING_ARGUMENT;
 xide_binding_check_info info={0};binding_stop stop={cancelled,user,0};
 void* scratch=NULL;binding_path* paths=NULL;size_t prepared=0;
 if(!limit)limit=32u*1024u*1024u;
 xide_rename_plan_info pi;
 if(!before || !after || !plan || limit>XIDE_REFERENCE_BYTES || !xideRenamePlanInfo(plan,&pi) ||
    before->sources>UINT32_MAX || after->sources>UINT32_MAX)goto done;
 info.beforeReferences=before->count;info.afterReferences=after->count;
 if(bindingStopped(&stop)){why=XIDE_BINDING_CANCELLED;goto done;}
 if(before->sources!=after->sources){why=XIDE_BINDING_SOURCE_CHANGED;goto done;}
 why=XIDE_BINDING_LIMIT;size_t files=before->sources;
 if(!referenceCharge(&files,after->sources,SIZE_MAX) || !referenceCharge(&files,pi.documents,SIZE_MAX) ||
    files>limit/sizeof(binding_path) || !referenceCharge(&info.bytes,files*sizeof(binding_path),limit) ||
    before->sources>(limit-info.bytes)/sizeof(binding_source) ||
    !referenceCharge(&info.bytes,before->sources*sizeof(binding_source),limit) ||
    before->count>(limit-info.bytes)/sizeof(reference_row) ||
    !referenceCharge(&info.bytes,before->count*sizeof(reference_row),limit))goto done;
 scratch=XIDE_BINDING_ALLOC(info.bytes ? info.bytes : 1);why=XIDE_BINDING_MEMORY;if(!scratch)goto done;
 paths=scratch;binding_source* mapping=(binding_source*)(paths+files);
 reference_row* expected=(reference_row*)(mapping+before->sources);
 for(size_t i=0;i<before->sources;++i)mapping[i]=(binding_source){0,SIZE_MAX};
 for(unsigned group=0;group<3;++group){size_t count=group==0?before->sources:group==1?after->sources:pi.documents;
  for(size_t i=0;i<count;++i){
   if(bindingStopped(&stop)){why=XIDE_BINDING_CANCELLED;goto done;}
   const char* path;
   if(group<2)path=(group?after:before)->source[i].path;
   else{xide_rename_plan_document d;if(!xideRenamePlanDocumentAt(plan,i,&d))goto done;path=d.path;}
   binding_path* at=paths+prepared++;*at=(binding_path){NULL,i,group};
   at->key=XIDE_BINDING_PATH(path);why=XIDE_BINDING_MEMORY;if(!at->key)goto done;
   why=XIDE_BINDING_LIMIT;size_t units=wcslen(at->key)+1;
   if(units>(limit-info.bytes)/sizeof(wchar_t) || !referenceCharge(&info.bytes,units*sizeof(wchar_t),limit))goto done;
  }
 }
 if(files)qsort(paths,files,sizeof(*paths),bindingPathOrder);
 for(size_t first=0;first<files;){
  if(bindingStopped(&stop)){why=XIDE_BINDING_CANCELLED;goto done;}
  size_t last=first+1,old=SIZE_MAX,changed=SIZE_MAX,next=SIZE_MAX;
  while(last<files && !xidePathNativeKeyCompare(paths[first].key,paths[last].key))++last;
  for(size_t i=first;i<last;++i){size_t* slot=paths[i].group==0?&old:paths[i].group==1?&next:&changed;
   if(*slot!=SIZE_MAX){why=XIDE_BINDING_ARGUMENT;goto done;}*slot=paths[i].index;}
  why=XIDE_BINDING_SOURCE_CHANGED;
  if((old==SIZE_MAX)!=(next==SIZE_MAX))goto done;
  if(old!=SIZE_MAX){
   const reference_source *a=before->source+old,*b=after->source+next;
   if(a->kind!=b->kind || a->prefix<0 || a->prefix>1 || b->prefix<0 || b->prefix>1)goto done;
   if(changed==SIZE_MAX){if(!bindingTextEqual(a->text,a->size,b->text,b->size,&stop,&info))goto done;}
   else{xide_rename_plan_document d;why=XIDE_BINDING_ARGUMENT;if(!xideRenamePlanDocumentAt(plan,changed,&d))goto done;
    why=XIDE_BINDING_SOURCE_CHANGED;xstrview original=bindingPostBom(d.before),edited=bindingPostBom(d.after);
    if(a->kind!=d.kind || a->prefix!=d.coordinatePrefix ||
       !bindingTextEqual(a->text,a->size,original.Data,original.Size,&stop,&info) ||
       !bindingTextEqual(b->text,b->size,edited.Data,edited.Size,&stop,&info))goto done;
   }
   mapping[old]=(binding_source){(uint32_t)next+1,changed};++info.sources;
  }
  first=last;
 }
 if(before->count!=after->count){why=XIDE_BINDING_CHANGED;goto done;}
 for(size_t i=0;i<before->count;++i){
  if(!(i%64) && bindingStopped(&stop)){why=XIDE_BINDING_CANCELLED;goto done;}
  const reference_row* r=&before->items[i].row;why=XIDE_BINDING_ARGUMENT;
  if(!r->source || r->source>before->sources || !r->targetSource || r->targetSource>before->sources ||
     r->end<=r->start || r->end>before->source[r->source-1].size || r->targetEnd<=r->targetStart ||
     r->targetEnd>before->source[r->targetSource-1].size)goto done;
  const binding_source *s=mapping+r->source-1,*t=mapping+r->targetSource-1;
  reference_row* e=expected+i;*e=(reference_row){.source=s->after,.targetSource=t->after};why=XIDE_BINDING_UNMAPPABLE;
  if(!bindingMapOffset(plan,s->plan,r->start,&e->start) || !bindingMapOffset(plan,s->plan,r->end,&e->end) ||
     !bindingMapOffset(plan,t->plan,r->targetStart,&e->targetStart) || !bindingMapOffset(plan,t->plan,r->targetEnd,&e->targetEnd))goto done;
  ++info.mappedReferences;
 }
 if(before->count)qsort(expected,before->count,sizeof(*expected),compareReference);
 for(size_t i=0;i<after->count;++i){
  if(!(i%64) && bindingStopped(&stop)){why=XIDE_BINDING_CANCELLED;goto done;}
  ++info.comparedReferences;why=XIDE_BINDING_CHANGED;
  if(compareReference(expected+i,&after->items[i].row))goto done;
 }
 why=bindingStopped(&stop)?XIDE_BINDING_CANCELLED:XIDE_BINDING_PRESERVED;
done:
 if(stop.stopped)why=XIDE_BINDING_CANCELLED;
 for(size_t i=0;i<prepared;++i)free(paths[i].key);
 xrtFree(scratch);if(out)*out=info;return why;
}
