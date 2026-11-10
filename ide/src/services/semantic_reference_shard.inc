/* Native-only immutable compact query data. Included after the original
 * checked navigation implementation so binding logic has one authority. */
#include "services/semantic_reference_shard.h"
#include "analysis/identifier.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "src/lexer/xl6_literal_span.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#ifndef XIDE_REFERENCE_ALLOC
#define XIDE_REFERENCE_ALLOC xrtMalloc
#endif
typedef struct reference_source {char *path,*text;size_t size;int kind,prefix;} reference_source;
typedef struct reference_item {reference_row row;xide_lsp_position start,end;} reference_item;
struct xide_reference_shard {xatomic32 references;size_t bytes,sources,count;reference_source* source;reference_item* items;};
struct xide_reference_key {xatomic32 references;size_t bytes,size;char *path,*text;uint32_t start,end;int kind,prefix,identifierUnits;xide_reference_family* family;};
static int referenceIdentifierUnits(const char* text,size_t bytes)
{
 if(!xlsIdentifierValid(text,bytes))return -1;
 int units=0;for(size_t i=0;i<bytes;){unsigned char c=(unsigned char)text[i];size_t n=c<128?1:c<0xe0?2:c<0xf0?3:4;i+=n;units+=n==4?2:1;}return units;
}
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
{if(k && xrtAtomic32FetchSub(&k->references,1,XMEMORY_ACQ_REL)==1){xideSemanticReferenceFamilyRelease(k->family);xrtFree(k);}}
size_t xideSemanticReferenceShardBytes(const xide_reference_shard* s){return s ? s->bytes : 0;}
size_t xideSemanticReferenceKeyBytes(const xide_reference_key* k){return k ? k->bytes : 0;}
size_t xideSemanticReferenceShardSources(const xide_reference_shard* s){return s ? s->sources : 0;}
int xideSemanticReferenceShardSource(const xide_reference_shard* s,size_t i,xide_semantic_source* out)
{if(out)*out=(xide_semantic_source){0};if(!s || !out || i>=s->sources)return 0;const reference_source* r=s->source+i;*out=(xide_semantic_source){r->path,r->text,r->kind,r->prefix};return 1;}
static xide_reference_shard* referenceShardCapture(xide_semantic_definitions* d,
 const xide_semantic_source* sources,size_t count,size_t limit,int* quota)
{
 if(quota)*quota=0;
 size_t bytes=sizeof(xide_reference_shard);xide_reference_shard* s=NULL;
 /* The original navigation constructor already bounds count and references. */
 int okay=count<=SIZE_MAX/sizeof(reference_source) && d->references.Count<=SIZE_MAX/sizeof(reference_item) && referenceCharge(&bytes,count*sizeof(reference_source),limit) && referenceCharge(&bytes,d->references.Count*sizeof(reference_item),limit);
 for(size_t i=0;okay && i<count;++i)okay=referenceSourceBytes(&bytes,sources+i,limit);
 if(!okay && quota)*quota=1;
 if(okay)s=XIDE_REFERENCE_ALLOC(bytes);
 if(s){
  memset(s,0,sizeof(*s));xrtAtomic32Init(&s->references,1);s->bytes=bytes;s->sources=count;s->count=d->references.Count;s->source=(reference_source*)(s+1);s->items=(reference_item*)(s->source+count);char* text=(char*)(s->items+s->count);
  for(size_t i=0;i<count;++i){reference_source* r=s->source+i;size_t pathBytes=strlen(sources[i].path)+1;r->path=text;memcpy(text,sources[i].path,pathBytes);text+=pathBytes;r->size=strlen(sources[i].text);r->text=text;memcpy(text,sources[i].text,r->size+1);text+=r->size+1;r->kind=sources[i].kind;r->prefix=sources[i].utf16Prefix;}
  const reference_row* rows=(const reference_row*)d->references.Data;
  for(size_t i=0;i<s->count;++i){const reference_row* r=rows+i;const span* at=occurrence(d->nodes+r->node,(int)r->source,(point){0},0);if(!at || at->startOffset!=(int)r->start || at->endOffset!=(int)r->end){if(quota)*quota=-1;xideSemanticReferenceShardRelease(s);s=NULL;break;}s->items[i]=(reference_item){*r,{at->start.line,at->start.character,at->startOffset},{at->end.line,at->end.character,at->endOffset}};s->items[i].row.node=0;}
 }
 return s;
}
xide_reference_shard* xideSemanticReferenceShardCreate(xls_projection_store* cache,
 xls_semantic_check* check,const xide_semantic_source* sources,size_t count,size_t limit,xide_model_error* error)
{
 if(!limit || limit>XIDE_REFERENCE_BYTES){referenceError(error);return NULL;}
 xls_projection* lease=NULL;xide_semantic_definitions* d=xideSemanticDefinitionsAcquireTyped(cache,check,sources,count,&lease,NULL,error);if(!d)return NULL;
 xide_reference_shard* s=referenceShardCapture(d,sources,count,limit,NULL);
 xideSemanticDefinitionsReleaseTyped(d,lease);if(!s)referenceError(error);return s;
}
xide_reference_key* xideSemanticReferenceKeyCreate(xls_projection_store* cache,
 xls_semantic_check* check,const xide_semantic_source* sources,size_t count,const char* path,int line,int character,size_t limit,xide_model_error* error)
{return xideSemanticReferenceKeyCreateAt(cache,check,sources,count,path,line,character,limit,NULL,error);}
xide_reference_key* xideSemanticReferenceKeyCreateAt(xls_projection_store* cache,
 xls_semantic_check* check,const xide_semantic_source* sources,size_t count,const char* path,int line,int character,size_t limit,xide_semantic_reference* selected,xide_model_error* error)
{
 if(selected)*selected=(xide_semantic_reference){0};
 if(!path || line<0 || character<0 || !limit || limit>XIDE_REFERENCE_BYTES){referenceError(error);return NULL;}
 xls_projection* lease=NULL;xide_semantic_definitions* d=xideSemanticDefinitionsAcquireTyped(cache,check,sources,count,&lease,NULL,error);if(!d)return NULL;
 xide_reference_key* key=NULL;int failed=0;int source=sourceFor(d,path);size_t decl=SIZE_MAX;span chosenSpan={0};
 if(source){size_t chosen=selectNode(d,source,line,character,&decl);const span* at=chosen==SIZE_MAX?NULL:occurrence(d->nodes+chosen,source,(point){line,character},1);if(!at)decl=SIZE_MAX;else chosenSpan=*at;}
 if(decl!=SIZE_MAX){const span* at=&d->nodes[decl].selection;
  if(at->source && at->endOffset>at->startOffset){const xide_semantic_source* target=sources+at->source-1;size_t bytes=sizeof(*key);int okay=referenceSourceBytes(&bytes,target,limit);
   if(okay)key=XIDE_REFERENCE_ALLOC(bytes);
   if(key){memset(key,0,sizeof(*key));xrtAtomic32Init(&key->references,1);key->bytes=bytes;key->start=(uint32_t)at->startOffset;key->end=(uint32_t)at->endOffset;key->kind=target->kind;key->prefix=target->utf16Prefix;key->path=(char*)(key+1);size_t pathBytes=strlen(target->path)+1;memcpy(key->path,target->path,pathBytes);key->text=key->path+pathBytes;key->size=strlen(target->text);memcpy(key->text,target->text,key->size+1);key->identifierUnits=referenceIdentifierUnits(key->text+key->start,key->end-key->start);}else failed=1;
  }
 }
 if(key && selected)*selected=(xide_semantic_reference){sources[source-1].path,{chosenSpan.start.line,chosenSpan.start.character,chosenSpan.startOffset},{chosenSpan.end.line,chosenSpan.end.character,chosenSpan.endOffset},sameSelection(&chosenSpan,&d->nodes[decl].selection)};
 xideSemanticDefinitionsReleaseTyped(d,lease);if(failed || !source)referenceError(error);return key;
}
int xideSemanticReferenceKeyTarget(const xide_reference_key* k,xide_semantic_source* source,size_t* start,size_t* end)
{
 if(source)*source=(xide_semantic_source){0};
 if(start)*start=0;
 if(end)*end=0;
 if(!k || !source || !start || !end || k->end<=k->start || k->end>k->size)return 0;
 *source=(xide_semantic_source){k->path,k->text,k->kind,k->prefix};*start=k->start;*end=k->end;return 1;
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
static const reference_item* referenceItemAt(const xide_reference_shard_view* view,size_t index)
{
 if(!view || !view->shard || index>=view->count)return NULL;
 const xide_reference_shard* s=view->shard;if(view->first>s->count || view->count>s->count-view->first || (view->excluded!=SIZE_MAX && (view->excluded>view->count || view->count==s->count-view->first)))return NULL;
 return s->items+view->first+index+(view->excluded!=SIZE_MAX && index>=view->excluded);
}
int xideSemanticReferenceShardAt(const xide_reference_shard_view* view,size_t index,xide_semantic_reference* out)
{
 if(out)*out=(xide_semantic_reference){0};
 const reference_item* item=referenceItemAt(view,index);if(!out || !item)return 0;
 const reference_row* r=&item->row;const xide_reference_shard* s=view->shard;
 *out=(xide_semantic_reference){s->source[r->source-1].path,item->start,item->end,r->source==r->targetSource && r->start==r->targetStart && r->end==r->targetEnd};return 1;
}
int xideSemanticReferenceKeyIdentifier(const xide_reference_key* key,xstrview* name,int* units)
{
 if(name)*name=(xstrview){0};
 if(units)*units=0;
 if(!key || !name || !units || key->identifierUnits<=0)return 0;
 *name=(xstrview){key->text+key->start,key->end-key->start};*units=key->identifierUnits;return 1;
}
enum xide_reference_match xideSemanticReferenceShardIdentifiers(const xide_reference_shard* shard,const xide_reference_key* key,int include,xide_reference_identifier_view* out)
{
 if(out)*out=(xide_reference_identifier_view){0};
 if(!out)return XIDE_REFERENCE_INVALID;
 xide_reference_shard_view view;enum xide_reference_match match=xideSemanticReferenceShardQuery(shard,key,include,&view);if(match!=XIDE_REFERENCE_MATCH)return match;
 if(key->identifierUnits<=0)return XIDE_REFERENCE_IDENTIFIER_UNSUPPORTED;
 *out=(xide_reference_identifier_view){.references=view,.utf16Units=key->identifierUnits,.targetStart=key->start,.targetEnd=key->end};
 if(view.count){const reference_item* item=referenceItemAt(&view,0);if(!item)return XIDE_REFERENCE_INVALID;out->targetSource=item->row.targetSource;out->name=(xstrview){shard->source[out->targetSource-1].text+key->start,key->end-key->start};}
 return XIDE_REFERENCE_MATCH;
}
int xideSemanticReferenceShardIdentifierAt(const xide_reference_identifier_view* view,size_t index,xide_semantic_reference* out)
{
 if(out)*out=(xide_semantic_reference){0};
 if(!view || !out || !view->name.Data || view->utf16Units<=0)return 0;
 const reference_item* item=referenceItemAt(&view->references,index);if(!item)return 0;const reference_row* row=&item->row;
 const reference_source* source=view->references.shard->source+row->source-1;
 if(row->targetSource!=view->targetSource || row->targetStart!=view->targetStart || row->targetEnd!=view->targetEnd ||
    view->name.Size!=row->targetEnd-row->targetStart || item->start.offset<0 || item->end.offset<item->start.offset ||
    (size_t)item->end.offset>source->size || view->name.Size>(size_t)(item->end.offset-item->start.offset) || item->end.character<view->utf16Units)return 0;
 size_t end=(size_t)item->end.offset,begin=end-view->name.Size;
 if(memcmp(source->text+begin,view->name.Data,view->name.Size) || (begin && xl6LiteralIdentByte((unsigned char)source->text[begin-1])) ||
    (end<source->size && xl6LiteralIdentByte((unsigned char)source->text[end])))return 0;
 *out=(xide_semantic_reference){source->path,{item->end.line,item->end.character-view->utf16Units,(int)begin},item->end,row->source==row->targetSource && begin==row->targetStart && end==row->targetEnd};return 1;
}
#include "services/semantic_reference_family.inc"
