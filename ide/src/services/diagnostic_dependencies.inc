/* Actor-only construction; publication getters expose immutable metadata. */
#define DEPENDENCY_CHUNK_BYTES (256u*1024u)
size_t xideDiagnosticPublicationDependencyCount(const xide_diagnostic_publication* p)
{ return p ? p->dependencies.Count : 0; }
const char* xideDiagnosticPublicationDependencyId(const xide_diagnostic_publication* p,size_t i)
{ return p && i<p->dependencies.Count ? ((dependency_set*)p->dependencies.Data)[i].id : NULL; }
const char* xideDiagnosticPublicationGroupDependency(const xide_diagnostic_publication* p,size_t i)
{ return p && i<p->groupDependencies.Count ? xideDiagnosticPublicationDependencyId(p,((size_t*)p->groupDependencies.Data)[i]) : NULL; }
const xvalue* xideDiagnosticPublicationDependencyChunks(const xide_diagnostic_publication* p,size_t i)
{ return p && i<p->dependencies.Count ? ((dependency_set*)p->dependencies.Data)[i].chunks : NULL; }
static int compareDependency(const void* a,const void* b)
{ return strcmp(((const dependency_index*)a)->id,((const dependency_index*)b)->id); }
size_t xideDiagnosticPublicationFindDependency(const xide_diagnostic_publication* p,const char* id)
{
 if(!p || !p->dependencyIndex.Count || !id || strlen(id)!=64) return SIZE_MAX;
 dependency_index key={.index=0};memcpy(key.id,id,65);
 const dependency_index* found=bsearch(&key,p->dependencyIndex.Data,p->dependencyIndex.Count,sizeof(key),compareDependency);
 return found ? found->index : SIZE_MAX;
}
static int dependencyAppendChunk(xvalue* chunks,const char* id,xvalue* paths)
{
 xvalue* chunk=xrtValueObject();int okay=chunk && put(chunk,"schema",xrtValueInt(1)) && put(chunk,"id",xrtValueString(sv(id))) &&
  put(chunk,"removed",xrtValueBool(false)) && put(chunk,"part",xrtValueInt((int64)xrtValueCount(chunks))) &&
  xrtValueObjectSet(chunk,sv("paths"),paths) && xrtValueArrayAppend(chunks,chunk);
 xrtValueRelease(chunk);return okay;
}
static xvalue* dependencyChunks(const xvalue* paths,const char* id,xcancel* cancel)
{
 xvalue *chunks=xrtValueArray(),*part=xrtValueArray();size_t bytes=2;
 if(!chunks || !part) goto failed;
 for(size_t i=0;i<xrtValueCount(paths);++i) {
  if(i%16==0 && xrtCancelRequested(cancel)) goto failed;
  const xvalue* path=xrtValueArrayGet(paths,i);size_t encoded=encodedBytes(path,DEPENDENCY_CHUNK_BYTES-512);
  if(encoded==SIZE_MAX) goto failed;
  if(encoded+1>DEPENDENCY_CHUNK_BYTES-512-bytes && xrtValueCount(part)) {
   if(!dependencyAppendChunk(chunks,id,part)) goto failed;
   xrtValueRelease(part);part=xrtValueArray();bytes=2;if(!part) goto failed;
  }
  if(encoded+1>DEPENDENCY_CHUNK_BYTES-512-bytes || !xrtValueArrayAppend(part,(xvalue*)path)) goto failed;
  bytes+=encoded+1;
 }
 if(!dependencyAppendChunk(chunks,id,part)) goto failed;
 for(size_t i=0;i<xrtValueCount(chunks);++i) {
  xvalue* chunk=xrtValueArrayGet(chunks,i);
  if(!put(chunk,"parts",xrtValueInt((int64)xrtValueCount(chunks))) || encodedBytes(chunk,DEPENDENCY_CHUNK_BYTES)==SIZE_MAX) goto failed;
 }
 xrtValueRelease(part);return chunks;
failed:
 xrtValueRelease(part);xrtValueRelease(chunks);return NULL;
}
static xvalue* dependencyPathsFromChunks(const xvalue* chunks)
{
 xvalue* paths=xrtValueArray();if(!paths) return NULL;
 for(size_t i=0;i<xrtValueCount(chunks);++i) {
  const xvalue* part=get(xrtValueArrayGet(chunks,i),"paths");
  for(size_t j=0;j<xrtValueCount(part);++j) if(!xrtValueArrayAppend(paths,xrtValueArrayGet(part,j))) {xrtValueRelease(paths);return NULL;}
 }
 return paths;
}
static int prepareDependencies(xide_diagnostic_store* store,record* const* records,const group_ref* refs,size_t count,xcancel* cancel,output* out)
{
 xmap masks={0},identities={0};size_t bytes=0;int okay=0;xide_diagnostic_publication* p=out->publication;
 if(!xrtMapInit(&masks,sizeof(size_t)) || !xrtMapInit(&identities,sizeof(size_t))) {fail(out,"Cannot index diagnostic dependency sets.");goto done;}
 for(size_t first=0;first<count;) {
  if(xrtCancelRequested(cancel)) {fail(out,"Diagnostic dependency preparation cancelled.");goto done;}
  size_t last=first;uint64 mask=0;
  do {size_t owner=refs[last].order/(FILE_LIMIT+RECORD_LIMIT);mask|=(uint64)1<<owner;++last;} while(last<count && !comparePath(refs[first].group->key,refs[last].group->key));
  const size_t* known=xrtMapConstGet(&masks,(xbytesview){(const unsigned char*)&mask,sizeof(mask)});size_t index;
  if(known) index=*known;
  else {
   const xide_language_input_guard* guards[RECORD_LIMIT];size_t guardsCount=0;
   for(size_t i=0;i<RECORD_LIMIT;++i) if(mask&((uint64)1<<i)) guards[guardsCount++]=records[i]->guard;
   xvalue* paths=xideLanguageInputGuardsPaths(guards,guardsCount,stopped,cancel);dependency_set set={0};
   if(!paths || !xlsDiagnosticPathsDigest(paths,set.id)) {xrtValueRelease(paths);fail(out,"Cannot prepare complete diagnostic dependency paths.");goto done;}
   const size_t* existing=xrtMapConstGet(&identities,(xbytesview){(const unsigned char*)set.id,64});
   if(existing) {
    index=*existing;xvalue* previous=dependencyPathsFromChunks(((dependency_set*)p->dependencies.Data)[index].chunks);
    int same=previous && xrtValueEqual(paths,previous);xrtValueRelease(previous);xrtValueRelease(paths);
    if(!same) {fail(out,"Conflicting diagnostic dependency identities.");goto done;}
   } else {
    set.chunks=dependencyChunks(paths,set.id,cancel);xrtValueRelease(paths);index=p->dependencies.Count;
    size_t encoded=set.chunks ? encodedBytes(set.chunks,store->limits.retainedReportBytes-bytes) : SIZE_MAX;
    if(encoded==SIZE_MAX || encoded>store->limits.retainedReportBytes-bytes) {xrtValueRelease(set.chunks);fail(out,"Diagnostic dependency report byte limit exceeded.");goto done;}
    if(!xrtArrayPush(&p->dependencies,&set)) {xrtValueRelease(set.chunks);fail(out,"Cannot retain diagnostic dependencies.");goto done;}
    dependency_index entry={.index=index};memcpy(entry.id,set.id,65);
    if(!xrtArrayPush(&p->dependencyIndex,&entry) || !xrtMapSet(&identities,(xbytesview){(const unsigned char*)set.id,64},&index)) {fail(out,"Cannot retain diagnostic dependency index.");goto done;}
    bytes+=encoded;
   }
   if(!xrtMapSet(&masks,(xbytesview){(const unsigned char*)&mask,sizeof(mask)},&index)) {fail(out,"Cannot retain diagnostic contributor index.");goto done;}
  }
  if(!xrtArrayPush(&p->groupDependencies,&index)) {fail(out,"Cannot retain diagnostic dependency binding.");goto done;}
  first=last;
 }
 if(p->dependencyIndex.Count) qsort(p->dependencyIndex.Data,p->dependencyIndex.Count,sizeof(dependency_index),compareDependency);
 okay=p->groupDependencies.Count==p->keys.Count;
 if(!okay) fail(out,"Incomplete diagnostic dependency bindings.");
done:
 xrtMapUnit(&masks);xrtMapUnit(&identities);return okay;
}
