/* Batch union of original provider components; no binding/name inference. */
typedef struct merge_source {const reference_source* source;wchar_t* key;uint32_t index;} merge_source;
typedef struct merge_declaration {size_t relations;uint32_t source,start,end,index;int units;unsigned kinds,unsupported,renamePolicy;} merge_declaration;
static int mergeSourceOrder(const void* a,const void* b)
{const merge_source *x=a,*y=b;int n=xidePathNativeKeyCompare(x->key,y->key);return n ? n : unsignedOrder(x->index,y->index);}
static int mergeDeclarationOrder(const void* a,const void* b)
{const merge_declaration *x=a,*y=b;int n=unsignedOrder(x->source,y->source);if(!n)n=unsignedOrder(x->start,y->start);if(!n)n=unsignedOrder(x->end,y->end);return n ? n : unsignedOrder(x->index,y->index);}
static int mergeSameDeclaration(const merge_declaration* a,const merge_declaration* b)
{return a->source==b->source && a->start==b->start && a->end==b->end;}
xide_reference_family* xideSemanticReferenceGraphsFamily(xide_reference_graph* const* graphs,size_t count,
 const xide_reference_key* key,size_t limit,int (*cancelled)(void*),void* user,enum xide_reference_family_error* output)
{
 enum xide_reference_family_error why=XIDE_REFERENCE_FAMILY_INVALID;xide_reference_family* f=NULL;void* scratch=NULL;merge_source* files=NULL;size_t sources=0,declarations=0,components=0,known=0,prepared=0;
 if(!limit)limit=128u*1024u*1024u;
 if(!graphs || !count || !key || limit>128u*1024u*1024u)goto done;
 if(familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
 if(key->identifierUnits<=0){why=XIDE_REFERENCE_FAMILY_UNSUPPORTED;goto done;}
 why=XIDE_REFERENCE_FAMILY_LIMIT;
 for(size_t i=0;i<count;++i){
  if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
  const xide_reference_graph* g=graphs[i];if(!g){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}
  if(!referenceCharge(&known,g->ownedBytes,limit) || !referenceCharge(&known,g->shard->bytes,limit) ||
     !referenceCharge(&sources,g->shard->sources,UINT32_MAX) || !referenceCharge(&declarations,g->count,UINT32_MAX) || !referenceCharge(&components,g->components,UINT32_MAX))goto done;
 }
 size_t scratchBytes=0;
 if(sources>(limit-known)/(sizeof(merge_source)+sizeof(uint32_t)) || !referenceCharge(&scratchBytes,sources*(sizeof(merge_source)+sizeof(uint32_t)),limit-known) ||
    declarations>(limit-known-scratchBytes)/(sizeof(merge_declaration)+sizeof(uint32_t)+1) || !referenceCharge(&scratchBytes,declarations*(sizeof(merge_declaration)+sizeof(uint32_t)+1),limit-known) ||
    components>(limit-known-scratchBytes)/sizeof(uint32_t) || !referenceCharge(&scratchBytes,components*sizeof(uint32_t),limit-known) || !referenceCharge(&known,scratchBytes,limit))goto done;
 scratch=XIDE_FAMILY_ALLOC(scratchBytes ? scratchBytes : 1);why=XIDE_REFERENCE_FAMILY_MEMORY;if(!scratch)goto done;
 files=scratch;merge_declaration* rows=(merge_declaration*)(files+sources);uint32_t* parents=(uint32_t*)(rows+declarations);uint32_t* sourceMap=parents+declarations;uint32_t* heads=sourceMap+sources;unsigned char* rank=(unsigned char*)(heads+components);
 for(size_t i=0;i<declarations;++i){parents[i]=(uint32_t)i;rank[i]=0;}
 if(components)memset(heads,0xff,components*sizeof(*heads));
 for(size_t i=0;i<count;++i)for(size_t j=0;j<graphs[i]->shard->sources;++j){
  if(!(prepared%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
  merge_source* at=files+prepared;*at=(merge_source){graphs[i]->shard->source+j,NULL,(uint32_t)prepared};++prepared;
  at->key=xidePathNativeWide(at->source->path);why=XIDE_REFERENCE_FAMILY_MEMORY;if(!at->key)goto done;
  why=XIDE_REFERENCE_FAMILY_LIMIT;if(!referenceCharge(&known,(wcslen(at->key)+1)*sizeof(wchar_t),limit))goto done;
 }
 if(sources)qsort(files,sources,sizeof(*files),mergeSourceOrder);
 size_t canonical=0,seedSource=SIZE_MAX;wchar_t* seedPath=xidePathNativeWide(key->path);why=XIDE_REFERENCE_FAMILY_MEMORY;if(!seedPath)goto done;
 for(size_t i=0;i<sources;++i){
  if(!(i%64) && familyStopped(cancelled,user)){free(seedPath);why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
  const reference_source* s=files[i].source;
  if(!i || xidePathNativeKeyCompare(files[i-1].key,files[i].key))canonical=i;
  else{const reference_source* first=files[canonical].source;if(first->kind!=s->kind || first->prefix!=s->prefix || first->size!=s->size || memcmp(first->text,s->text,s->size)){free(seedPath);why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}}
  sourceMap[files[i].index]=(uint32_t)canonical;
  if(seedSource==SIZE_MAX && !xidePathNativeKeyCompare(files[i].key,seedPath))seedSource=canonical;
 }
 free(seedPath);why=XIDE_REFERENCE_FAMILY_ABSENT;if(seedSource==SIZE_MAX)goto done;
 const reference_source* seed=files[seedSource].source;why=XIDE_REFERENCE_FAMILY_INVALID;
 if(seed->kind!=key->kind || seed->prefix!=key->prefix || seed->size!=key->size || memcmp(seed->text,key->text,key->size))goto done;
 size_t sourceBase=0,componentBase=0,at=0;
 for(size_t i=0;i<count;++i){const xide_reference_graph* g=graphs[i];
  for(size_t j=0;j<g->count;++j){
   if(!(at%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
   const graph_entry* e=g->entries+j;const graph_component* c=g->groups+e->component;uint32_t* head=heads+componentBase+e->component;
   rows[at]=(merge_declaration){.source=sourceMap[sourceBase+e->source-1],.start=e->start,.end=e->end,.index=(uint32_t)at,.units=e->units,.unsupported=c->unsupported,.renamePolicy=c->renamePolicy};
   if(*head==UINT32_MAX){*head=(uint32_t)at;rows[at].relations=c->relations;rows[at].kinds=c->kinds;}else familyJoin(parents,rank,*head,(uint32_t)at);
   ++at;
  }
  sourceBase+=g->shard->sources;componentBase+=g->components;
 }
 if(declarations)qsort(rows,declarations,sizeof(*rows),mergeDeclarationOrder);
 uint32_t selected=UINT32_MAX;
 for(size_t i=0;i<declarations;++i){
  if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
  if(i && mergeSameDeclaration(rows+i-1,rows+i))familyJoin(parents,rank,rows[i-1].index,rows[i].index);
  if(rows[i].source==seedSource && rows[i].start==key->start && rows[i].end==key->end)selected=rows[i].index;
 }
 why=XIDE_REFERENCE_FAMILY_ABSENT;if(selected==UINT32_MAX)goto done;selected=familyFind(parents,selected);
 size_t members=0,fileCount=0,relations=0,bytes=sizeof(*f);unsigned kinds=0,renamePolicy=0;uint32_t previous=UINT32_MAX;const merge_declaration* last=NULL;
 for(size_t i=0;i<declarations;++i){
  if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
  merge_declaration* row=rows+i;if(familyFind(parents,row->index)!=selected)continue;
  const reference_source* s=files[row->source].source;why=XIDE_REFERENCE_FAMILY_UNSUPPORTED;
  if(row->unsupported || row->units!=key->identifierUnits || row->end<=row->start || row->end>s->size || row->end-row->start!=key->end-key->start || memcmp(s->text+row->start,key->text+key->start,row->end-row->start))goto done;
  kinds|=row->kinds;renamePolicy|=row->renamePolicy;why=XIDE_REFERENCE_FAMILY_LIMIT;if(!referenceCharge(&relations,row->relations,SIZE_MAX))goto done;
  if(last && mergeSameDeclaration(last,row))continue;
  ++members;last=row;
  if(previous!=row->source){++fileCount;previous=row->source;if(!referenceCharge(&bytes,sizeof(reference_source),limit-known) || !referenceCharge(&bytes,strlen(s->path)+1,limit-known) || !referenceCharge(&bytes,s->size+1,limit-known))goto done;}
  if(!referenceCharge(&bytes,sizeof(family_member),limit-known))goto done;
 }
 f=XIDE_FAMILY_ALLOC(bytes);why=XIDE_REFERENCE_FAMILY_MEMORY;if(!f)goto done;memset(f,0,sizeof(*f));xrtAtomic32Init(&f->references,1);f->bytes=bytes;f->count=members;f->sources=fileCount;f->relations=relations;f->kinds=kinds;f->renamePolicy=renamePolicy;f->source=(reference_source*)(f+1);f->members=(family_member*)(f->source+fileCount);char* text=(char*)(f->members+members);at=0;size_t current=0;previous=UINT32_MAX;last=NULL;
 for(size_t i=0;i<declarations;++i){
  if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
  const merge_declaration* row=rows+i;if(familyFind(parents,row->index)!=selected || (last && mergeSameDeclaration(last,row)))continue;last=row;
  if(previous!=row->source){const reference_source* s=files[row->source].source;reference_source* owned=f->source+current++;*owned=*s;size_t pathBytes=strlen(s->path)+1;owned->path=text;why=XIDE_REFERENCE_FAMILY_CANCELLED;if(!familyCopy(text,s->path,pathBytes,cancelled,user))goto done;text+=pathBytes;owned->text=text;if(!familyCopy(text,s->text,s->size+1,cancelled,user))goto done;text+=s->size+1;previous=row->source;}
  f->members[at++]=(family_member){(uint32_t)current-1,row->start,row->end,row->units};
 }
 why=familyStopped(cancelled,user) ? XIDE_REFERENCE_FAMILY_CANCELLED : XIDE_REFERENCE_FAMILY_OK;
done:
 for(size_t i=0;i<prepared;++i)free(files[i].key);
 xrtFree(scratch);if(why!=XIDE_REFERENCE_FAMILY_OK){xideSemanticReferenceFamilyRelease(f);f=NULL;}if(output)*output=why;return f;
}
