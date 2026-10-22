#include "services/diagnostic_store.h"
#include "platform/path.h"
#include <xrt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define RECORD_LIMIT 64u
#define FILE_LIMIT 512u
#define GROUP_LIMIT 4096u
#define DIAGNOSTIC_LIMIT 4096u
typedef struct stored_group { wchar_t* key; xvalue* value; } stored_group;
typedef struct record { char* key; xide_language_input_guard* guard; xarray groups; size_t bytes; } record;
struct xide_diagnostic_store { xtaskpool* pool; record* records[RECORD_LIMIT]; xide_diagnostic_limits limits; };
struct xide_diagnostic_query { xfuture* future; xfuturewatch watch; int watchInitialized,watchLinked; void (*wake)(void*); void* user; };
enum operation { PUT,REMOVE,INVALIDATE,SWEEP,SNAPSHOT };
typedef struct input { xide_diagnostic_store* store; enum operation kind; char* key; xvalue* checked; xide_language_input_guard* guard; } input;
typedef struct output { xvalue *documents,*invalidated; size_t records,bytes; uint64_t thread; char error[256]; } output;
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
{ (void)user; input* value=data; xrtFree(value->key); xrtValueRelease(value->checked); xideLanguageInputGuardRelease(value->guard); xrtFree(value); }
static void freeOutput(void* data,void* user)
{ (void)user; output* value=data; xrtValueRelease(value->documents); xrtValueRelease(value->invalidated); xrtFree(value); }
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
static int makeSnapshot(record* const* records,size_t count,size_t limit,xcancel* cancel,output* out)
{
 xarray references={0},seen={0}; xmap hashes={0}; int okay=0; size_t publishedBytes=2;
 if(limit<publishedBytes) return fail(out,"Merged diagnostic report byte limit exceeded.");
 if(!xrtArrayInit(&references,sizeof(group_ref)) || !xrtArrayInit(&seen,sizeof(seen_diagnostic)) || !xrtMapInit(&hashes,sizeof(size_t))) { fail(out,"Cannot allocate diagnostic merge state."); goto done; }
 for(size_t i=0;i<count;++i) if(records[i]) {
  ++out->records; if(!addBytes(&out->bytes,records[i]->bytes)) { fail(out,"Diagnostic report size overflow."); goto done; }
  stored_group* groups=(stored_group*)records[i]->groups.Data;
  for(size_t j=0;j<records[i]->groups.Count;++j) { group_ref ref={&groups[j],i*(FILE_LIMIT+RECORD_LIMIT)+j}; if(!xrtArrayPush(&references,&ref)) { fail(out,"Cannot index diagnostic contributions."); goto done; } }
 }
 if(references.Count) qsort(references.Data,references.Count,sizeof(group_ref),compareGroup);
 out->documents=xrtValueArray(); if(!out->documents) { fail(out,"Cannot allocate merged diagnostics."); goto done; }
 group_ref* refs=(group_ref*)references.Data; xvalue* merged=NULL; const stored_group* previous=NULL; uint64 groupId=0;
 for(size_t i=0;i<references.Count;++i) {
  if(xrtCancelRequested(cancel)) { fail(out,"Diagnostic merge cancelled."); goto done; }
  stored_group* source=refs[i].group;
  if(!previous || comparePath(previous->key,source->key)) {
   if(++groupId>GROUP_LIMIT) { fail(out,"Merged diagnostic file limit exceeded."); goto done; }
   merged=emptyGroup(source->value); size_t bytes=merged ? encodedBytes(merged,limit-publishedBytes) : SIZE_MAX;
   if(bytes==SIZE_MAX || !addBytes(&bytes,groupId>1 ? 1 : 0) || bytes>limit-publishedBytes) { xrtValueRelease(merged); fail(out,"Merged diagnostic report byte limit exceeded."); goto done; }
   publishedBytes+=bytes;
   int appended=xrtValueArrayAppend(out->documents,merged); xrtValueRelease(merged);
   if(!appended) { fail(out,"Cannot retain merged diagnostic group."); goto done; }
   previous=source;
  } else if(!compatible(previous->value,source->value)) { fail(out,"Conflicting diagnostic input versions or fingerprints."); goto done; }
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
 okay=1;
done:
 xrtArrayUnit(&references); xrtArrayUnit(&seen); xrtMapUnit(&hashes); return okay;
}
static xtaskoutcome execute(xcancel* cancel,void* data,xtaskvalue* result)
{
 input* in=data; xide_diagnostic_store* store=in->store;
 if(xrtCancelRequested(cancel)) return XTASK_CANCELLED;
 output* out=xrtCalloc(1,sizeof(*out)); if(!out) return XTASK_FAILED;
 out->thread=xrtThreadCurrentId(); out->invalidated=xrtValueArray();
 record* trial[RECORD_LIMIT]; memcpy(trial,store->records,sizeof(trial)); record* fresh=NULL;
 if(!out->invalidated) { fail(out,"Cannot allocate diagnostic invalidation list."); goto done; }
 for(size_t i=0;i<store->limits.records;++i) if(trial[i]) {
  record* value=trial[i];
  if(in->kind==INVALIDATE && xideLanguageInputGuardTouches(value->guard,in->key)) xideLanguageInputGuardInvalidate(value->guard);
  if(in->kind==SWEEP) { xide_model_error error={0}; xideLanguageInputGuardValidate(value->guard,stopped,cancel,&error); }
  if(!xideLanguageInputGuardCurrent(value->guard) || (in->kind==REMOVE && !strcmp(value->key,in->key))) {
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
 if(!makeSnapshot(trial,store->limits.records,store->limits.publishedReportBytes,cancel,out)) goto done;
 if(xrtCancelRequested(cancel)) goto done;
 for(size_t i=0;i<store->limits.records;++i) if(store->records[i]!=trial[i]) { releaseRecord(store->records[i]); store->records[i]=trial[i]; }
 fresh=NULL;
done:
 releaseRecord(fresh);
 if(xrtCancelRequested(cancel)) { freeOutput(out,NULL); return XTASK_CANCELLED; }
 if(*out->error) { xrtValueRelease(out->documents); out->documents=NULL; xrtValueRelease(out->invalidated); out->invalidated=NULL; }
 *result=(xtaskvalue){out,freeOutput,NULL}; return XTASK_SUCCESS;
}
xide_diagnostic_store* xideDiagnosticStoreCreate(const xide_diagnostic_limits* limits)
{
 xide_diagnostic_limits chosen=limits ? *limits : (xide_diagnostic_limits){64,32u*1024u*1024u,4u*1024u*1024u,8};
 if(!chosen.records || chosen.records>RECORD_LIMIT || !chosen.retainedReportBytes || !chosen.publishedReportBytes || !chosen.queue || chosen.queue>64) return NULL;
 xide_diagnostic_store* store=xrtCalloc(1,sizeof(*store)); if(!store) return NULL; store->limits=chosen;
 xtaskpoolconfig config={.Threads=1,.QueueLimit=chosen.queue}; store->pool=xrtTaskPoolCreate(&config);
 if(!store->pool) { xideDiagnosticStoreDestroy(store); return NULL; } return store;
}
void xideDiagnosticStoreDestroy(xide_diagnostic_store* store)
{
 if(!store) return;
 if(store->pool) { xrtTaskPoolCancel(store->pool); xrtTaskPoolDestroy(store->pool); }
 for(size_t i=0;i<RECORD_LIMIT;++i) releaseRecord(store->records[i]);
 xrtFree(store);
}
int xideDiagnosticStoreLoad(xide_diagnostic_store* store,xide_diagnostic_load* load)
{
 if(!load) return 0;
 memset(load,0,sizeof(*load)); xtaskpoolstats stats={0};
 if(!store || !xrtTaskPoolGet(store->pool,&stats)) return 0;
 *load=(xide_diagnostic_load){stats.Queued,stats.Running,stats.Submitted,stats.Completed,stats.Cancelled,stats.Rejected}; return 1;
}
static xide_diagnostic_query* submit(xide_diagnostic_store* store,enum operation kind,const char* key,xide_language_input_guard* guard,xvalue* checked)
{
 if(!store || (kind==PUT && (!guard || !checked)) || ((kind==PUT || kind==REMOVE || kind==INVALIDATE) && (!key || !*key || strlen(key)>65535 || !xrtUtf8Valid(sv(key),NULL)))) { xrtValueRelease(checked); return NULL; }
 input* in=xrtCalloc(1,sizeof(*in)); xide_diagnostic_query* query=xrtCalloc(1,sizeof(*query));
 if(!in || !query) { xrtFree(in); xrtFree(query); xrtValueRelease(checked); return NULL; }
 in->store=store; in->kind=kind; in->checked=checked; in->guard=xideLanguageInputGuardRetain(guard); if(key) in->key=xrtStrDup(key);
 if(key && !in->key) { freeInput(in,NULL); xrtFree(query); return NULL; }
 xtaskargs args={.Destroy=freeInput}; query->future=xrtTaskSubmit(store->pool,execute,in,&args);
 if(!query->future) { freeInput(in,NULL); xrtFree(query); return NULL; } return query;
}
xide_diagnostic_query* xideDiagnosticStorePutTake(xide_diagnostic_store* store,const char* key,xide_language_input_guard* guard,xvalue* checked)
{ return submit(store,PUT,key,guard,checked); }
xide_diagnostic_query* xideDiagnosticStoreRemove(xide_diagnostic_store* store,const char* key) { return submit(store,REMOVE,key,NULL,NULL); }
xide_diagnostic_query* xideDiagnosticStoreInvalidate(xide_diagnostic_store* store,const char* path) { return submit(store,INVALIDATE,path,NULL,NULL); }
xide_diagnostic_query* xideDiagnosticStoreSweep(xide_diagnostic_store* store) { return submit(store,SWEEP,NULL,NULL,NULL); }
xide_diagnostic_query* xideDiagnosticStoreSnapshot(xide_diagnostic_store* store) { return submit(store,SNAPSHOT,NULL,NULL,NULL); }
enum xide_diagnostic_state xideDiagnosticQueryResult(xide_diagnostic_query* query,xide_diagnostic_snapshot* result)
{
 if(result) memset(result,0,sizeof(*result));
 if(!query || !result) return XIDE_DIAGNOSTIC_FAILED;
 xfuturestate state=xrtFutureState(query->future); if(state==XFUTURE_PENDING) return XIDE_DIAGNOSTIC_PENDING;
 if(state==XFUTURE_CANCELLED) return XIDE_DIAGNOSTIC_CANCELLED;
 const output* out=state==XFUTURE_RESOLVED ? xrtFutureValue(query->future) : NULL;
 if(!out) { snprintf(result->error,sizeof(result->error),"Diagnostic actor failed."); return XIDE_DIAGNOSTIC_FAILED; }
 *result=(xide_diagnostic_snapshot){out->documents,out->invalidated,out->records,out->bytes,out->thread,{0}};
 snprintf(result->error,sizeof(result->error),"%s",out->error); return *out->error ? XIDE_DIAGNOSTIC_FAILED : XIDE_DIAGNOSTIC_READY;
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
