struct xide_family_references {xide_reference_shard* shard;xide_family_reference_info info;xide_semantic_reference* rows;};
void xideSemanticFamilyReferencesRelease(xide_family_references* r)
{if(r){xideSemanticReferenceShardRelease(r->shard);xrtFree(r);}}
int xideSemanticFamilyReferencesInfo(const xide_family_references* r,xide_family_reference_info* out)
{if(out)*out=(xide_family_reference_info){0};if(!r || !out)return 0;*out=r->info;return 1;}
int xideSemanticFamilyReferenceAt(const xide_family_references* r,size_t at,xide_semantic_reference* out)
{if(out)*out=(xide_semantic_reference){0};if(!r || !out || at>=r->info.locations)return 0;*out=r->rows[at];return 1;}
static int familyTargetOrder(const void* a,const void* b)
{const family_member *x=a,*y=b;int n=unsignedOrder(x->source,y->source);if(!n)n=unsignedOrder(x->start,y->start);return n ? n : unsignedOrder(x->end,y->end);}
static const family_member* familyReferenceTarget(const family_member* targets,size_t count,uint32_t source,const reference_row* row)
{family_member key={source,row->targetStart,row->targetEnd,0};size_t low=0,high=count;while(low<high){size_t mid=low+(high-low)/2;if(familyTargetOrder(targets+mid,&key)<0)low=mid+1;else high=mid;}return low<count && !familyTargetOrder(targets+low,&key) ? targets+low : NULL;}
static int familyProjectReference(const xide_reference_shard* shard,size_t index,const family_member* target,xide_semantic_reference* out)
{
 const reference_row* row=&shard->items[index].row;if(!row->targetSource || row->targetSource>shard->sources)return 0;
 const reference_source* source=shard->source+row->targetSource-1;if(row->targetEnd<=row->targetStart || row->targetEnd>source->size)return 0;
 xide_reference_identifier_view view={.references={shard,index,1,SIZE_MAX},.name={source->text+row->targetStart,row->targetEnd-row->targetStart},.utf16Units=target->units,.targetSource=row->targetSource,.targetStart=row->targetStart,.targetEnd=row->targetEnd};
 return xideSemanticReferenceShardIdentifierAt(&view,0,out);
}
xide_family_references* xideSemanticReferenceShardFamily(const xide_reference_shard* shard,const xide_reference_family* family,
 int include,size_t limit,int (*cancelled)(void*),void* user,enum xide_reference_family_error* output)
{
 enum xide_reference_family_error why=XIDE_REFERENCE_FAMILY_INVALID;xide_family_references* result=NULL;void* scratch=NULL;merge_source* paths=NULL;size_t prepared=0;
 if(!limit)limit=32u*1024u*1024u;
 if(!shard || !family || (include!=0 && include!=1) || limit>XIDE_REFERENCE_BYTES)goto done;
 if(familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
 why=XIDE_REFERENCE_FAMILY_LIMIT;size_t files=shard->sources;
 if(!referenceCharge(&files,family->sources,UINT32_MAX))goto done;
 size_t known=0;if(files>limit/(sizeof(merge_source)+sizeof(uint32_t)) || !referenceCharge(&known,files*(sizeof(merge_source)+sizeof(uint32_t)),limit) || family->count>(limit-known)/sizeof(family_member) || !referenceCharge(&known,family->count*sizeof(family_member),limit))goto done;
 scratch=XIDE_FAMILY_ALLOC(known ? known : 1);why=XIDE_REFERENCE_FAMILY_MEMORY;if(!scratch)goto done;
 paths=scratch;family_member* targets=(family_member*)(paths+files);uint32_t* sourceMap=(uint32_t*)(targets+family->count);if(files)memset(sourceMap,0,files*sizeof(*sourceMap));
 for(size_t i=0;i<shard->sources;++i){paths[prepared]=(merge_source){shard->source+i,NULL,(uint32_t)prepared};++prepared;}
 uint32_t previous=UINT32_MAX;
 for(size_t i=0;i<family->count;++i)if(previous!=family->members[i].source){if(prepared>=files){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}previous=family->members[i].source;paths[prepared]=(merge_source){family->source+previous,NULL,(uint32_t)prepared};++prepared;}
 if(prepared!=files){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}
 for(size_t i=0;i<files;++i){
  if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
  paths[i].key=xidePathNativeWide(paths[i].source->path);why=XIDE_REFERENCE_FAMILY_MEMORY;if(!paths[i].key)goto done;
  why=XIDE_REFERENCE_FAMILY_LIMIT;if(!referenceCharge(&known,(wcslen(paths[i].key)+1)*sizeof(wchar_t),limit))goto done;
 }
 if(files)qsort(paths,files,sizeof(*paths),mergeSourceOrder);
 xide_family_reference_info info={0};
 for(size_t first=0;first<files;){
  if(!(first%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
  size_t last=first+1;uint32_t canonical=0;int member=0;
  while(last<files && !xidePathNativeKeyCompare(paths[first].key,paths[last].key))++last;
  for(size_t i=first;i<last;++i)if(paths[i].index<shard->sources){if(!canonical)canonical=paths[i].index+1;}else member=1;
  if(canonical && member){const reference_source* base=paths[first].source;
   for(size_t i=first;i<last;++i){const reference_source* source=paths[i].source;why=XIDE_REFERENCE_FAMILY_INVALID;
    if(base->kind!=source->kind || base->prefix!=source->prefix || base->size!=source->size)goto done;
    if(base->text!=source->text){++info.sourceComparisons;info.sourceBytesCompared+=source->size;if(memcmp(base->text,source->text,source->size))goto done;}
    sourceMap[paths[i].index]=canonical;
   }
  }
  first=last;
 }
 size_t groups=0,count=0;previous=UINT32_MAX;
 for(size_t i=0;i<family->count;++i){
  if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
  const family_member* m=family->members+i;if(previous!=m->source){previous=m->source;++groups;}
  uint32_t source=sourceMap[shard->sources+groups-1];if(source){targets[count]=*m;targets[count++].source=source;}
 }
 if(count)qsort(targets,count,sizeof(*targets),familyTargetOrder);
 size_t matches=0;
 for(size_t i=0;count && i<shard->count;++i){
  if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
  const reference_row* row=&shard->items[i].row;why=XIDE_REFERENCE_FAMILY_INVALID;if(!row->targetSource || row->targetSource>shard->sources)goto done;
  const family_member* target=familyReferenceTarget(targets,count,sourceMap[row->targetSource-1],row);if(!target)continue;
  xide_semantic_reference at;why=XIDE_REFERENCE_FAMILY_UNSUPPORTED;if(!familyProjectReference(shard,i,target,&at))goto done;
  if(include || !at.declaration)++matches;
 }
 size_t bytes=sizeof(*result);why=XIDE_REFERENCE_FAMILY_LIMIT;if(matches>(limit-known)/sizeof(xide_semantic_reference) || !referenceCharge(&bytes,matches*sizeof(xide_semantic_reference),limit-known))goto done;
 result=XIDE_FAMILY_ALLOC(bytes);why=XIDE_REFERENCE_FAMILY_MEMORY;if(!result)goto done;memset(result,0,sizeof(*result));result->shard=xideSemanticReferenceShardRetain((xide_reference_shard*)shard);result->rows=(xide_semantic_reference*)(result+1);result->info=info;result->info.locations=matches;result->info.bytes=bytes+shard->bytes;result->info.referenceRowsVisited=count ? 2*shard->count : 0;size_t at=0;
 for(size_t i=0;count && i<shard->count;++i){
  if(!(i%64) && familyStopped(cancelled,user)){why=XIDE_REFERENCE_FAMILY_CANCELLED;goto done;}
  const reference_row* row=&shard->items[i].row;const family_member* target=familyReferenceTarget(targets,count,sourceMap[row->targetSource-1],row);if(!target)continue;
  xide_semantic_reference location;why=XIDE_REFERENCE_FAMILY_UNSUPPORTED;if(!familyProjectReference(shard,i,target,&location))goto done;
  if(include || !location.declaration){if(at>=matches){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}result->rows[at++]=location;}
 }
 if(at!=matches){why=XIDE_REFERENCE_FAMILY_INVALID;goto done;}
 why=familyStopped(cancelled,user) ? XIDE_REFERENCE_FAMILY_CANCELLED : XIDE_REFERENCE_FAMILY_OK;
done:
 for(size_t i=0;i<prepared;++i)free(paths[i].key);
 xrtFree(scratch);if(why!=XIDE_REFERENCE_FAMILY_OK){xideSemanticFamilyReferencesRelease(result);result=NULL;}if(output)*output=why;return result;
}
