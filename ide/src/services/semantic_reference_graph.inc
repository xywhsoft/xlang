/* Immutable topology beside the existing exact reference source bank. */
#ifndef XIDE_GRAPH_ALLOC
#define XIDE_GRAPH_ALLOC XIDE_REFERENCE_ALLOC
#endif
typedef struct graph_entry {uint32_t source,start,end,component,next;int units;} graph_entry;
typedef struct graph_component {uint32_t first,members,relations;unsigned kinds,unsupported;} graph_component;
struct xide_reference_graph {
 xatomic32 references;size_t ownedBytes,count,components,relations;
 xide_reference_shard* shard;graph_entry* entries;graph_component* groups;
};
xide_reference_graph* xideSemanticReferenceGraphRetain(xide_reference_graph* g)
{if(g)xrtAtomic32FetchAdd(&g->references,1,XMEMORY_RELAXED);return g;}
void xideSemanticReferenceGraphRelease(xide_reference_graph* g)
{if(g && xrtAtomic32FetchSub(&g->references,1,XMEMORY_ACQ_REL)==1){xideSemanticReferenceShardRelease(g->shard);xrtFree(g);}}
int xideSemanticReferenceGraphInfo(const xide_reference_graph* g,xide_reference_graph_info* out)
{if(out)*out=(xide_reference_graph_info){0};if(!g || !out)return 0;*out=(xide_reference_graph_info){g->count,g->components,g->relations,g->shard->sources,g->ownedBytes,g->ownedBytes+g->shard->bytes};return 1;}
int xideSemanticReferenceGraphDeclarationAt(const xide_reference_graph* g,size_t at,xide_reference_graph_declaration* out)
{if(out)*out=(xide_reference_graph_declaration){0};if(!g || !out || at>=g->count)return 0;const graph_entry* e=g->entries+at;*out=(xide_reference_graph_declaration){e->source,e->start,e->end,e->component,e->units};return 1;}
int xideSemanticReferenceGraphComponentAt(const xide_reference_graph* g,size_t at,xide_reference_graph_component* out)
{if(out)*out=(xide_reference_graph_component){0};if(!g || !out || at>=g->components)return 0;const graph_component* c=g->groups+at;*out=(xide_reference_graph_component){c->members,c->relations,c->kinds,(int)c->unsupported};return 1;}
xide_reference_shard* xideSemanticReferenceGraphRetainShard(xide_reference_graph* g)
{return g ? xideSemanticReferenceShardRetain(g->shard) : NULL;}
static family_location graphDeclaration(const xide_semantic_definitions* d,const declaration* item,uint32_t ordinal)
{
 family_location loc={.index=ordinal};if(item->node!=SIZE_MAX){const span* s=&d->nodes[item->node].selection;if(s->source && s->startOffset>=0 && s->endOffset>s->startOffset){loc.source=(uint32_t)s->source;loc.start=(uint32_t)s->startOffset;loc.end=(uint32_t)s->endOffset;}}return loc;
}
xide_reference_graph* xideSemanticReferenceGraphCreate(xls_projection_store* cache,xls_semantic_check* check,
 const xide_semantic_source* sources,size_t sourceCount,size_t limit,int (*cancelled)(void*),void* user,enum xide_reference_family_error* output)
{
 enum xide_reference_family_error why=XIDE_REFERENCE_FAMILY_INVALID;xide_reference_graph* g=NULL;xide_reference_shard* shard=NULL;xls_projection* lease=NULL;xide_semantic_definitions* d=NULL;xls_callable_relations* callables=NULL;void* scratch=NULL;
 if(!limit)limit=XIDE_REFERENCE_BYTES;
 if(!check || !sources || !sourceCount || limit>XIDE_REFERENCE_BYTES)goto done;
 if(familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
 d=xideSemanticDefinitionsAcquireTyped(cache,check,sources,sourceCount,&lease,NULL,NULL);if(!d || d->native)goto done;
 int shardQuota=0;shard=referenceShardCapture(d,sources,sourceCount,limit,&shardQuota);why=shardQuota>0 ? XIDE_REFERENCE_FAMILY_LIMIT : shardQuota<0 ? XIDE_REFERENCE_FAMILY_INVALID : XIDE_REFERENCE_FAMILY_MEMORY;if(!shard)goto done;
 why=XIDE_REFERENCE_FAMILY_LIMIT;if(shard->bytes>=limit)goto done;
 enum xls_callable_error ce;size_t remaining=limit-shard->bytes;callables=xlsSemanticCheckCallableRelations(check,remaining>32u*1024u*1024u ? 32u*1024u*1024u : remaining,cancelled,user,&ce);
 if(!callables){why=ce==XLS_CALLABLE_LIMIT ? XIDE_REFERENCE_FAMILY_LIMIT : ce==XLS_CALLABLE_MEMORY ? XIDE_REFERENCE_FAMILY_MEMORY : ce==XLS_CALLABLE_CANCELLED ? XIDE_REFERENCE_FAMILY_CANCELLED : XIDE_REFERENCE_FAMILY_INVALID;goto done;}
 xls_dispatch_info info;if(!xlsSemanticCheckDispatchInfo(check,&info)){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}
 size_t dispatch=info.classes+info.protocols,edges=xlsCallableRelationsCount(callables),known=shard->bytes;why=XIDE_REFERENCE_FAMILY_LIMIT;
 if(edges>SIZE_MAX-dispatch || !referenceCharge(&known,xlsCallableRelationsBytes(callables),limit))goto done;
 edges+=dispatch;if(edges>UINT32_MAX/2 || d->typeCount>UINT32_MAX || d->declCount>UINT32_MAX-d->typeCount || 2*edges>UINT32_MAX-d->declCount-d->typeCount)goto done;
 size_t idsCap=2*edges,cap=idsCap+d->declCount+d->typeCount,padding=(4-cap%4)%4,scratchBytes=padding;
 if(idsCap>(limit-known)/sizeof(uint32_t) || !referenceCharge(&scratchBytes,idsCap*sizeof(uint32_t),limit-known) || cap>(limit-known-scratchBytes)/(2*sizeof(uint32_t)+1+sizeof(family_location)))goto done;
 scratchBytes+=cap*(2*sizeof(uint32_t)+1+sizeof(family_location));
 if(scratchBytes){scratch=XIDE_GRAPH_ALLOC(scratchBytes);why=XIDE_REFERENCE_FAMILY_MEMORY;if(!scratch)goto done;}
 uint32_t empty[1];uint32_t* ids=scratch ? scratch : empty;uint32_t* parents=ids+idsCap;uint32_t* roots=parents+cap;unsigned char* rank=(unsigned char*)(roots+cap);family_location* loc=(family_location*)(rank+cap+padding);
 for(size_t i=0;i<edges;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}xls_dispatch_relation r;if(!familyRelation(check,dispatch,callables,i,&r)){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}ids[2*i]=r.declaration;ids[2*i+1]=r.implementation;}
 if(idsCap)qsort(ids,idsCap,sizeof(*ids),familyIdOrder);
 size_t idCount=0;
 for(size_t i=0;i<idsCap;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}if(!i || ids[i]!=ids[i-1])ids[idCount++]=ids[i];}
 size_t count=0;
 for(size_t i=0;i<idCount;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}loc[count]=familyDeclaration(d,ids[i],(uint32_t)count);++count;}
 for(size_t i=0;i<d->declCount+d->typeCount;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}const declaration* item=i<d->declCount ? d->declarations+i : d->typeDeclarations+i-d->declCount;family_location at=graphDeclaration(d,item,(uint32_t)count);if(at.source)loc[count++]=at;}
 for(size_t i=0;i<count;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}parents[i]=(uint32_t)i;}
 if(count){memset(rank,0,count);qsort(loc,count,sizeof(*loc),familyLocationOrder);}
 for(size_t i=1;i<count;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}if(familySameLocation(loc+i-1,loc+i))familyJoin(parents,rank,loc[i-1].index,loc[i].index);}
 for(size_t i=0;i<edges;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}xls_dispatch_relation r;if(!familyRelation(check,dispatch,callables,i,&r)){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}uint32_t a=familyId(ids,idCount,r.declaration),b=familyId(ids,idCount,r.implementation);if(a==UINT32_MAX || b==UINT32_MAX){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}familyJoin(parents,rank,a,b);}
 for(size_t i=0;i<count;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}parents[i]=familyFind(parents,(uint32_t)i);}
 size_t components=0,entries=0;if(count)memset(roots,0xff,count*sizeof(*roots));
 /* Every parent was fully compressed before reusing the array for dense IDs. */
 for(size_t i=0;i<count;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}uint32_t root=parents[i];if(roots[root]==UINT32_MAX)roots[root]=(uint32_t)components++;parents[i]=roots[root];}
 for(size_t i=0;i<count;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}if(loc[i].source && (!i || !familySameLocation(loc+i-1,loc+i)))++entries;}
 why=XIDE_REFERENCE_FAMILY_LIMIT;size_t owned=sizeof(*g);
 if(!referenceCharge(&known,scratchBytes,limit) || entries>SIZE_MAX/sizeof(graph_entry) || components>SIZE_MAX/sizeof(graph_component) || !referenceCharge(&owned,entries*sizeof(graph_entry),limit-known) || !referenceCharge(&owned,components*sizeof(graph_component),limit-known))goto done;
 g=XIDE_GRAPH_ALLOC(owned);why=XIDE_REFERENCE_FAMILY_MEMORY;if(!g)goto done;memset(g,0,sizeof(*g));xrtAtomic32Init(&g->references,1);g->ownedBytes=owned;g->count=entries;g->components=components;g->relations=edges;g->shard=xideSemanticReferenceShardRetain(shard);g->entries=(graph_entry*)(g+1);g->groups=(graph_component*)(g->entries+entries);memset(g->groups,0,components*sizeof(*g->groups));
 for(size_t i=0;i<components;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}g->groups[i].first=UINT32_MAX;roots[i]=UINT32_MAX;}
 size_t at=0;
 for(size_t i=0;i<count;++i){
  if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
  uint32_t component=parents[loc[i].index];graph_component* c=g->groups+component;
  if(!loc[i].source){c->unsupported=1;continue;}
  if(i && familySameLocation(loc+i-1,loc+i))continue;
  if(loc[i].source>shard->sources){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}reference_source* source=shard->source+loc[i].source-1;
  graph_entry* e=g->entries+at;*e=(graph_entry){loc[i].source,loc[i].start,loc[i].end,component,UINT32_MAX,0};
  if((source->kind&1) && e->end>e->start && e->end<=source->size)e->units=referenceIdentifierUnits(source->text+e->start,e->end-e->start);
  if(e->units<=0)c->unsupported=1;
  if(c->first==UINT32_MAX)c->first=(uint32_t)at;else{
   g->entries[roots[component]].next=(uint32_t)at;const graph_entry* first=g->entries+c->first;
   if(first->units>0 && e->units>0 && (first->end-first->start!=e->end-e->start || first->units!=e->units || memcmp(shard->source[first->source-1].text+first->start,source->text+e->start,e->end-e->start)))c->unsupported=1;
  }
  roots[component]=(uint32_t)at;++c->members;++at;
 }
 for(size_t i=0;i<edges;++i){if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}xls_dispatch_relation r;if(!familyRelation(check,dispatch,callables,i,&r)){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}uint32_t a=familyId(ids,idCount,r.declaration);if(a==UINT32_MAX){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}graph_component* c=g->groups+parents[a];++c->relations;c->kinds|=r.kind;}
 if(at!=entries || !xlsSemanticCheckTypedIndex(check)){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}
 why=familyStopped(cancelled,user) ? XIDE_REFERENCE_FAMILY_CANCELLED : XIDE_REFERENCE_FAMILY_OK;
done:
 xrtFree(scratch);xlsCallableRelationsDestroy(callables);xideSemanticReferenceShardRelease(shard);xideSemanticDefinitionsReleaseTyped(d,lease);if(why!=XIDE_REFERENCE_FAMILY_OK){xideSemanticReferenceGraphRelease(g);g=NULL;}if(output)*output=why;return g;
}
xide_reference_family* xideSemanticReferenceGraphFamily(xide_reference_graph* g,const xide_reference_key* key,
 size_t limit,int (*cancelled)(void*),void* user,enum xide_reference_family_error* output)
{
 enum xide_reference_family_error why=XIDE_REFERENCE_FAMILY_INVALID;xide_reference_family* f=NULL;
 if(!limit)limit=32u*1024u*1024u;
 if(!g || !key || limit>XIDE_REFERENCE_BYTES)goto done;
 if(familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
 if(key->identifierUnits<=0){why=XIDE_REFERENCE_FAMILY_UNSUPPORTED;goto done;}
 size_t source=0;for(size_t i=0;i<g->shard->sources;++i)if(!strcmp(g->shard->source[i].path,key->path)){source=i+1;break;}
 if(!source)for(size_t i=0;i<g->shard->sources;++i)if(xidePathLexicalEqual(g->shard->source[i].path,key->path)){source=i+1;break;}
 why=XIDE_REFERENCE_FAMILY_ABSENT;if(!source)goto done;const reference_source* bank=g->shard->source+source-1;
 why=XIDE_REFERENCE_FAMILY_INVALID;if(bank->kind!=key->kind || bank->size!=key->size || (bank->text!=key->text && memcmp(bank->text,key->text,key->size)))goto done;
 size_t low=0,high=g->count;
 while(low<high){size_t mid=low+(high-low)/2;const graph_entry* e=g->entries+mid;int n=unsignedOrder(e->source,(uint32_t)source);if(!n)n=unsignedOrder(e->start,key->start);if(!n)n=unsignedOrder(e->end,key->end);if(n<0)low=mid+1;else high=mid;}
 why=XIDE_REFERENCE_FAMILY_ABSENT;if(low>=g->count)goto done;const graph_entry* selected=g->entries+low;if(selected->source!=source || selected->start!=key->start || selected->end!=key->end)goto done;
 const graph_component* c=g->groups+selected->component;why=XIDE_REFERENCE_FAMILY_UNSUPPORTED;if(c->unsupported || !c->members || selected->units!=key->identifierUnits)goto done;
 size_t bytes=sizeof(*f);why=XIDE_REFERENCE_FAMILY_LIMIT;if((size_t)c->members>limit/sizeof(family_member) || !referenceCharge(&bytes,(size_t)c->members*sizeof(family_member),limit) || bytes>SIZE_MAX-g->ownedBytes-g->shard->bytes)goto done;
 f=XIDE_GRAPH_ALLOC(bytes);why=XIDE_REFERENCE_FAMILY_MEMORY;if(!f)goto done;memset(f,0,sizeof(*f));xrtAtomic32Init(&f->references,1);f->graph=xideSemanticReferenceGraphRetain(g);f->bytes=bytes+g->ownedBytes+g->shard->bytes;f->count=c->members;f->relations=c->relations;f->kinds=c->kinds;f->source=g->shard->source;f->members=(family_member*)(f+1);size_t at=0;uint32_t previous=0;
 for(uint32_t entry=c->first;entry!=UINT32_MAX;entry=g->entries[entry].next){if(!(at%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}const graph_entry* e=g->entries+entry;if(at>=f->count || e->component!=selected->component){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}if(previous!=e->source){++f->sources;previous=e->source;}f->members[at++]=(family_member){e->source-1,e->start,e->end,e->units};}
 if(at!=f->count){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}
 why=familyStopped(cancelled,user) ? XIDE_REFERENCE_FAMILY_CANCELLED : XIDE_REFERENCE_FAMILY_OK;
done:
 if(why!=XIDE_REFERENCE_FAMILY_OK){xideSemanticReferenceFamilyRelease(f);f=NULL;}if(output)*output=why;return f;
}
