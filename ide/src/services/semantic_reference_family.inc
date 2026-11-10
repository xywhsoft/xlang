#include "analysis/dispatch.h"
#include "analysis/callable.h"
#ifndef XIDE_FAMILY_ALLOC
#define XIDE_FAMILY_ALLOC XIDE_REFERENCE_ALLOC
#endif
typedef struct family_member {uint32_t source,start,end;int units;} family_member;
typedef struct family_location {uint32_t index,source,start,end;int units;} family_location;
struct xide_reference_family {
 xatomic32 references;size_t bytes,count,sources,relations;unsigned kinds;
 reference_source* source;family_member* members;
 xide_reference_graph* graph;
};
xide_reference_family* xideSemanticReferenceFamilyRetain(xide_reference_family* f)
{if(f)xrtAtomic32FetchAdd(&f->references,1,XMEMORY_RELAXED);return f;}
void xideSemanticReferenceFamilyRelease(xide_reference_family* f)
{if(f && xrtAtomic32FetchSub(&f->references,1,XMEMORY_ACQ_REL)==1){xideSemanticReferenceGraphRelease(f->graph);xrtFree(f);}}
int xideSemanticReferenceFamilyInfo(const xide_reference_family* f,xide_reference_family_info* out)
{if(out)*out=(xide_reference_family_info){0};if(!f || !out)return 0;*out=(xide_reference_family_info){f->count,f->sources,f->bytes,f->relations,f->kinds};return 1;}
int xideSemanticReferenceFamilyMember(const xide_reference_family* f,size_t at,xide_semantic_source* source,size_t* start,size_t* end)
{
 if(source)*source=(xide_semantic_source){0};
 if(start)*start=0;
 if(end)*end=0;
 if(!f || at>=f->count || !source || !start || !end)return 0;
 const family_member* m=f->members+at;const reference_source* s=f->source+m->source;
 *source=(xide_semantic_source){s->path,s->text,s->kind,s->prefix};*start=m->start;*end=m->end;return 1;
}
xide_reference_key* xideSemanticReferenceFamilyKey(xide_reference_family* f,size_t at)
{
 if(!f || at>=f->count || f->bytes>SIZE_MAX-sizeof(xide_reference_key))return NULL;
 xide_reference_key* key=XIDE_FAMILY_ALLOC(sizeof(*key));if(!key)return NULL;memset(key,0,sizeof(*key));
 const family_member* m=f->members+at;reference_source* s=f->source+m->source;xrtAtomic32Init(&key->references,1);
 key->family=xideSemanticReferenceFamilyRetain(f);key->bytes=sizeof(*key)+f->bytes;key->size=s->size;key->path=s->path;key->text=s->text;key->kind=s->kind;key->prefix=s->prefix;key->start=m->start;key->end=m->end;key->identifierUnits=m->units;return key;
}
static int familyStopped(int (*cancelled)(void*),void* user){return cancelled && cancelled(user);}
static int familyLocationOrder(const void* a,const void* b)
{const family_location *x=a,*y=b;int n=unsignedOrder(x->source,y->source);if(!n)n=unsignedOrder(x->start,y->start);if(!n)n=unsignedOrder(x->end,y->end);return n?n:unsignedOrder(x->index,y->index);}
static int familySameLocation(const family_location* a,const family_location* b)
{return a->source && a->source==b->source && a->start==b->start && a->end==b->end;}
static int familyIdOrder(const void* a,const void* b){return unsignedOrder(*(const uint32_t*)a,*(const uint32_t*)b);}
static uint32_t familyFind(uint32_t* parents,uint32_t at)
{while(parents[at]!=at){parents[at]=parents[parents[at]];at=parents[at];}return at;}
static void familyJoin(uint32_t* parents,unsigned char* rank,uint32_t a,uint32_t b)
{a=familyFind(parents,a);b=familyFind(parents,b);if(a==b)return;if(rank[a]<rank[b])parents[a]=b;else{parents[b]=a;if(rank[a]==rank[b])++rank[a];}}
static uint32_t familyId(const uint32_t* ids,size_t count,uint32_t id)
{size_t lo=0,hi=count;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(ids[mid]<id)lo=mid+1;else hi=mid;}return lo<count && ids[lo]==id?(uint32_t)lo:UINT32_MAX;}
static family_location familyDeclaration(const xide_semantic_definitions* d,uint32_t id,uint32_t ordinal)
{
 declaration key={id,0};const declaration* found=bsearch(&key,d->declarations,d->declCount,sizeof(*d->declarations),compareDeclaration);
 family_location out={.index=ordinal};if(found && found->node!=SIZE_MAX){const span* s=&d->nodes[found->node].selection;if(s->source && s->startOffset>=0 && s->endOffset>s->startOffset){out.source=(uint32_t)s->source;out.start=(uint32_t)s->startOffset;out.end=(uint32_t)s->endOffset;}}return out;
}
static int familyCopy(char* target,const char* source,size_t bytes,int (*cancelled)(void*),void* user)
{while(bytes){if(familyStopped(cancelled,user))return 0;size_t n=bytes>65536?65536:bytes;memcpy(target,source,n);target+=n;source+=n;bytes-=n;}return 1;}
static int familyRelation(xls_semantic_check* check,size_t dispatch,const xls_callable_relations* groups,size_t at,xls_dispatch_relation* out)
{if(at<dispatch)return xlsSemanticCheckDispatchRelation(check,at,out);xls_callable_relation r;if(!xlsCallableRelationsAt(groups,at-dispatch,&r))return 0;*out=(xls_dispatch_relation){.declaration=r.declaration,.implementation=r.implementation,.kind=r.kind};return 1;}
xide_reference_family* xideSemanticReferenceFamilyCreate(xls_projection_store* cache,xls_semantic_check* check,
 const xide_semantic_source* sources,size_t sourceCount,const xide_reference_key* key,size_t limit,
 int (*cancelled)(void*),void* user,enum xide_reference_family_error* output)
{
 enum xide_reference_family_error why=XIDE_REFERENCE_FAMILY_INVALID;xide_reference_family* family=NULL;void* scratch=NULL;xls_projection* lease=NULL;xide_semantic_definitions* d=NULL;xls_callable_relations* groups=NULL;
 if(!limit)limit=32u*1024u*1024u;
 if(!check || !sources || !sourceCount || !key || limit>XIDE_REFERENCE_BYTES)goto done;
 if(familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
 if(key->identifierUnits<=0){why=XIDE_REFERENCE_FAMILY_UNSUPPORTED;goto done;}
 d=xideSemanticDefinitionsAcquireTyped(cache,check,sources,sourceCount,&lease,NULL,NULL);if(!d)goto done;
 int source=sourceFor(d,key->path);why=XIDE_REFERENCE_FAMILY_ABSENT;if(!source)goto done;
 xls_input_source seedSource;if(!xlsSemanticCheckSource(check,(size_t)source-1,&seedSource) || sources[source-1].kind!=key->kind || seedSource.size!=key->size || memcmp(sources[source-1].text,key->text,key->size)){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}
 family_location seed={.source=(uint32_t)source,.start=key->start,.end=key->end,.units=key->identifierUnits};int foundSeed=0;
 for(size_t i=0;i<d->declCount+d->typeCount;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}const declaration* item=i<d->declCount?d->declarations+i:d->typeDeclarations+i-d->declCount;if(item->node==SIZE_MAX)continue;const span* s=&d->nodes[item->node].selection;if(s->source==source && s->startOffset==(int)key->start && s->endOffset==(int)key->end){foundSeed=1;break;}}
 if(!foundSeed)goto done;
 xls_dispatch_info info;if(!xlsSemanticCheckDispatchInfo(check,&info)){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}
 enum xls_callable_error groupError;groups=xlsSemanticCheckCallableRelations(check,limit>32u*1024u*1024u ? 32u*1024u*1024u : limit,cancelled,user,&groupError);
 if(!groups){why=groupError==XLS_CALLABLE_LIMIT ? XIDE_REFERENCE_FAMILY_LIMIT : groupError==XLS_CALLABLE_MEMORY ? XIDE_REFERENCE_FAMILY_MEMORY : groupError==XLS_CALLABLE_CANCELLED ? XIDE_REFERENCE_FAMILY_CANCELLED : XIDE_REFERENCE_FAMILY_INVALID;goto done;}
 size_t dispatch=info.classes+info.protocols,groupCount=xlsCallableRelationsCount(groups),groupBytes=xlsCallableRelationsBytes(groups);why=XIDE_REFERENCE_FAMILY_LIMIT;if(groupCount>SIZE_MAX-dispatch || groupBytes>limit)goto done;limit-=groupBytes;
 size_t edges=dispatch+groupCount,scratchBytes=0,members=1;family_location* locations=&seed;unsigned kinds=0;size_t connected=0;
 if(edges){
  why=XIDE_REFERENCE_FAMILY_LIMIT;if(edges>UINT32_MAX/2)goto done;size_t cap=edges*2;
  if(cap>(SIZE_MAX-sizeof(family_location))/(2*sizeof(uint32_t)+2+sizeof(family_location)))goto done;
  scratchBytes=cap*(2*sizeof(uint32_t)+2)+(cap+1)*sizeof(family_location);if(scratchBytes>limit)goto done;
  scratch=XIDE_FAMILY_ALLOC(scratchBytes);why=XIDE_REFERENCE_FAMILY_MEMORY;if(!scratch)goto done;
  uint32_t* ids=scratch;uint32_t* parents=ids+cap;unsigned char* rank=(unsigned char*)(parents+cap);unsigned char* wanted=rank+cap;locations=(family_location*)(wanted+cap);
  for(size_t i=0;i<edges;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}xls_dispatch_relation r;if(!familyRelation(check,dispatch,groups,i,&r)){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}ids[2*i]=r.declaration;ids[2*i+1]=r.implementation;}
  qsort(ids,cap,sizeof(*ids),familyIdOrder);size_t count=0;for(size_t i=0;i<cap;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}if(!i || ids[i]!=ids[i-1])ids[count++]=ids[i];}
  memset(rank,0,count);memset(wanted,0,count);
  for(size_t i=0;i<count;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}parents[i]=(uint32_t)i;locations[i]=familyDeclaration(d,ids[i],(uint32_t)i);}
  qsort(locations,count,sizeof(*locations),familyLocationOrder);
  for(size_t i=1;i<count;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}if(familySameLocation(locations+i-1,locations+i))familyJoin(parents,rank,locations[i-1].index,locations[i].index);}
  for(size_t i=0;i<edges;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}xls_dispatch_relation r;if(!familyRelation(check,dispatch,groups,i,&r)){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}uint32_t a=familyId(ids,count,r.declaration),b=familyId(ids,count,r.implementation);if(a==UINT32_MAX || b==UINT32_MAX){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}familyJoin(parents,rank,a,b);}
  for(size_t i=0;i<count;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}if(familySameLocation(locations+i,&seed))wanted[familyFind(parents,locations[i].index)]=1;}
  members=0;
  for(size_t i=0;i<count;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}if(!wanted[familyFind(parents,locations[i].index)])continue;if(!locations[i].source){why=XIDE_REFERENCE_FAMILY_UNSUPPORTED;goto done;}if(!members || !familySameLocation(locations+members-1,locations+i))locations[members++]=locations[i];}
  if(!members){locations[0]=seed;members=1;}
  for(size_t i=0;i<edges;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}xls_dispatch_relation r;if(!familyRelation(check,dispatch,groups,i,&r)){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}uint32_t a=familyId(ids,count,r.declaration);if(a==UINT32_MAX){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}if(wanted[familyFind(parents,a)]){++connected;kinds|=r.kind;}}
 }
 why=XIDE_REFERENCE_FAMILY_LIMIT;size_t bytes=sizeof(*family),files=0,sourceBytes=0;uint32_t previous=0;
 if(members>SIZE_MAX/sizeof(family_member) || !referenceCharge(&bytes,members*sizeof(family_member),limit-scratchBytes))goto done;
 for(size_t i=0;i<members;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}family_location* at=locations+i;const xide_semantic_source* s=sources+at->source-1;
  if(previous!=at->source){xls_input_source captured;if(!xlsSemanticCheckSource(check,at->source-1,&captured)){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}sourceBytes=captured.size;++files;if(!referenceCharge(&bytes,sizeof(reference_source),limit-scratchBytes) || !referenceCharge(&bytes,strlen(s->path)+1,limit-scratchBytes) || !referenceCharge(&bytes,sourceBytes+1,limit-scratchBytes))goto done;previous=at->source;}
  if(!(s->kind&1) || at->end<=at->start || at->end>sourceBytes || at->end-at->start!=key->end-key->start || memcmp(s->text+at->start,key->text+key->start,key->end-key->start) || (at->units=referenceIdentifierUnits(s->text+at->start,at->end-at->start))!=key->identifierUnits){why=XIDE_REFERENCE_FAMILY_UNSUPPORTED;goto done;}
 }
 family=XIDE_FAMILY_ALLOC(bytes);why=XIDE_REFERENCE_FAMILY_MEMORY;if(!family)goto done;memset(family,0,sizeof(*family));xrtAtomic32Init(&family->references,1);family->bytes=bytes;family->count=members;family->sources=files;family->relations=connected;family->kinds=kinds;family->source=(reference_source*)(family+1);family->members=(family_member*)(family->source+files);char* text=(char*)(family->members+members);previous=0;size_t current=0;
 for(size_t i=0;i<members;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}const family_location* at=locations+i;const xide_semantic_source* s=sources+at->source-1;
  if(previous!=at->source){xls_input_source captured;if(!xlsSemanticCheckSource(check,at->source-1,&captured)){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}reference_source* owned=family->source+current++;size_t pathBytes=strlen(s->path)+1;owned->path=text;owned->size=captured.size;owned->kind=s->kind;owned->prefix=s->utf16Prefix;why=XIDE_REFERENCE_FAMILY_CANCELLED;if(!familyCopy(text,s->path,pathBytes,cancelled,user))goto done;text+=pathBytes;owned->text=text;if(!familyCopy(text,s->text,owned->size+1,cancelled,user))goto done;text+=owned->size+1;previous=at->source;}
  family->members[i]=(family_member){(uint32_t)current-1,at->start,at->end,at->units};
 }
 why=XIDE_REFERENCE_FAMILY_CANCELLED;if(familyStopped(cancelled,user))goto done;
 {xls_dispatch_info final;if(!xlsSemanticCheckDispatchInfo(check,&final) || final.classes!=info.classes || final.protocols!=info.protocols){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}}
 why=XIDE_REFERENCE_FAMILY_OK;
done:
 xrtFree(scratch);xlsCallableRelationsDestroy(groups);xideSemanticDefinitionsReleaseTyped(d,lease);if(why!=XIDE_REFERENCE_FAMILY_OK){xideSemanticReferenceFamilyRelease(family);family=NULL;}if(output)*output=why;return family;
}
#include "services/semantic_reference_graph.inc"
#include "services/semantic_reference_merge.inc"
