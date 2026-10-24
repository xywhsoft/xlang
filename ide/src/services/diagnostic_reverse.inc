/* Native actor adapter. The portable core owns only prepared keys/owner bits;
 * use the existing lexical path and guard APIs,including all negative probes.
 * Borrow the original immutable rows;the core already deduplicates keys,so
 * no intermediate JSON path array,second normalization pass or sort is needed.
 * GUI compatibility retains its original actor and runtime. */
static int reverseCompare(xls_dependency_key left,xls_dependency_key right,void* user)
{(void)user;return xidePathNativeKeyCompare(left.data,right.data);}
static wchar_t* reverseKey(const char* path)
{
 char* lexical=xidePathResolveLexical(NULL,path);wchar_t* key=lexical ? xidePathNativeWide(lexical) : NULL;
 free(lexical);return key;
}
static int reverseAffected(xide_diagnostic_store* store,const char* path,uint64_t* affected)
{
 *affected=0;
 if(!store->reverse){++store->reverseFallbacks;return 0;}
 wchar_t* key=reverseKey(path);
 if(!key){++store->reverseFallbacks;*affected=UINT64_MAX;return 1;}
 int okay=xlsDependencyGraphFind(store->reverse,(xls_dependency_key){key,(wcslen(key)+1)*sizeof(*key)},affected);
 free(key);if(!okay)++store->reverseFallbacks;return okay;
}
static void reverseFreeKeys(xls_dependency_key* keys,size_t count)
{for(size_t i=0;i<count;++i)free((void*)keys[i].data);xrtFree(keys);}
static xls_dependency_key* reverseKeys(record* value,size_t* count,xcancel* cancel)
{
 *count=0;const xvalue* lists[]={xideLanguageInputGuardFiles(value->guard),xideLanguageInputGuardProbes(value->guard)};
 if(xrtValueType(lists[0])!=XVALUE_ARRAY || xrtValueType(lists[1])!=XVALUE_ARRAY ||
    xrtValueCount(lists[0])>512 || xrtValueCount(lists[1])>2048)return NULL;
 size_t n=xrtValueCount(lists[0])+xrtValueCount(lists[1]);xls_dependency_key* keys=n ? xrtCalloc(n,sizeof(*keys)) : NULL;
 if(!n || !keys)return NULL;
 for(size_t list=0;list<2;++list)for(size_t i=0;i<xrtValueCount(lists[list]);++i){
  if(xrtCancelRequested(cancel))goto failed;
  const char* path=string(get(xrtValueArrayGet(lists[list],i),"path"));wchar_t* key=path ? reverseKey(path) : NULL;
  if(!key)goto failed;
  keys[(*count)++]=(xls_dependency_key){key,(wcslen(key)+1)*sizeof(*key)};
 }
 return keys;
failed:reverseFreeKeys(keys,*count);*count=0;return NULL;
}
static int reverseUpdate(xide_diagnostic_store* store,record* const* trial,xcancel* cancel)
{
 int changed=0;for(size_t i=0;i<store->limits.records;++i)changed|=store->records[i]!=trial[i];
 if(!changed)return 1;
 xls_dependency_graph* graph=store->reverse;
 if(!graph)graph=xlsDependencyGraphCreate(NULL,reverseCompare,NULL,NULL);
 xls_dependency_update updates[RECORD_LIMIT];xls_dependency_key* owned[RECORD_LIMIT]={0};size_t count=0;
 int okay=graph!=NULL;
 for(size_t i=0;okay && i<store->limits.records;++i)if(!store->reverse || store->records[i]!=trial[i]){
  size_t keys=0;
  if(trial[i]){owned[i]=reverseKeys(trial[i],&keys,cancel);if(!owned[i]){okay=0;break;}}
  updates[count++]=(xls_dependency_update){i,owned[i],keys};
 }
 if(okay)okay=xlsDependencyGraphUpdate(graph,updates,count,stopped,cancel,NULL);
 for(size_t i=0;i<count;++i)reverseFreeKeys(owned[updates[i].root],updates[i].count);
 if(!okay){
  if(xrtCancelRequested(cancel)){if(graph!=store->reverse)xlsDependencyGraphDestroy(graph);return 0;}
  /* An optional index must not reject a valid diagnostic publication. Fall
   * back to the original complete guard walk; next changed owner can rebuild. */
  ++store->reverseFallbacks;xlsDependencyGraphDestroy(graph);store->reverse=NULL;return 1;
 }
 store->reverse=graph;return 1;
}
