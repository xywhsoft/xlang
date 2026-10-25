/* Native-only immutable compact query data. Included after the original
 * checked navigation implementation so binding logic has one authority. */
#include "services/semantic_reference_shard.h"
#ifndef XIDE_REFERENCE_ALLOC
#define XIDE_REFERENCE_ALLOC xrtMalloc
#endif
typedef struct reference_source {char *path,*text;size_t size;int kind,prefix;} reference_source;
typedef struct reference_item {reference_row row;xide_lsp_position start,end;} reference_item;
struct xide_reference_shard {xatomic32 references;size_t bytes,sources,count;reference_source* source;reference_item* items;};
struct xide_reference_key {xatomic32 references;size_t bytes,size;char *path,*text;uint32_t start,end;int kind;};
static void referenceError(xide_model_error* error)
{if(error){error->code=XIDE_MODEL_INVALID;snprintf(error->message,sizeof(error->message),"Unable to capture bounded verified reference data.");}}
static int referenceCharge(size_t* used,size_t add,size_t limit)
{if(*used>limit || add>limit-*used)return 0;*used+=add;return 1;}
static int referenceSourceBytes(size_t* used,const xide_semantic_source* source,size_t limit)
{return referenceCharge(used,strlen(source->path)+1,limit) && referenceCharge(used,strlen(source->text)+1,limit);}
xide_reference_shard* xideSemanticReferenceShardRetain(xide_reference_shard* s)
{if(s)xrtAtomic32FetchAdd(&s->references,1,XMEMORY_RELAXED);return s;}
void xideSemanticReferenceShardRelease(xide_reference_shard* s)
{if(s && xrtAtomic32FetchSub(&s->references,1,XMEMORY_ACQ_REL)==1)xrtFree(s);}
xide_reference_key* xideSemanticReferenceKeyRetain(xide_reference_key* k)
{if(k)xrtAtomic32FetchAdd(&k->references,1,XMEMORY_RELAXED);return k;}
void xideSemanticReferenceKeyRelease(xide_reference_key* k)
{if(k && xrtAtomic32FetchSub(&k->references,1,XMEMORY_ACQ_REL)==1)xrtFree(k);}
size_t xideSemanticReferenceShardBytes(const xide_reference_shard* s){return s ? s->bytes : 0;}
size_t xideSemanticReferenceKeyBytes(const xide_reference_key* k){return k ? k->bytes : 0;}
size_t xideSemanticReferenceShardSources(const xide_reference_shard* s){return s ? s->sources : 0;}
int xideSemanticReferenceShardSource(const xide_reference_shard* s,size_t i,xide_semantic_source* out)
{if(out)*out=(xide_semantic_source){0};if(!s || !out || i>=s->sources)return 0;const reference_source* r=s->source+i;*out=(xide_semantic_source){r->path,r->text,r->kind,r->prefix};return 1;}
xide_reference_shard* xideSemanticReferenceShardCreate(xls_projection_store* cache,
 xls_semantic_check* check,const xide_semantic_source* sources,size_t count,size_t limit,xide_model_error* error)
{
 if(!limit || limit>XIDE_REFERENCE_BYTES){referenceError(error);return NULL;}
 xls_projection* lease=NULL;xide_semantic_definitions* d=xideSemanticDefinitionsAcquireTyped(cache,check,sources,count,&lease,NULL,error);if(!d)return NULL;
 size_t bytes=sizeof(xide_reference_shard);xide_reference_shard* s=NULL;
 /* The original navigation constructor already bounds count and references. */
 int okay=count<=SIZE_MAX/sizeof(reference_source) && d->references.Count<=SIZE_MAX/sizeof(reference_item) && referenceCharge(&bytes,count*sizeof(reference_source),limit) && referenceCharge(&bytes,d->references.Count*sizeof(reference_item),limit);
 for(size_t i=0;okay && i<count;++i)okay=referenceSourceBytes(&bytes,sources+i,limit);
 if(okay)s=XIDE_REFERENCE_ALLOC(bytes);
 if(s){
  memset(s,0,sizeof(*s));xrtAtomic32Init(&s->references,1);s->bytes=bytes;s->sources=count;s->count=d->references.Count;s->source=(reference_source*)(s+1);s->items=(reference_item*)(s->source+count);char* text=(char*)(s->items+s->count);
  for(size_t i=0;i<count;++i){reference_source* r=s->source+i;size_t pathBytes=strlen(sources[i].path)+1;r->path=text;memcpy(text,sources[i].path,pathBytes);text+=pathBytes;r->size=strlen(sources[i].text);r->text=text;memcpy(text,sources[i].text,r->size+1);text+=r->size+1;r->kind=sources[i].kind;r->prefix=sources[i].utf16Prefix;}
  const reference_row* rows=(const reference_row*)d->references.Data;
  for(size_t i=0;i<s->count;++i){const reference_row* r=rows+i;const span* at=occurrence(d->nodes+r->node,(int)r->source,(point){0},0);if(!at || at->startOffset!=(int)r->start || at->endOffset!=(int)r->end){xideSemanticReferenceShardRelease(s);s=NULL;break;}s->items[i]=(reference_item){*r,{at->start.line,at->start.character,at->startOffset},{at->end.line,at->end.character,at->endOffset}};s->items[i].row.node=0;}
 }
 xideSemanticDefinitionsReleaseTyped(d,lease);if(!s)referenceError(error);return s;
}
xide_reference_key* xideSemanticReferenceKeyCreate(xls_projection_store* cache,
 xls_semantic_check* check,const xide_semantic_source* sources,size_t count,const char* path,int line,int character,size_t limit,xide_model_error* error)
{
 if(!path || line<0 || character<0 || !limit || limit>XIDE_REFERENCE_BYTES){referenceError(error);return NULL;}
 xls_projection* lease=NULL;xide_semantic_definitions* d=xideSemanticDefinitionsAcquireTyped(cache,check,sources,count,&lease,NULL,error);if(!d)return NULL;
 xide_reference_key* key=NULL;int failed=0;int source=sourceFor(d,path);size_t decl=SIZE_MAX;
 if(source){size_t chosen=selectNode(d,source,line,character,&decl);if(chosen==SIZE_MAX || !occurrence(d->nodes+chosen,source,(point){line,character},1))decl=SIZE_MAX;}
 if(decl!=SIZE_MAX){const span* at=&d->nodes[decl].selection;
  if(at->source && at->endOffset>at->startOffset){const xide_semantic_source* target=sources+at->source-1;size_t bytes=sizeof(*key);int okay=referenceSourceBytes(&bytes,target,limit);
   if(okay)key=XIDE_REFERENCE_ALLOC(bytes);
   if(key){memset(key,0,sizeof(*key));xrtAtomic32Init(&key->references,1);key->bytes=bytes;key->start=(uint32_t)at->startOffset;key->end=(uint32_t)at->endOffset;key->kind=target->kind;key->path=(char*)(key+1);size_t pathBytes=strlen(target->path)+1;memcpy(key->path,target->path,pathBytes);key->text=key->path+pathBytes;key->size=strlen(target->text);memcpy(key->text,target->text,key->size+1);}else failed=1;
  }
 }
 xideSemanticDefinitionsReleaseTyped(d,lease);if(failed || !source)referenceError(error);return key;
}
enum xide_reference_match xideSemanticReferenceShardQuery(const xide_reference_shard* s,
 const xide_reference_key* key,int include,xide_reference_shard_view* view)
{
 if(view)*view=(xide_reference_shard_view){0};
 if(!s || !key || !view || (include!=0 && include!=1))return XIDE_REFERENCE_INVALID;
 *view=(xide_reference_shard_view){s,0,0,SIZE_MAX};size_t source=0;
 for(size_t i=0;i<s->sources;++i)if(!strcmp(s->source[i].path,key->path)){source=i+1;break;}
 if(!source)for(size_t i=0;i<s->sources;++i)if(xidePathLexicalEqual(s->source[i].path,key->path)){source=i+1;break;}
 if(!source)return XIDE_REFERENCE_MATCH;
 const reference_source* text=s->source+source-1;
 if(text->kind!=key->kind || text->size!=key->size || memcmp(text->text,key->text,key->size)){*view=(xide_reference_shard_view){0};return XIDE_REFERENCE_SOURCE_CHANGED;}
 span target={.source=(int)source,.startOffset=(int)key->start,.endOffset=(int)key->end};size_t lo=0,hi=s->count;
 while(lo<hi){size_t mid=lo+(hi-lo)/2;if(referenceTargetOrder(&s->items[mid].row,&target,0)<0)lo=mid+1;else hi=mid;}size_t first=lo;hi=s->count;
 while(lo<hi){size_t mid=lo+(hi-lo)/2;if(referenceTargetOrder(&s->items[mid].row,&target,0)<=0)lo=mid+1;else hi=mid;}size_t last=lo;view->first=first;view->count=last-first;
 if(!include){lo=first;hi=last;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(referenceOccurrenceOrder(&s->items[mid].row,&target)<0)lo=mid+1;else hi=mid;}if(lo<last && !referenceOccurrenceOrder(&s->items[lo].row,&target)){view->excluded=lo-first;--view->count;}}
 return XIDE_REFERENCE_MATCH;
}
int xideSemanticReferenceShardAt(const xide_reference_shard_view* view,size_t index,xide_semantic_reference* out)
{
 if(out)*out=(xide_semantic_reference){0};
 if(!view || !out || !view->shard || index>=view->count)return 0;
 const xide_reference_shard* s=view->shard;if(view->first>s->count || view->count>s->count-view->first || (view->excluded!=SIZE_MAX && (view->excluded>view->count || view->count==s->count-view->first)))return 0;
 const reference_item* item=s->items+view->first+index+(view->excluded!=SIZE_MAX && index>=view->excluded);const reference_row* r=&item->row;
 *out=(xide_semantic_reference){s->source[r->source-1].path,item->start,item->end,r->source==r->targetSource && r->start==r->targetStart && r->end==r->targetEnd};return 1;
}
