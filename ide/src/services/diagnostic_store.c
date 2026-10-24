#include "services/diagnostic_store.h"
#include "platform/path.h"
#include "protocol/diagnostic_paths.h"
#include <xrt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define RECORD_LIMIT 64u
#define FILE_LIMIT 512u
#define GROUP_LIMIT 4096u
#define DIAGNOSTIC_LIMIT 4096u
typedef struct stored_group { wchar_t* key; xvalue* value; } stored_group;
typedef struct record { char* key; xide_language_input_guard* guard; xarray groups; size_t bytes; } record;
struct xide_diagnostic_store { xtaskpool* pool; record* records[RECORD_LIMIT]; xide_diagnostic_limits limits; int dependencies;xide_analysis_catalog* catalog;
#ifdef XLS_COMPILER_XRT
 xls_dependency_graph* reverse;uint64_t reverseFallbacks;
#endif
};
struct xide_diagnostic_query { xfuture* future; xfuturewatch watch; int watchInitialized,watchLinked; void (*wake)(void*); void* user; };
enum operation { PUT,REMOVE,INVALIDATE,SWEEP,SNAPSHOT,CLEAR };
typedef struct input { xide_diagnostic_store* store; enum operation kind; char* key; xvalue* checked; xide_language_input_guard* guard;xide_analysis_catalog* catalog; } input;
typedef struct dependency_set { char id[65];xvalue* chunks; } dependency_set;
typedef struct dependency_index {char id[65];size_t index;} dependency_index;
struct xide_diagnostic_publication { xvalue* documents; xarray keys,dependencies,dependencyIndex,groupDependencies; };
typedef struct output { xide_diagnostic_publication* publication; xvalue* invalidated; size_t records,bytes; uint64_t thread; int taken; char* conflictPath; char error[256];
#ifdef XLS_COMPILER_XRT
 xls_dependency_stats dependencyGraph;uint64_t dependencyFallbacks;int dependencyIndexed;
#endif
} output;
typedef struct source_key { wchar_t* key; int seen; } source_key;
typedef struct group_ref { stored_group* group; size_t order; } group_ref;
typedef struct seen_diagnostic { const xvalue* value; size_t next; } seen_diagnostic;
static xstrview sv(const char* text) { return (xstrview){text,strlen(text)}; }
static xvalue* get(const xvalue* value,const char* key) { return value && xrtValueType(value)==XVALUE_OBJECT ? xrtValueObjectGet(value,sv(key)) : NULL; }
static const char* string(const xvalue* value) { xstrview text={0}; return value && xrtValueGetString(value,&text) && text.Size<=65535 && !memchr(text.Data,0,text.Size) ? text.Data : NULL; }
static int put(xvalue* value,const char* key,xvalue* owned) { return owned && xrtValueObjectSetNew(value,sv(key),owned); }
static int stopped(void* user) { return xrtCancelRequested(user); }
static int fail(output* out,const char* reason)
{
 size_t size=strlen(reason); if(size>=sizeof(out->error)) {
  size=sizeof(out->error)-1; while(size && ((unsigned char)reason[size]&0xc0)==0x80) --size;
 }
 memcpy(out->error,reason,size); out->error[size]=0; return 0;
}
static int addBytes(size_t* total,size_t amount) { if(amount>SIZE_MAX-*total) return 0; *total+=amount; return 1; }
static bool countBytes(xbytesview data,void* user)
{ size_t* size=user; return addBytes(size,data.Size); }
static size_t encodedBytes(const xvalue* value,size_t limit)
{
 if(!limit) return SIZE_MAX;
 xjsonwriteconfig config; xrtJsonWriteConfigInit(&config); config.MaxOutputBytes=limit;
 size_t size=0; return xrtJsonWrite(value,&config,countBytes,&size) ? size : SIZE_MAX;
}
static int comparePath(const wchar_t* a,const wchar_t* b) { return xidePathNativeKeyCompare(a,b); }
static int compareSource(const void* a,const void* b) { return comparePath(((const source_key*)a)->key,((const source_key*)b)->key); }
static int compareGroup(const void* a,const void* b)
{
 const group_ref *x=a,*y=b; int result=comparePath(x->group->key,y->group->key);
 return result ? result : (x->order>y->order)-(x->order<y->order);
}
static void releaseRecord(record* value)
{
 if(!value) return;
 stored_group* groups=(stored_group*)value->groups.Data;
 for(size_t i=0;i<value->groups.Count;++i) { free(groups[i].key); xrtValueRelease(groups[i].value); }
 xrtArrayUnit(&value->groups); xideLanguageInputGuardRelease(value->guard); xrtFree(value->key); xrtFree(value);
}
static void freeInput(void* data,void* user)
{ (void)user; input* value=data; xrtFree(value->key); xrtValueRelease(value->checked); xideLanguageInputGuardRelease(value->guard);xideAnalysisCatalogDestroy(value->catalog); xrtFree(value); }
static void freeOutput(void* data,void* user)
{ (void)user; output* value=data; xideDiagnosticPublicationDestroy(value->publication); xrtValueRelease(value->invalidated); xrtFree(value->conflictPath); xrtFree(value); }
void xideDiagnosticPublicationDestroy(xide_diagnostic_publication* publication)
{
 if(!publication) return;
 wchar_t** keys=(wchar_t**)publication->keys.Data;
 for(size_t i=0;i<publication->keys.Count;++i) free(keys[i]);
 for(size_t i=0;i<publication->dependencies.Count;++i) xrtValueRelease(((dependency_set*)publication->dependencies.Data)[i].chunks);
 xrtArrayUnit(&publication->dependencies);xrtArrayUnit(&publication->dependencyIndex);xrtArrayUnit(&publication->groupDependencies);
 xrtArrayUnit(&publication->keys); xrtValueRelease(publication->documents); xrtFree(publication);
}
size_t xideDiagnosticPublicationCount(const xide_diagnostic_publication* publication)
{ return publication ? publication->keys.Count : 0; }
const xvalue* xideDiagnosticPublicationGroup(const xide_diagnostic_publication* publication,size_t index)
{ return publication && index<publication->keys.Count ? xrtValueArrayGet(publication->documents,index) : NULL; }
size_t xideDiagnosticPublicationFind(const xide_diagnostic_publication* publication,const char* path)
{
 if(!publication || !path) return SIZE_MAX;
 wchar_t* key=xidePathNativeWide(path); if(!key) return SIZE_MAX;
 size_t first=0,last=publication->keys.Count,result=SIZE_MAX; wchar_t** keys=(wchar_t**)publication->keys.Data;
 while(first<last) { size_t at=first+(last-first)/2; int order=xidePathNativeKeyCompare(key,keys[at]); if(!order) { result=at; break; } if(order<0) last=at; else first=at+1; }
 free(key); return result;
}
int xideDiagnosticPublicationCompare(const xide_diagnostic_publication* a,size_t i,const xide_diagnostic_publication* b,size_t j)
{
 const wchar_t* x=a && i<a->keys.Count ? ((wchar_t**)a->keys.Data)[i] : NULL;
 const wchar_t* y=b && j<b->keys.Count ? ((wchar_t**)b->keys.Data)[j] : NULL;
 return xidePathNativeKeyCompare(x,y);
}
int xideDiagnosticPublicationEqual(const xide_diagnostic_publication* a,size_t i,const xide_diagnostic_publication* b,size_t j)
{
 const xvalue *x=xideDiagnosticPublicationGroup(a,i),*y=xideDiagnosticPublicationGroup(b,j);
 return x && y && xrtValueEqual(x,y);
}
static record* buildRecord(input* in,xcancel* cancel,output* out)
{
 const xvalue *files=xideLanguageInputGuardFiles(in->guard),*groups=get(in->checked,"documents");
 size_t count=xrtValueCount(files),groupCount=xrtValueCount(groups); source_key keys[FILE_LIMIT]={0}; size_t used=0;
 record* value=NULL;
 if(!files || !count || count>FILE_LIMIT || !groups || xrtValueType(groups)!=XVALUE_ARRAY || groupCount>FILE_LIMIT+RECORD_LIMIT) { fail(out,"Invalid verified diagnostic input groups."); goto done; }
 xide_model_error error={0};
 if(!xideLanguageInputGuardValidate(in->guard,stopped,cancel,&error)) { fail(out,*error.message ? error.message : "Diagnostic inputs changed."); goto done; }
 for(size_t i=0;i<count;++i) {
  const xvalue* file=xrtValueArrayGet(files,i); int64 kind=0; const char* path=string(get(file,"path"));
  if(!path || !xrtValueGetInt(get(file,"kind"),&kind) || kind<1 || kind>3) { fail(out,"Invalid actual diagnostic input."); goto done; }
  wchar_t* key=xidePathNativeWide(path); if(!key) { fail(out,"Cannot index diagnostic source paths."); goto done; }
  keys[used++]=(source_key){key,0};
 }
 qsort(keys,used,sizeof(*keys),compareSource);
 value=xrtCalloc(1,sizeof(*value));
 if(!value || !(value->key=xrtStrDup(in->key)) || !xrtArrayInit(&value->groups,sizeof(stored_group))) { fail(out,"Cannot allocate diagnostic record."); goto done; }
 value->guard=xideLanguageInputGuardRetain(in->guard);
 value->bytes=xideLanguageInputGuardReportBytes(in->guard);
 if(value->bytes==SIZE_MAX || !addBytes(&value->bytes,strlen(in->key)+1)) { fail(out,"Cannot size diagnostic input reports."); goto done; }
 if(value->bytes>in->store->limits.retainedReportBytes) { fail(out,"Retained diagnostic report byte limit exceeded."); goto done; }
 for(size_t i=0;i<groupCount;++i) {
  if(xrtCancelRequested(cancel)) { fail(out,"Diagnostic update cancelled."); goto done; }
  const xvalue* group=xrtValueArrayGet(groups,i); const char* path=string(get(group,"path")); const xvalue* diagnostics=get(group,"diagnostics");
  wchar_t* key=path ? xidePathNativeWide(path) : NULL;
  if(!key || !diagnostics || xrtValueType(diagnostics)!=XVALUE_ARRAY || xrtValueCount(diagnostics)>DIAGNOSTIC_LIMIT) { free(key); fail(out,"Invalid diagnostic document."); goto done; }
  source_key wanted={key,0}; source_key* actual=bsearch(&wanted,keys,used,sizeof(*keys),compareSource);
  if(!actual) {
   free(key); if(xrtValueCount(diagnostics)) { fail(out,"Diagnostics refer to an unanalyzed overlay."); goto done; }
   continue; /* Empty captured overlays are not successful checks of a file. */
  }
  if(actual->seen) { free(key); fail(out,"Duplicate actual diagnostic document."); goto done; }
  actual->seen=1; size_t bytes=encodedBytes(group,in->store->limits.retainedReportBytes-value->bytes);
  if(bytes==SIZE_MAX || bytes>in->store->limits.retainedReportBytes-value->bytes) { free(key); fail(out,"Retained diagnostic report byte limit exceeded."); goto done; }
  xvalue* owned=xrtValueDeepClone(group);
  stored_group kept={key,owned};
  if(!owned || !addBytes(&value->bytes,bytes) || !xrtArrayPush(&value->groups,&kept)) { free(key); xrtValueRelease(owned); fail(out,"Cannot retain diagnostic document."); goto done; }
 }
 for(size_t i=0;i<used;++i) if(!keys[i].seen) { fail(out,"Actual diagnostic source is missing its result group."); goto done; }
done:
 for(size_t i=0;i<used;++i) free(keys[i].key);
 if(*out->error) { releaseRecord(value); value=NULL; }
 return value;
}
static int compatible(const xvalue* a,const xvalue* b)
{
 static const char* fields[]={"version","origin","normalizedSize","normalizedSha256"};
 for(size_t i=0;i<sizeof(fields)/sizeof(*fields);++i) {
  const xvalue *x=get(a,fields[i]),*y=get(b,fields[i]);
  if((x || y) && (!x || !y || !xrtValueEqual(x,y))) return 0;
 }
 const xvalue *x=get(a,"input"),*y=get(b,"input");
 static const char* diskFields[]={"rawSize","rawSha256","metadata"};
 for(size_t i=0;i<sizeof(diskFields)/sizeof(*diskFields);++i) {
  const xvalue *u=get(x,diskFields[i]),*v=get(y,diskFields[i]);
  if((u || v) && (!u || !v || !xrtValueEqual(u,v))) return 0;
 }
 return 1;
}
/* XRT container hashing requires an explicit identity policy. Diagnostic
   buckets need only the mandatory message/range scalars; full XRT structural
   equality resolves collisions and preserves every optional field. */
static int diagnosticBucket(const xvalue* diagnostic,uint64* hash)
{
 const char* message=string(get(diagnostic,"message")); const xvalue* range=get(diagnostic,"range");
 const xvalue *start=get(range,"start"),*end=get(range,"end"); int64 positions[4];
 if(!message || !xrtValueGetInt(get(start,"line"),&positions[0]) || !xrtValueGetInt(get(start,"character"),&positions[1]) ||
  !xrtValueGetInt(get(end,"line"),&positions[2]) || !xrtValueGetInt(get(end,"character"),&positions[3])) return 0;
 *hash=xrtHash64Seed(positions,sizeof(positions),xrtHash64(message,strlen(message))); return 1;
}
static xvalue* emptyGroup(const xvalue* source)
{
 static const char* fields[]={"path","version","origin","normalizedSize","normalizedSha256","input"};
 xvalue* result=xrtValueObject(); int okay=result!=NULL;
 for(size_t i=0;okay && i<sizeof(fields)/sizeof(*fields);++i) { const xvalue* v=get(source,fields[i]); if(v) okay=put(result,fields[i],xrtValueDeepClone(v)); }
 if(okay) okay=put(result,"diagnostics",xrtValueArray());
 if(!okay) { xrtValueRelease(result); result=NULL; } return result;
}
#include "diagnostic_dependencies.inc"
static int sameRecordContext(unsigned char cache[RECORD_LIMIT][RECORD_LIMIT],record* const* records,size_t left,size_t right)
{
 if(!cache[left][right]) cache[left][right]=cache[right][left]=(unsigned char)
  (xideLanguageInputGuardSameContext(records[left]->guard,records[right]->guard) ? 1 : 2);
 return cache[left][right]==1;
}
static int selectContributions(xide_analysis_catalog* catalog,record* const* records,size_t count,xarray* references,
 unsigned char cache[RECORD_LIMIT][RECORD_LIMIT],xcancel* cancel,output* out)
{
 wchar_t* entries[RECORD_LIMIT]={0};int preferred[RECORD_LIMIT]={0};int okay=0;size_t kept=0;group_ref* refs=(group_ref*)references->Data;
 for(size_t i=0;i<count;++i) if(records[i]) {
  const xls_context* context=xideLanguageInputGuardContext(records[i]->guard);if(!context) continue;
  entries[i]=xidePathNativeWide(xlsContextSpec(context)->entry);if(!entries[i]) {fail(out,"Cannot prepare configured diagnostic entry keys.");goto done;}
  preferred[i]=xideAnalysisCatalogPrefers(catalog,context);
 }
 for(size_t first=0;first<references->Count;) {
  if(xrtCancelRequested(cancel)) goto done;
  size_t last=first+1;while(last<references->Count && !comparePath(refs[first].group->key,refs[last].group->key)) ++last;
  const xls_context* wanted=NULL;enum xide_analysis_binding binding=xideAnalysisCatalogFindNative(catalog,refs[first].group->key,&wanted);
  if(binding==XIDE_ANALYSIS_ERROR) {fail(out,"Cannot resolve configured diagnostic ownership.");goto done;}
  size_t chosen=SIZE_MAX;int best=0;
  for(size_t i=first;i<last;++i) {
   size_t owner=refs[i].order/(FILE_LIMIT+RECORD_LIMIT);if(!entries[owner]) continue;
   const xls_context* context=xideLanguageInputGuardContext(records[owner]->guard);
   if(binding==XIDE_ANALYSIS_PROJECT) {if(context==wanted || xlsContextEqual(context,wanted)) {chosen=owner;break;}}
   else if(binding==XIDE_ANALYSIS_UNOWNED) {int score=!comparePath(entries[owner],refs[first].group->key) ? 3 : preferred[owner] ? 2 : 1;if(score>best) {best=score;chosen=owner;}}
  }
  if(chosen!=SIZE_MAX) for(size_t i=first;i<last;++i) {
   size_t owner=refs[i].order/(FILE_LIMIT+RECORD_LIMIT);
   if(sameRecordContext(cache,records,chosen,owner)) refs[kept++]=refs[i];
  }
  first=last;
 }
 references->Count=kept;okay=1;
done:for(size_t i=0;i<count;++i) free(entries[i]);return okay;
}
static int makeSnapshot(xide_diagnostic_store* store,record* const* records,size_t count,xide_analysis_catalog* catalog,xcancel* cancel,output* out)
{
 size_t limit=store->limits.publishedReportBytes;
 xarray references={0},seen={0}; xmap hashes={0}; int okay=0; size_t publishedBytes=2;
 if(limit<publishedBytes) return fail(out,"Merged diagnostic report byte limit exceeded.");
 if(!xrtArrayInit(&references,sizeof(group_ref)) || !xrtArrayInit(&seen,sizeof(seen_diagnostic)) || !xrtMapInit(&hashes,sizeof(size_t))) { fail(out,"Cannot allocate diagnostic merge state."); goto done; }
 for(size_t i=0;i<count;++i) if(records[i]) {
  ++out->records; if(!addBytes(&out->bytes,records[i]->bytes)) { fail(out,"Diagnostic report size overflow."); goto done; }
  stored_group* groups=(stored_group*)records[i]->groups.Data;
  for(size_t j=0;j<records[i]->groups.Count;++j) { group_ref ref={&groups[j],i*(FILE_LIMIT+RECORD_LIMIT)+j}; if(!xrtArrayPush(&references,&ref)) { fail(out,"Cannot index diagnostic contributions."); goto done; } }
 }
 if(references.Count) qsort(references.Data,references.Count,sizeof(group_ref),compareGroup);
 unsigned char contexts[RECORD_LIMIT][RECORD_LIMIT]={{0}};
 if(catalog && !selectContributions(catalog,records,count,&references,contexts,cancel,out)) goto done;
 out->publication=xrtCalloc(1,sizeof(*out->publication));
 if(!out->publication || !xrtArrayInit(&out->publication->keys,sizeof(wchar_t*)) ||
    !xrtArrayInit(&out->publication->dependencies,sizeof(dependency_set)) || !xrtArrayInit(&out->publication->dependencyIndex,sizeof(dependency_index)) ||
    !xrtArrayInit(&out->publication->groupDependencies,sizeof(size_t)) || !(out->publication->documents=xrtValueArray())) { fail(out,"Cannot allocate merged diagnostics."); goto done; }
 group_ref* refs=(group_ref*)references.Data; xvalue* merged=NULL; const stored_group* previous=NULL; uint64 groupId=0;
 /* One full context comparison per contributing record pair, rather than
    repeatedly comparing potentially large descriptors for every shared file. */
 size_t previousOwner=0;
 for(size_t i=0;i<references.Count;++i) {
  if(xrtCancelRequested(cancel)) { fail(out,"Diagnostic merge cancelled."); goto done; }
  stored_group* source=refs[i].group;size_t owner=refs[i].order/(FILE_LIMIT+RECORD_LIMIT);
  if(!previous || comparePath(previous->key,source->key)) {
   if(++groupId>GROUP_LIMIT) { fail(out,"Merged diagnostic file limit exceeded."); goto done; }
   merged=emptyGroup(source->value); size_t bytes=merged ? encodedBytes(merged,limit-publishedBytes) : SIZE_MAX;
   if(bytes==SIZE_MAX || !addBytes(&bytes,groupId>1 ? 1 : 0) || bytes>limit-publishedBytes) { xrtValueRelease(merged); fail(out,"Merged diagnostic report byte limit exceeded."); goto done; }
   publishedBytes+=bytes;
   size_t keySize=(wcslen(source->key)+1)*sizeof(wchar_t); wchar_t* key=malloc(keySize);
   if(key) memcpy(key,source->key,keySize);
   int appended=key && xrtArrayPush(&out->publication->keys,&key);
   if(!appended) free(key);
   appended=appended && xrtValueArrayAppend(out->publication->documents,merged); xrtValueRelease(merged);
   if(!appended) { fail(out,"Cannot retain merged diagnostic group."); goto done; }
   previous=source;previousOwner=owner;
  } else if(!sameRecordContext(contexts,records,previousOwner,owner)) {
   out->conflictPath=xrtStrDup(string(get(source->value,"path")));
   fail(out,out->conflictPath ? "Conflicting diagnostic analysis contexts." : "Cannot retain diagnostic context conflict path.");goto done;
  } else if(!compatible(previous->value,source->value)) {
   out->conflictPath=xrtStrDup(string(get(source->value,"path")));
   fail(out,out->conflictPath ? "Conflicting diagnostic input versions or fingerprints." : "Cannot retain changed diagnostic input path."); goto done;
  }
  const xvalue* diagnostics=get(source->value,"diagnostics"); xvalue* target=get(merged,"diagnostics");
  for(size_t j=0;j<xrtValueCount(diagnostics);++j) {
   const xvalue* diagnostic=xrtValueArrayGet(diagnostics,j); uint64 key[2]={groupId,0};
   if(!diagnosticBucket(diagnostic,&key[1])) { fail(out,"Invalid diagnostic message or range."); goto done; }
   bool added=false; size_t* head=xrtMapGetOrAdd(&hashes,(xbytesview){(const unsigned char*)key,sizeof(key)},&added);
   if(!head) { fail(out,"Cannot index diagnostic identity."); goto done; } if(added) *head=SIZE_MAX;
   seen_diagnostic* candidates=(seen_diagnostic*)seen.Data; size_t at=*head;
   while(at!=SIZE_MAX && !xrtValueEqual(candidates[at].value,diagnostic)) at=candidates[at].next;
   if(at!=SIZE_MAX) continue;
   if(xrtValueCount(target)==DIAGNOSTIC_LIMIT || seen.Count==DIAGNOSTIC_LIMIT*4u) { fail(out,"Merged diagnostic count limit exceeded."); goto done; }
   size_t bytes=encodedBytes(diagnostic,limit-publishedBytes);
   if(bytes==SIZE_MAX || !addBytes(&bytes,xrtValueCount(target) ? 1 : 0) || bytes>limit-publishedBytes) { fail(out,"Merged diagnostic report byte limit exceeded."); goto done; }
   publishedBytes+=bytes;
   xvalue* clone=xrtValueDeepClone(diagnostic); int appended=clone && xrtValueArrayAppend(target,clone); xrtValueRelease(clone);
   seen_diagnostic remembered={diagnostic,*head}; size_t newHead=seen.Count;
   if(!appended || !xrtArrayPush(&seen,&remembered)) { fail(out,"Cannot retain merged diagnostic."); goto done; } *head=newHead;
  }
 }
 if(store->dependencies && !prepareDependencies(store,records,refs,references.Count,cancel,out)) goto done;
 okay=1;
done:
 xrtArrayUnit(&references); xrtArrayUnit(&seen); xrtMapUnit(&hashes); return okay;
}
#ifdef XLS_COMPILER_XRT
#include "services/diagnostic_reverse.inc"
#endif
static xtaskoutcome execute(xcancel* cancel,void* data,xtaskvalue* result)
{
 input* in=data; xide_diagnostic_store* store=in->store;
 if(xrtCancelRequested(cancel)) return XTASK_CANCELLED;
 output* out=xrtCalloc(1,sizeof(*out)); if(!out) return XTASK_FAILED;
 out->thread=xrtThreadCurrentId(); out->invalidated=xrtValueArray();
 record* trial[RECORD_LIMIT]; memcpy(trial,store->records,sizeof(trial)); record* fresh=NULL;
 if(!out->invalidated) { fail(out,"Cannot allocate diagnostic invalidation list."); goto done; }
#ifdef XLS_COMPILER_XRT
 uint64_t affected=0;int indexed=in->kind==INVALIDATE && reverseAffected(store,in->key,&affected);
#endif
 for(size_t i=0;i<store->limits.records;++i) if(trial[i]) {
  record* value=trial[i];
#ifdef XLS_COMPILER_XRT
  if(in->kind==INVALIDATE && (indexed ? (affected&(UINT64_C(1)<<i))!=0 : xideLanguageInputGuardTouches(value->guard,in->key))) xideLanguageInputGuardInvalidate(value->guard);
#else
  if(in->kind==INVALIDATE && xideLanguageInputGuardTouches(value->guard,in->key)) xideLanguageInputGuardInvalidate(value->guard);
#endif
  if(in->kind==SWEEP) { xide_model_error error={0}; xideLanguageInputGuardValidate(value->guard,stopped,cancel,&error); }
  if(!xideLanguageInputGuardCurrent(value->guard) || in->kind==CLEAR || (in->kind==REMOVE && !strcmp(value->key,in->key))) {
   xvalue* key=xrtValueString(sv(value->key)); int appended=key && xrtValueArrayAppend(out->invalidated,key); xrtValueRelease(key);
   if(!appended) { fail(out,"Cannot retain diagnostic invalidation key."); goto done; } trial[i]=NULL;
  }
 }
 if(in->kind==PUT) {
  size_t slot=store->limits.records;
  for(size_t i=0;i<store->limits.records;++i) if(trial[i] && !strcmp(trial[i]->key,in->key)) { slot=i; break; }
  if(slot==store->limits.records) for(size_t i=0;i<store->limits.records;++i) if(!trial[i]) { slot=i; break; }
  if(slot==store->limits.records) { fail(out,"Diagnostic entry limit exceeded."); goto done; }
  fresh=buildRecord(in,cancel,out); if(!fresh) goto done; trial[slot]=fresh;
 }
 size_t bytes=0;
 for(size_t i=0;i<store->limits.records;++i) if(trial[i] && (!addBytes(&bytes,trial[i]->bytes) || bytes>store->limits.retainedReportBytes)) { fail(out,"Retained diagnostic report byte limit exceeded."); goto done; }
 if(!makeSnapshot(store,trial,store->limits.records,in->catalog ? in->catalog : store->catalog,cancel,out)) goto done;
 if(xrtCancelRequested(cancel)) goto done;
#ifdef XLS_COMPILER_XRT
 if(!reverseUpdate(store,trial,cancel)) goto done;
#endif
 for(size_t i=0;i<store->limits.records;++i) if(store->records[i]!=trial[i]) { releaseRecord(store->records[i]); store->records[i]=trial[i]; }
 if(in->catalog) {xideAnalysisCatalogDestroy(store->catalog);store->catalog=xideAnalysisCatalogRetain(in->catalog);}
 fresh=NULL;
#ifdef XLS_COMPILER_XRT
 xlsDependencyGraphStats(store->reverse,&out->dependencyGraph);out->dependencyFallbacks=store->reverseFallbacks;out->dependencyIndexed=store->reverse!=NULL;
#endif
done:
 releaseRecord(fresh);
 if(xrtCancelRequested(cancel)) { freeOutput(out,NULL); return XTASK_CANCELLED; }
 if(*out->error) { xideDiagnosticPublicationDestroy(out->publication); out->publication=NULL; xrtValueRelease(out->invalidated); out->invalidated=NULL; }
 *result=(xtaskvalue){out,freeOutput,NULL}; return XTASK_SUCCESS;
}
xide_diagnostic_store* xideDiagnosticStoreCreateWithDependencies(const xide_diagnostic_limits* limits,int enabled)
{
 xide_diagnostic_limits chosen=limits ? *limits : (xide_diagnostic_limits){64,32u*1024u*1024u,4u*1024u*1024u,8};
 if((enabled!=0 && enabled!=1) || !chosen.records || chosen.records>RECORD_LIMIT || !chosen.retainedReportBytes || !chosen.publishedReportBytes || !chosen.queue || chosen.queue>64) return NULL;
 xide_diagnostic_store* store=xrtCalloc(1,sizeof(*store)); if(!store) return NULL; store->limits=chosen;store->dependencies=enabled;
#ifdef XLS_COMPILER_XRT
 store->reverse=xlsDependencyGraphCreate(NULL,reverseCompare,NULL,NULL);
#endif
 xtaskpoolconfig config={.Threads=1,.QueueLimit=chosen.queue}; store->pool=xrtTaskPoolCreate(&config);
 if(!store->pool) { xideDiagnosticStoreDestroy(store); return NULL; } return store;
}
xide_diagnostic_store* xideDiagnosticStoreCreate(const xide_diagnostic_limits* limits)
{ return xideDiagnosticStoreCreateWithDependencies(limits,0); }
void xideDiagnosticStoreDestroy(xide_diagnostic_store* store)
{
 if(!store) return;
 if(store->pool) { xrtTaskPoolCancel(store->pool); xrtTaskPoolDestroy(store->pool); }
 for(size_t i=0;i<RECORD_LIMIT;++i) releaseRecord(store->records[i]);
 xideAnalysisCatalogDestroy(store->catalog);
#ifdef XLS_COMPILER_XRT
 xlsDependencyGraphDestroy(store->reverse);
#endif
 xrtFree(store);
}
int xideDiagnosticStoreLoad(xide_diagnostic_store* store,xide_diagnostic_load* load)
{
 if(!load) return 0;
 memset(load,0,sizeof(*load)); xtaskpoolstats stats={0};
 if(!store || !xrtTaskPoolGet(store->pool,&stats)) return 0;
 *load=(xide_diagnostic_load){stats.Queued,stats.Running,stats.Submitted,stats.Completed,stats.Cancelled,stats.Rejected}; return 1;
}
static xide_diagnostic_query* submit(xide_diagnostic_store* store,enum operation kind,const char* key,xide_language_input_guard* guard,xvalue* checked,xide_analysis_catalog* catalog)
{
 if(!store || (kind==PUT && (!guard || !checked)) || ((kind==PUT || kind==REMOVE || kind==INVALIDATE) && (!key || !*key || strlen(key)>65535 || !xrtUtf8Valid(sv(key),NULL)))) { xrtValueRelease(checked); return NULL; }
 input* in=xrtCalloc(1,sizeof(*in)); xide_diagnostic_query* query=xrtCalloc(1,sizeof(*query));
 if(!in || !query) { xrtFree(in); xrtFree(query); xrtValueRelease(checked); return NULL; }
 in->store=store; in->kind=kind; in->checked=checked; in->guard=xideLanguageInputGuardRetain(guard); if(key) in->key=xrtStrDup(key);
 in->catalog=xideAnalysisCatalogRetain(catalog);
 if(key && !in->key) { freeInput(in,NULL); xrtFree(query); return NULL; }
 xtaskargs args={.Destroy=freeInput}; query->future=xrtTaskSubmit(store->pool,execute,in,&args);
 if(!query->future) { freeInput(in,NULL); xrtFree(query); return NULL; } return query;
}
xide_diagnostic_query* xideDiagnosticStorePutTake(xide_diagnostic_store* store,const char* key,xide_language_input_guard* guard,xvalue* checked)
{ return submit(store,PUT,key,guard,checked,NULL); }
xide_diagnostic_query* xideDiagnosticStorePutConfigured(xide_diagnostic_store* store,const char* key,xide_language_input_guard* guard,xvalue* checked,xide_analysis_catalog* catalog)
{if(!catalog || !xideLanguageInputGuardContext(guard)) {xrtValueRelease(checked);return NULL;}return submit(store,PUT,key,guard,checked,catalog);}
xide_diagnostic_query* xideDiagnosticStoreRemove(xide_diagnostic_store* store,const char* key) { return submit(store,REMOVE,key,NULL,NULL,NULL); }
xide_diagnostic_query* xideDiagnosticStoreInvalidate(xide_diagnostic_store* store,const char* path) { return submit(store,INVALIDATE,path,NULL,NULL,NULL); }
xide_diagnostic_query* xideDiagnosticStoreSweep(xide_diagnostic_store* store) { return submit(store,SWEEP,NULL,NULL,NULL,NULL); }
xide_diagnostic_query* xideDiagnosticStoreSnapshot(xide_diagnostic_store* store) { return submit(store,SNAPSHOT,NULL,NULL,NULL,NULL); }
xide_diagnostic_query* xideDiagnosticStoreClear(xide_diagnostic_store* store) { return submit(store,CLEAR,NULL,NULL,NULL,NULL); }
enum xide_diagnostic_state xideDiagnosticQueryResult(xide_diagnostic_query* query,xide_diagnostic_snapshot* result)
{
 if(result) memset(result,0,sizeof(*result));
 if(!query || !result) return XIDE_DIAGNOSTIC_FAILED;
 xfuturestate state=xrtFutureState(query->future); if(state==XFUTURE_PENDING) return XIDE_DIAGNOSTIC_PENDING;
 if(state==XFUTURE_CANCELLED) return XIDE_DIAGNOSTIC_CANCELLED;
 const output* out=state==XFUTURE_RESOLVED ? xrtFutureValue(query->future) : NULL;
 if(!out || out->taken) { snprintf(result->error,sizeof(result->error),"Diagnostic actor failed or result ownership was transferred."); return XIDE_DIAGNOSTIC_FAILED; }
 *result=(xide_diagnostic_snapshot){out->publication ? out->publication->documents : NULL,out->invalidated,out->records,out->bytes,out->thread,out->conflictPath,{0}
#ifdef XLS_COMPILER_XRT
 ,{0},0,0
#endif
 };
#ifdef XLS_COMPILER_XRT
 result->dependencyGraph=out->dependencyGraph;result->dependencyFallbacks=out->dependencyFallbacks;result->dependencyIndexed=out->dependencyIndexed;
#endif
 snprintf(result->error,sizeof(result->error),"%s",out->error); return *out->error ? XIDE_DIAGNOSTIC_FAILED : XIDE_DIAGNOSTIC_READY;
}
int xideDiagnosticQueryTake(xide_diagnostic_query* query,xide_diagnostic_publication** publication,xvalue** invalidated)
{
 if(publication) *publication=NULL;
 if(invalidated) *invalidated=NULL;
 if(!query || !publication || !invalidated) return 0;
 xide_diagnostic_snapshot result={0}; if(xideDiagnosticQueryResult(query,&result)!=XIDE_DIAGNOSTIC_READY) return 0;
 output* value=xrtFutureValue(query->future); value->taken=1;
 *publication=value->publication; value->publication=NULL; *invalidated=value->invalidated; value->invalidated=NULL; return 1;
}
static void wakeQuery(void* user) { xide_diagnostic_query* query=user; query->wake(query->user); }
int xideDiagnosticQuerySetWake(xide_diagnostic_query* query,void (*wake)(void*),void* user)
{
 if(!query || !wake || query->watchInitialized) return 0;
 query->wake=wake; query->user=user;
 if(!xrtFutureWatchInit(&query->watch,wakeQuery,NULL,query)) return 0;
 query->watchInitialized=1;
 xfuturewatchresult state=xrtFutureWatchAdd(query->future,&query->watch); if(state==XFUTURE_WATCH_ERROR) return 0;
 query->watchLinked=state==XFUTURE_WATCH_PENDING; if(state==XFUTURE_WATCH_READY) wake(user); return 1;
}
void xideDiagnosticQueryCancel(xide_diagnostic_query* query) { if(query) xrtFutureCancel(query->future); }
void xideDiagnosticQueryDestroy(xide_diagnostic_query* query)
{
 if(!query) return;
 if(query->watchLinked) xrtFutureWatchRemove(query->future,&query->watch);
 xrtFutureDestroy(query->future); xrtFree(query);
}
