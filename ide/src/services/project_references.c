#include "services/project_references.h"
#ifdef XLS_COMPILER_XRT
#include "platform/path.h"
#include "analysis/worker_modules.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <limits.h>
#define RECORDS 65538u
#define RETAINED_BYTES (128u*1024u*1024u)
#define RESULT_BYTES (32u*1024u*1024u)
/* A reference ID is at most64KiB. Even6-byte JSON escapes plus the JSON-RPC
 * envelope fit the reserved1MiB;reject oversized results before framing. */
#define RESULT_WIRE_BYTES (XIDE_LSP_BODY_BYTES-1024u*1024u)
#define PROOF_BYTES (128u*1024u*1024u)
#ifndef XIDE_REFERENCE_MEMBER_WORKERS
#define XIDE_REFERENCE_MEMBER_WORKERS 4u
#endif
_Static_assert(XIDE_REFERENCE_MEMBER_WORKERS>=1 && XIDE_REFERENCE_MEMBER_WORKERS<=4,"bounded member actors");
typedef struct path_slot {wchar_t* key;size_t index;} path_slot;
typedef struct path_index {path_slot* slots;size_t count;} path_index;
typedef struct reference_record {xatomic32 references;xls_context* scope;xide_reference_shard* shard;xide_language_input_guard* guard;char** overlays;path_index paths;wchar_t* key;size_t count,bytes;uint64_t used;} reference_record;
struct xide_project_references {xtaskpool* pool;xls_worker *analysis,*keys,*members[XIDE_REFERENCE_MEMBER_WORKERS];size_t memberCount;xide_toolchains* cache;char* compiler;xarray records;struct {xls_context* context;size_t count;} scopes[64];size_t bytes;uint64_t clock;};
struct xide_reference_query {xfuture* future;xfuturewatch watch;int initialized,linked;void (*wake)(void*);void* user;};
typedef struct reference_input {xide_project_references* store;xls_context* scope;xls_analysis_document docs[64];path_index paths;size_t count;char *trigger,*path;int line,character,include,proof;} reference_input;
typedef struct reference_output {xvalue* locations;xide_project_reference_stats stats;uint64_t memberThreads[XIDE_REFERENCE_MEMBER_WORKERS];char error[512];} reference_output;
typedef struct reference_job {const char* path;xevent* event;xide_language_check* check;reference_record* record;int fresh,sourceRejected;char error[512];} reference_job;
typedef struct root {char* path;wchar_t* key;int covered,mustAnalyze;char* rejection;reference_record* cached;} root;
typedef struct result_row {char* path;wchar_t* key;xide_lsp_position start,end;const xvalue* proof;} result_row;
static xstrview sv(const char* s){return (xstrview){s,strlen(s)};}
static const xvalue* get(const xvalue* v,const char* key){return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,sv(key)) : NULL;}
static const char* string(const xvalue* v){xstrview s={0};return v && xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) ? s.Data : NULL;}
static int put(xvalue* v,const char* key,xvalue* item){return item && xrtValueObjectSetNew(v,sv(key),item);}
static int same(const char* a,const char* b){return a && b && (!strcmp(a,b) || xidePathLexicalEqual(a,b));}
static int stopped(void* user){return xrtCancelRequested(user);}
static int fail(reference_output* out,const char* message){snprintf(out->error,sizeof(out->error),"%s",message);return 0;}
static int charge(size_t* bytes,size_t add,size_t limit){if(*bytes>limit || add>limit-*bytes)return 0;*bytes+=add;return 1;}
static void pathIndexUnit(path_index* index)
{for(size_t i=0;i<index->count;++i)free(index->slots[i].key);xrtFree(index->slots);*index=(path_index){0};}
static int comparePathSlot(const void* a,const void* b)
{const path_slot *x=a,*y=b;int order=xidePathNativeKeyCompare(x->key,y->key);return order ? order : (x->index>y->index)-(x->index<y->index);}
static int absolutePath(const char* path)
{return path && ((path[0] && (path[0]=='/' || path[0]=='\\') && path[1]==path[0]) || (((path[0]>='a' && path[0]<='z') || (path[0]>='A' && path[0]<='Z')) && path[1]==':' && (path[2]=='/' || path[2]=='\\')));}
static int pathIndexBuild(path_index* index,const char* const* paths,size_t count)
{
 if(!count || count>64)return 0;
 /* Retained relative keys would depend on a past CWD. Preserve the original
    comparator on relative/unsupported paths or any preparation failure. */
 for(size_t i=0;i<count;++i)if(!absolutePath(paths[i]))return 0;
 index->slots=xrtCalloc(count,sizeof(*index->slots));if(!index->slots)return 0;index->count=count;
 for(size_t i=0;i<count;++i){index->slots[i]=(path_slot){xidePathLexicalWide(paths[i]),i};if(!index->slots[i].key){pathIndexUnit(index);return 0;}}
 qsort(index->slots,count,sizeof(*index->slots),comparePathSlot);return 1;
}
static int pathIndexClone(path_index* index,const path_index* source)
{
 if(!source->count)return 0;
 index->slots=xrtCalloc(source->count,sizeof(*index->slots));if(!index->slots)return 0;index->count=source->count;
 for(size_t i=0;i<source->count;++i){size_t size=(wcslen(source->slots[i].key)+1)*sizeof(wchar_t);index->slots[i]=(path_slot){malloc(size),source->slots[i].index};if(!index->slots[i].key){pathIndexUnit(index);return 0;}memcpy(index->slots[i].key,source->slots[i].key,size);}return 1;
}
static int pathIndexFind(const path_index* index,const wchar_t* key)
{
 if(!key || !index->count)return -1;
 size_t lo=0,hi=index->count;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(xidePathNativeKeyCompare(index->slots[mid].key,key)<0)lo=mid+1;else hi=mid;}
 return lo<index->count && !xidePathNativeKeyCompare(index->slots[lo].key,key) ? (int)index->slots[lo].index : -1;
}
static reference_record* retainRecord(reference_record* r)
{if(r)xrtAtomic32FetchAdd(&r->references,1,XMEMORY_RELAXED);return r;}
static void releaseRecord(reference_record* r)
{if(!r || xrtAtomic32FetchSub(&r->references,1,XMEMORY_ACQ_REL)!=1)return;pathIndexUnit(&r->paths);if(r->overlays)for(size_t i=0;i<r->count;++i)xrtFree(r->overlays[i]);xrtFree(r->overlays);free(r->key);xlsContextRelease(r->scope);xideSemanticReferenceShardRelease(r->shard);xideLanguageInputGuardRelease(r->guard);xrtFree(r);}
static void freeInput(void* data,void* user)
{(void)user;reference_input* in=data;pathIndexUnit(&in->paths);for(size_t i=0;i<in->count;++i){xrtFree((char*)in->docs[i].key);xrtFree((char*)in->docs[i].path);xlsTextRelease(in->docs[i].text);}xlsContextRelease(in->scope);xrtFree(in->trigger);xrtFree(in->path);xrtFree(in);}
static void freeOutput(void* data,void* user){(void)user;reference_output* out=data;xrtValueRelease(out->locations);xrtFree(out);}
static int compareRoot(const void* a,const void* b){return xidePathNativeKeyCompare(((const root*)a)->key,((const root*)b)->key);}
static int compareRow(const void* a,const void* b)
{const result_row *x=a,*y=b;int c=xidePathNativeKeyCompare(x->key,y->key);if(!c)c=(x->start.line>y->start.line)-(x->start.line<y->start.line);if(!c)c=(x->start.character>y->start.character)-(x->start.character<y->start.character);if(!c)c=(x->end.line>y->end.line)-(x->end.line<y->end.line);return c ? c : (x->end.character>y->end.character)-(x->end.character<y->end.character);}
static int addRoot(xarray* roots,const char* directory,const char* path,size_t* bytes)
{
 if(!path || !xidePathHasExtension(path,".xl"))return 1;
 root r={.path=xidePathResolveLexical(directory,path)};if(r.path)r.key=xidePathNativeWide(r.path);
 if(!r.path || !r.key || !charge(bytes,strlen(r.path)+1+(wcslen(r.key)+1)*sizeof(wchar_t)+2*sizeof(root),PROOF_BYTES) || !xrtArrayPush(roots,&r)){free(r.path);free(r.key);return 0;}return 1;
}
static int hasOverlay(const reference_record* r,const char* path){for(size_t i=0;i<r->count;++i)if(same(r->overlays[i],path))return 1;return 0;}
static int currentDocument(const reference_input* in,const char* path){for(size_t i=0;i<in->count;++i)if(same(in->docs[i].path,path))return (int)i;return -1;}
static int indexedDocument(const reference_input* in,const char* path)
{wchar_t* key=in->paths.count ? xidePathLexicalWide(path) : NULL;if(!key)return currentDocument(in,path);int at=pathIndexFind(&in->paths,key);free(key);return at;}
static int overlayCurrent(const reference_record* r,const reference_input* in,xcancel* cancel)
{
 const xvalue* files=xideLanguageInputGuardFiles(r->guard);
 for(size_t i=0;i<xrtValueCount(files);++i){if(stopped(cancel))return 0;const xvalue* row=xrtValueArrayGet(files,i);const char *path=string(get(row,"path")),*origin=string(get(row,"origin"));if(!path || !origin)return 0;int at=indexedDocument(in,path),overlay=!strcmp(origin,"overlay");
  if(overlay!=(at>=0))return 0;
  if(overlay){int found=0;for(size_t j=0;j<xideSemanticReferenceShardSources(r->shard);++j){xide_semantic_source source;if(!xideSemanticReferenceShardSource(r->shard,j,&source))return 0;if(!same(source.path,path))continue;const char* text=xlsTextData(in->docs[at].text);size_t size=xlsTextSize(in->docs[at].text),bom=size>=3 && !memcmp(text,"\xef\xbb\xbf",3)?3:0;found=source.utf16Prefix==(int)(bom!=0) && strlen(source.text)==size-bom && !memcmp(source.text,text+bom,size-bom);break;}if(!found)return 0;}
 }
 /* Overlay topology can change a previously negative import candidate even
    though the candidate's disk existence is unchanged. Unused body edits do
    not invalidate a shard whose actual binding/existence reads are unchanged. */
 if(in->paths.count && r->paths.count){
  for(size_t i=0;i<in->paths.count;++i)if(pathIndexFind(&r->paths,in->paths.slots[i].key)<0 && xideLanguageInputGuardTouches(r->guard,in->docs[in->paths.slots[i].index].path))return 0;
  for(size_t i=0;i<r->paths.count;++i)if(pathIndexFind(&in->paths,r->paths.slots[i].key)<0 && xideLanguageInputGuardTouches(r->guard,r->overlays[r->paths.slots[i].index]))return 0;
 }else{
  for(size_t i=0;i<in->count;++i)if(!hasOverlay(r,in->docs[i].path) && xideLanguageInputGuardTouches(r->guard,in->docs[i].path))return 0;
  for(size_t i=0;i<r->count;++i)if(currentDocument(in,r->overlays[i])<0 && xideLanguageInputGuardTouches(r->guard,r->overlays[i]))return 0;
 }
 xide_model_error error={0};return xideLanguageInputGuardValidate(r->guard,stopped,cancel,&error);
}
static reference_record** records(xide_project_references* store){return (reference_record**)store->records.Data;}
static int recordOrder(const reference_record* r,uint64_t hash,const wchar_t* key)
{uint64_t other=xlsContextHash(r->scope);return other!=hash ? (other>hash)-(other<hash) : xidePathNativeKeyCompare(r->key,key);}
static size_t lowerRecord(xide_project_references* store,const xls_context* scope,const wchar_t* key)
{size_t lo=0,hi=store->records.Count;uint64_t hash=xlsContextHash(scope);while(lo<hi){size_t mid=lo+(hi-lo)/2;if(recordOrder(records(store)[mid],hash,key)<0)lo=mid+1;else hi=mid;}return lo;}
static void removeRecord(xide_project_references* store,size_t at)
{
 reference_record* r=records(store)[at];store->bytes-=r->bytes;
 for(size_t i=0;i<64;++i)if(store->scopes[i].context==r->scope){if(!--store->scopes[i].count){store->bytes-=xlsContextBytes(r->scope);store->scopes[i].context=NULL;}break;}
 xrtArrayRemove(&store->records,at,1);releaseRecord(r);
}
static reference_record* findRecord(xide_project_references* store,const reference_input* in,const root* target,xcancel* cancel)
{
 size_t at=lowerRecord(store,in->scope,target->key);uint64_t hash=xlsContextHash(in->scope);
 while(at<store->records.Count && !recordOrder(records(store)[at],hash,target->key)){reference_record* r=records(store)[at];
  if(!xlsContextEqual(r->scope,in->scope)){++at;continue;}
  if(overlayCurrent(r,in,cancel)){r->used=++store->clock;return r;}
  if(!stopped(cancel))removeRecord(store,at);
  return NULL;
 }return NULL;
}
static size_t scopeSlot(xide_project_references* store,const xls_context* context,size_t* empty)
{*empty=64;for(size_t i=0;i<64;++i){if(!store->scopes[i].context){if(*empty==64)*empty=i;}else if(xlsContextEqual(store->scopes[i].context,context))return i;}return 64;}
static int cacheRecord(xide_project_references* store,reference_record* r)
{
 const size_t limit=RETAINED_BYTES-2u*RECORDS*sizeof(reference_record*)-sizeof(*store);
 if(r->bytes>limit || xlsContextBytes(r->scope)>limit-r->bytes)return 0;
 size_t at=lowerRecord(store,r->scope,r->key);uint64_t hash=xlsContextHash(r->scope);
 while(at<store->records.Count && !recordOrder(records(store)[at],hash,r->key)){if(xlsContextEqual(records(store)[at]->scope,r->scope)){removeRecord(store,at);break;}++at;}
 size_t empty,scope=scopeSlot(store,r->scope,&empty);
 while(store->records.Count>=RECORDS || (scope==64 && empty==64) || store->bytes>limit-r->bytes-(scope==64 ? xlsContextBytes(r->scope) : 0)){
  if(!store->records.Count)return 0;
  size_t oldest=0;for(size_t i=1;i<store->records.Count;++i)if(records(store)[i]->used<records(store)[oldest]->used)oldest=i;
  if(scope==64 && empty==64){xls_context* discard=xlsContextRetain(records(store)[oldest]->scope);for(size_t i=store->records.Count;i--;)if(records(store)[i]->scope==discard)removeRecord(store,i);xlsContextRelease(discard);}else removeRecord(store,oldest);
  scope=scopeSlot(store,r->scope,&empty);
 }
 if(scope!=64){xls_context* canonical=xlsContextRetain(store->scopes[scope].context);xlsContextRelease(r->scope);r->scope=canonical;}
 at=lowerRecord(store,r->scope,r->key);if(!xrtArrayInsert(&store->records,at,&r,1))return 0;
 retainRecord(r);
 if(scope==64){scope=empty;store->scopes[scope].context=r->scope;store->bytes+=xlsContextBytes(r->scope);}++store->scopes[scope].count;store->bytes+=r->bytes;r->used=++store->clock;return 1;
}
static void childWake(void* user){xrtEventSet(user);}
static void closeJob(reference_job* job)
{
 xideLanguageCheckDestroy(job->check);if(job->event)xrtEventDestroy(job->event);
 job->check=NULL;job->event=NULL;
}
static void clearJobs(reference_job* jobs,size_t count)
{
 /* Signal every outstanding child before joining any one child. Warm shard
    pins survive cache eviction while an earlier ordered result is committed. */
 for(size_t i=0;i<count;++i)if(jobs[i].check)xideLanguageCheckCancel(jobs[i].check);
 for(size_t i=0;i<count;++i){closeJob(jobs+i);releaseRecord(jobs[i].record);jobs[i]=(reference_job){0};}
}
static int beginAnalysis(reference_input* in,const char* path,int keyWanted,xls_worker* actor,reference_job* job)
{
 /* Membership remains in the scheduler's exact retained scope. It is index
    metadata,not an implicit compiler import;keep it out of every child copy.
    Every semantic option and the project/configuration identity are retained. */
 job->path=path;job->fresh=1;
 xls_context_spec spec=*xlsContextSpec(in->scope);spec.entry=path;spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){0};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);job->event=xrtEventCreate(false,false);xide_model_error error={0};xide_language_analysis_options options={1,childWake,job->event};
 job->check=context && job->event ? xideLanguageCheckStartReferenceCapture(context,in->store->cache,in->store->compiler,actor,in->docs,in->count,in->trigger,keyWanted?in->path:NULL,in->line,in->character,&options,&error) : NULL;xlsContextRelease(context);
 if(!job->check)snprintf(job->error,sizeof(job->error),"Cannot start a configured member reference analysis.");
 return job->check!=NULL;
}
static reference_record* finishAnalysis(reference_input* in,reference_job* job,int keyWanted,xcancel* cancel,xide_reference_key** key,reference_output* out)
{
 reference_record* r=NULL;xide_language_check* check=job->check;xide_model_error error={0};
 xcancelwatch* wake=check && cancel ? xrtCancelWatch(cancel,childWake,job->event) : NULL;
 if(!check){fail(out,job->error);goto done;}
 while(xideLanguageCheckState(check,NULL,0)==XIDE_CHECK_RUNNING && !stopped(cancel))xrtEventWaitFor(job->event,10000);
 if(stopped(cancel)){xideLanguageCheckCancel(check);goto done;}
 r=xrtCalloc(1,sizeof(*r));if(r)xrtAtomic32Init(&r->references,1);xide_reference_key* captured=NULL;
 if(!r || !xideLanguageCheckCaptureReferences(check,&r->shard,&captured)){job->sourceRejected=r && !keyWanted && xideLanguageCheckReferenceSourceRejected(check);fail(out,"A configured member did not produce complete verified reference facts.");goto failed;}
 if(keyWanted){xls_check_stats work;xls_worker_cache_stats sessions;xide_navigation_work navigation;
  if(!xideLanguageCheckReferenceWork(check,&work,&sessions,&navigation)){xideSemanticReferenceKeyRelease(captured);fail(out,"Cannot capture verified key worker work.");goto failed;}
  out->stats.keyActorBuilds=work.builds;out->stats.keyActorHits=work.hits;out->stats.keyActorSessions=sessions.sessions;
  out->stats.keyNavigationBuilds=(size_t)navigation.built;out->stats.keyNavigationHits=(size_t)navigation.hit;
 }
 if(keyWanted)*key=captured;else xideSemanticReferenceKeyRelease(captured);
 if(!keyWanted){uint64_t thread=xideLanguageCheckReferenceThread(check);int known=0;for(size_t i=0;i<out->stats.memberThreads;++i)known|=out->memberThreads[i]==thread;
  if(thread && !known && out->stats.memberThreads<XIDE_REFERENCE_MEMBER_WORKERS)out->memberThreads[out->stats.memberThreads++]=thread;
 }
 r->guard=xideLanguageCheckCaptureInputGuard(check,&error);r->scope=xlsContextRetain(in->scope);r->key=xidePathNativeWide(job->path);r->count=in->count;r->overlays=xrtCalloc(r->count,sizeof(char*));r->bytes=sizeof(*r)+r->count*sizeof(char*);int okay=r->guard && r->overlays && r->key && charge(&r->bytes,(wcslen(r->key)+1)*sizeof(wchar_t),SIZE_MAX) && charge(&r->bytes,xideSemanticReferenceShardBytes(r->shard),SIZE_MAX) && charge(&r->bytes,xideLanguageInputGuardReportBytes(r->guard),SIZE_MAX);
 for(size_t i=0;okay && i<r->count;++i){r->overlays[i]=xrtStrDup(in->docs[i].path);okay=r->overlays[i] && charge(&r->bytes,strlen(r->overlays[i])+1,SIZE_MAX);}
 pathIndexClone(&r->paths,&in->paths);
 okay=okay && charge(&r->bytes,r->paths.count*sizeof(path_slot),SIZE_MAX);
 for(size_t i=0;okay && i<r->paths.count;++i)okay=charge(&r->bytes,(wcslen(r->paths.slots[i].key)+1)*sizeof(wchar_t),SIZE_MAX);
 if(!okay || !overlayCurrent(r,in,cancel)){fail(out,"Member input proofs changed during reference capture.");goto failed;}++out->stats.captured;goto done;
failed:releaseRecord(r);r=NULL;
done:if(wake)xrtCancelUnwatch(wake);closeJob(job);return r;
}
static bool countProof(xbytesview data,void* user){return charge(user,data.Size,PROOF_BYTES);}
static const xvalue* sourceProof(reference_input* in,const reference_record* r,const char* path,xmap* known,xarray* proofs,size_t* proofBytes)
{
 bool added=false;size_t* index=xrtMapGetOrAdd(known,(xbytesview){(const unsigned char*)path,strlen(path)},&added);if(!index)return NULL;
 if(!added)return *index<proofs->Count ? ((xvalue**)proofs->Data)[*index] : NULL;
 *index=SIZE_MAX;const xvalue* input=NULL;const xvalue* files=xideLanguageInputGuardFiles(r->guard);
 for(size_t i=0;i<xrtValueCount(files);++i){const xvalue* file=xrtValueArrayGet(files,i);if(same(string(get(file,"path")),path)){input=file;break;}}
 const char* origin=string(get(input,"origin"));if(!origin)return NULL;
 int overlay=!strcmp(origin,"overlay"),at=indexedDocument(in,path);if(overlay!=(at>=0) || (!overlay && strcmp(origin,"disk")))return NULL;
 if(overlay && in->docs[at].revision>UINT_MAX)return NULL;
 xvalue* document=NULL;
 for(size_t i=0;i<xideSemanticReferenceShardSources(r->shard);++i){xide_semantic_source source;if(!xideSemanticReferenceShardSource(r->shard,i,&source))return NULL;if(same(source.path,path)){document=xideLanguageReferenceDocument(&source,overlay?(int)((int64_t)in->docs[at].revision+INT_MIN):0,overlay?NULL:input);break;}}
 xvalue* proof=xrtValueObject();int okay=document && proof && put(proof,"schema",xrtValueInt(1));
 if(okay){okay=put(proof,"document",document);document=NULL;}xrtValueRelease(document);
 xjsonwriteconfig config;xrtJsonWriteConfigInit(&config);config.MaxOutputBytes=PROOF_BYTES;
 okay=okay && charge(proofBytes,strlen(path)*2+4*sizeof(void*),PROOF_BYTES) && xrtJsonWrite(proof,&config,countProof,proofBytes);
 if(!okay || !xrtArrayPush(proofs,&proof)){xrtValueRelease(proof);return NULL;}*index=proofs->Count-1;return proof;
}
static int appendRows(reference_input* in,xarray* rows,const reference_record* r,const xide_reference_key* key,int include,size_t* textBytes,xmap* known,xarray* proofs,size_t* proofBytes,reference_output* out)
{
 xide_reference_shard_view view;enum xide_reference_match match=xideSemanticReferenceShardQuery(r->shard,key,include,&view);if(match!=XIDE_REFERENCE_MATCH)return fail(out,"Declaration source changed between independently analyzed members.");
 for(size_t i=0;i<view.count;++i){xide_semantic_reference at;if(!xideSemanticReferenceShardAt(&view,i,&at))return fail(out,"Invalid captured reference range.");result_row row={.path=xrtStrDup(at.path),.key=xidePathNativeWide(at.path),.start=at.start,.end=at.end};if(in->proof)row.proof=sourceProof(in,r,at.path,known,proofs,proofBytes);if(!row.path || !row.key || (in->proof && !row.proof)){xrtFree(row.path);free(row.key);return fail(out,"Cannot retain verified reference locations.");}size_t add=strlen(row.path)+1+(wcslen(row.key)+1)*sizeof(wchar_t);
  if(!charge(textBytes,add,RESULT_BYTES) || rows->Count>=RESULT_BYTES/(2*sizeof(result_row)) || !xrtArrayPush(rows,&row)){xrtFree(row.path);free(row.key);return fail(out,"Reference location budget exceeded.");}
  if(rows->Capacity>RESULT_BYTES/sizeof(result_row) || *textBytes>RESULT_BYTES-rows->Capacity*sizeof(result_row))return fail(out,"Reference location storage budget exceeded.");
 }return 1;
}
static xvalue* wirePosition(xide_lsp_position p){xvalue* v=xrtValueObject();if(v && put(v,"line",xrtValueInt(p.line)) && put(v,"character",xrtValueInt(p.character)))return v;xrtValueRelease(v);return NULL;}
static bool countWire(xbytesview data,void* user){size_t* size=user;return charge(size,data.Size,RESULT_WIRE_BYTES);}
static int locations(xarray* rows,reference_output* out)
{
 if(!xrtArraySort(rows,compareRow))return 0;
 out->locations=xrtValueArray();if(!out->locations)return 0;result_row* items=(result_row*)rows->Data;
 for(size_t i=0;i<rows->Count;++i){if(i && !compareRow(items+i-1,items+i))continue;char* uri=xideLspPathToUri(items[i].path,NULL);xvalue *location=xrtValueObject(),*range=xrtValueObject();int okay=uri && location && range && put(range,"start",wirePosition(items[i].start)) && put(range,"end",wirePosition(items[i].end));
  if(okay){okay=put(location,"range",range);range=NULL;}if(okay)okay=put(location,"uri",xrtValueString(sv(uri)));if(okay && items[i].proof)okay=xrtValueObjectSet(location,sv("xlang"),items[i].proof);if(okay){okay=xrtValueArrayAppendNew(out->locations,location);location=NULL;}xrtValueRelease(location);xrtValueRelease(range);free(uri);if(!okay)return 0;
 }
 xjsonwriteconfig config;xrtJsonWriteConfigInit(&config);config.MaxOutputBytes=RESULT_WIRE_BYTES;size_t bytes=0;out->stats.locations=xrtValueCount(out->locations);return xrtJsonWrite(out->locations,&config,countWire,&bytes);
}

static int ownsSource(const reference_record* record,const char* path)
{
 for(size_t i=0;i<xideSemanticReferenceShardSources(record->shard);++i){xide_semantic_source source;if(!xideSemanticReferenceShardSource(record->shard,i,&source))return -1;if((source.kind&1) && same(source.path,path))return 1;}return 0;
}
static int coverSources(root* roots,size_t count,const reference_record* record,const char* primary)
{
 for(size_t i=0;i<xideSemanticReferenceShardSources(record->shard);++i){
  xide_semantic_source source;if(!xideSemanticReferenceShardSource(record->shard,i,&source))return 0;if(!(source.kind&1))continue;
  wchar_t* key=xidePathNativeWide(source.path);if(!key)return 0;size_t lo=0,hi=count;
  while(lo<hi){size_t mid=lo+(hi-lo)/2;if(xidePathNativeKeyCompare(roots[mid].key,key)<0)lo=mid+1;else hi=mid;}
  if(lo<count && !xidePathNativeKeyCompare(roots[lo].key,key) && (!same(roots[lo].path,primary) || !xidePathNativeKeyCompare(record->key,key)))roots[lo].covered=1;
  free(key);
 }return 1;
}

/* Query-local pins of already checked,currently validated compilation owners.
 * Select providers in deterministic root order. A selected provider must still
 * contribute its full shard:later coverage never suppresses it,including a
 * possible import cycle. Covered libraries need no repeated program rejection.
 * Initial key coverage precedes this scan;no member scan for no-symbol queries.
 * Every pinned provider is scheduled and enters the original final guard list.
 * Pins can retain the previous bounded shard payload while newer cache entries
 * are admitted;this is extra query lifetime,not a global/RSS memory limit. */
static int planCachedOwners(reference_input* in,root* all,size_t count,size_t requested,xcancel* cancel,reference_output* out)
{
 all[requested].mustAnalyze=1;
 for(size_t i=0;i<count;++i){
  if(stopped(cancel))return 0;
  if(i==requested || all[i].covered || (i && !compareRoot(all+i-1,all+i)))continue;
  all[i].cached=retainRecord(findRecord(in->store,in,all+i,cancel));
  if(!all[i].cached)continue;
  all[i].mustAnalyze=1;++out->stats.coverageCacheRoots;
  if(!coverSources(all,count,all[i].cached,xlsContextSpec(in->scope)->entry))return 0;
 }
 return !stopped(cancel);
}
typedef struct owner_proof {xls_dependency_capture* dependency;xide_language_input_guard* guard;} owner_proof;
static int ownerProofValid(owner_proof* proof,xcancel* cancel)
{xide_model_error error={0};return !stopped(cancel) && (!proof->dependency || xlsDependencyCaptureValidate(proof->dependency)) && (!proof->guard || xideLanguageInputGuardValidate(proof->guard,stopped,cancel,&error));}
static int probeOwner(reference_input* in,const root* candidate,xcancel* cancel,owner_proof* proof,reference_output* out)
{
 reference_record* cached=findRecord(in->store,in,candidate,cancel);
 if(cached){int owns=ownsSource(cached,in->path);if(owns<0){fail(out,"Invalid cached owner source vector.");return -1;}++out->stats.ownerRouteHits;if(owns)proof->guard=xideLanguageInputGuardRetain(cached->guard);return owns;}
 if(stopped(cancel))return -1;
 xls_context_spec spec=*xlsContextSpec(in->scope);spec.entry=candidate->path;spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){0};
 xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xevent* event=xrtEventCreate(false,false);
 xls_query* query=context && event ? xlsWorkerDependenciesInContext(in->store->keys,context,in->docs,in->count,NULL) : NULL;xlsContextRelease(context);
 if(!query){if(event)xrtEventDestroy(event);fail(out,"Cannot start original dependency owner capture.");return -1;}
 ++out->stats.ownerProbes;
 int linked=xlsQuerySetWake(query,childWake,event);xcancelwatch* wake=cancel ? xrtCancelWatch(cancel,childWake,event) : NULL;
 xls_dependency_capture* value=NULL;xls_dependency_session_stats stats={0};char error[256]={0};enum xls_query_state state;
 while((state=xlsQueryDependencyResult(query,&value,&stats,NULL,error,sizeof(error)))==XLS_QUERY_PENDING && !stopped(cancel))xrtEventWaitFor(event,10000);
 if(stopped(cancel))xlsQueryCancel(query);
 if(wake)xrtCancelUnwatch(wake);
 xlsQueryDestroy(query);xrtEventDestroy(event);
 if(!linked || stopped(cancel) || state!=XLS_QUERY_READY || !value || !xlsDependencyCaptureInfo(value).expanded){xlsDependencyCaptureRelease(value);if(!stopped(cancel))fail(out,*error ? error : "Dependency owner expansion is incomplete.");return -1;}
 out->stats.ownerActorBuilds=(size_t)stats.builds;out->stats.ownerActorHits=(size_t)stats.hits;
 xls_check_inputs inputs;int owns=0;
 if(!xlsDependencyCaptureInputs(value,&inputs)){xlsDependencyCaptureRelease(value);fail(out,"Missing original dependency owner inputs.");return -1;}
 for(size_t i=0;i<inputs.sources;++i){xls_check_source source;if(!xlsDependencyCaptureSource(value,i,&source)){xlsDependencyCaptureRelease(value);fail(out,"Invalid original dependency owner source.");return -1;}if((source.kind&1) && same(source.path,in->path))owns=1;}
 if(owns)proof->dependency=value;else xlsDependencyCaptureRelease(value);return owns;
}
static int selectOwner(reference_input* in,root* roots,size_t count,size_t* requested,xcancel* cancel,owner_proof* proof,reference_output* out)
{
 const xls_context_spec* scope=xlsContextSpec(in->scope);if(scope->kind!=XLS_CONTEXT_PROJECT || same(in->path,scope->entry))return *requested!=SIZE_MAX;
 size_t primary=SIZE_MAX;for(size_t i=0;i<count;++i)if(same(roots[i].path,scope->entry)){primary=i;break;}
 if(primary==SIZE_MAX)return fail(out,"Configured entry is absent from reference roots.");
 int owns=probeOwner(in,roots+primary,cancel,proof,out);if(owns<0)return 0;if(owns){*requested=primary;return 1;}
 for(size_t i=0;i<count;++i){if(i==primary || same(roots[i].path,in->path) || (i && !compareRoot(roots+i-1,roots+i)))continue;
  owns=probeOwner(in,roots+i,cancel,proof,out);if(owns<0)return 0;if(owns){*requested=i;return 1;}
 }
 return *requested!=SIZE_MAX || fail(out,"No configured compilation owns the requested dependency source.");
}
static xtaskoutcome execute(xcancel* cancel,void* data,xtaskvalue* value)
{
 reference_input* in=data;reference_output* out=xrtCalloc(1,sizeof(*out));if(!out)return XTASK_FAILED;out->stats.thread=xrtThreadCurrentId();out->stats.memberWorkers=in->store->memberCount;xarray roots={0},rows={0},guards={0},proofs={0};xmap known={0};size_t rootBytes=0,rowBytes=0,proofBytes=0;xide_reference_key* key=NULL;reference_job jobs[XIDE_REFERENCE_MEMBER_WORKERS]={0};owner_proof ownerProof={0};
 const char* paths[64];for(size_t i=0;i<in->count;++i)paths[i]=in->docs[i].path;pathIndexBuild(&in->paths,paths,in->count);
 if(!xrtArrayInit(&roots,sizeof(root)) || !xrtArrayInit(&rows,sizeof(result_row)) || !xrtArrayInit(&guards,sizeof(xide_language_input_guard*)) || !xrtArrayInit(&proofs,sizeof(xvalue*)) || !xrtMapInit(&known,sizeof(size_t)))goto failed;
 const xls_context_spec* scope=xlsContextSpec(in->scope);if((scope->kind!=XLS_CONTEXT_PROJECT && !addRoot(&roots,scope->directory,in->path,&rootBytes)) || !addRoot(&roots,scope->directory,scope->entry,&rootBytes))goto failed;
 for(size_t i=0;i<scope->lists[XLS_CONTEXT_MEMBERS].count;++i){if(stopped(cancel))goto done;if(!addRoot(&roots,scope->directory,scope->lists[XLS_CONTEXT_MEMBERS].items[i],&rootBytes))goto failed;}
 if(!xrtArraySort(&roots,compareRoot))goto failed;
 /* Deduplicate aliases before analysis. Keep roots separately owned until
    cleanup,so every construction/cancellation path has the same destructor. */
 root* all=(root*)roots.Data;size_t requested=SIZE_MAX;for(size_t i=0;i<roots.Count;++i)if(same(all[i].path,in->path)){requested=i;break;}if(!selectOwner(in,all,roots.Count,&requested,cancel,&ownerProof,out))goto done;
 /* Resolve the requested binding first on its dedicated actor. No-symbol
    queries retain the original early-empty behavior and schedule no members. */
 if(stopped(cancel))goto done;
 beginAnalysis(in,all[requested].path,1,in->store->keys,jobs);
 jobs[0].record=finishAnalysis(in,jobs,1,cancel,&key,out);if(!jobs[0].record)goto done;
 ++out->stats.roots;if(ownsSource(jobs[0].record,in->path)!=1){fail(out,"Selected compilation no longer owns the requested source.");goto done;}if(!key){out->locations=xrtValueArray();if(!out->locations)goto failed;goto done;}
 if(!coverSources(all,roots.Count,jobs[0].record,scope->entry) || !planCachedOwners(in,all,roots.Count,requested,cancel,out))goto done;
 for(size_t next=0,active=1;;){
  for(size_t slot=0;slot<active;++slot){
   if(stopped(cancel))goto done;
   if(!jobs[slot].record){jobs[slot].record=finishAnalysis(in,jobs+slot,0,cancel,NULL,out);if(!jobs[slot].record){
    if(!jobs[slot].sourceRejected || stopped(cancel))goto done;
    root* rejected=NULL;for(size_t i=0;i<roots.Count;++i)if(same(all[i].path,jobs[slot].path)){rejected=all+i;break;}
    if(!rejected || !charge(&rootBytes,strlen(out->error)+1,PROOF_BYTES) || !(rejected->rejection=xrtStrDup(out->error)))goto failed;
    rejected->mustAnalyze=0;++out->stats.sourceRejections;*out->error=0;continue;
   }++out->stats.roots;}
   else if(!jobs[slot].fresh){++out->stats.reused;++out->stats.roots;}
   reference_record* record=jobs[slot].record;
   size_t charged=xideLanguageInputGuardReportBytes(record->guard);if(charged==SIZE_MAX || !charge(&proofBytes,charged,PROOF_BYTES))goto failed;
   xide_language_input_guard* guard=xideLanguageInputGuardRetain(record->guard);if(!xrtArrayPush(&guards,&guard)){xideLanguageInputGuardRelease(guard);goto failed;}
   if(!appendRows(in,&rows,record,key,in->include,&rowBytes,&known,&proofs,&proofBytes,out))goto done;
   if(!coverSources(all,roots.Count,record,scope->entry))goto failed;
   if(jobs[slot].fresh)cacheRecord(in->store,record);
  }
  clearJobs(jobs,XIDE_REFERENCE_MEMBER_WORKERS);active=0;size_t pending=0;
  while(active<in->store->memberCount && next<roots.Count){
   size_t i=next++;if(i==requested || (all[i].covered && !all[i].mustAnalyze) || (i && !compareRoot(all+i-1,all+i))){if(i!=requested && all[i].covered && !all[i].mustAnalyze)++out->stats.coveredRoots;continue;}
   if(stopped(cancel))goto done;
   reference_job* job=jobs+active;job->path=all[i].path;
   job->record=retainRecord(all[i].cached);all[i].mustAnalyze=1;
   if(!job->record){
    int started=beginAnalysis(in,job->path,0,in->store->members[active],job);
    if(started){++pending;++out->stats.memberTasks;}
    ++active;if(!started)break;
   }else ++active;
  }
  if(pending>out->stats.memberPeak)out->stats.memberPeak=pending;
  if(!active)break;
  /* The next iteration captures and commits each result in original root
     order. A later failure cannot replace an earlier error or discard its
     already admitted prefix;warm pins survive that prefix's evictions. */
 }
 for(size_t i=0;i<roots.Count;++i)if(all[i].rejection && !all[i].covered){fail(out,all[i].rejection);goto done;}
 for(size_t i=0;i<guards.Count;++i){xide_model_error error={0};if(!xideLanguageInputGuardValidate(((xide_language_input_guard**)guards.Data)[i],stopped,cancel,&error)){fail(out,"Project source/resolution inputs changed before publication.");goto done;}}
 if(!locations(&rows,out))goto failed;
 goto done;
failed:fail(out,"Project reference allocation or query budget failed.");
done:
 /* Preserve original ordered failure precedence without turning any later
  * infrastructure failure into success. Only an uncovered earlier source
  * rejection supplies the message;all such paths still return RequestFailed. */
 if(*out->error)for(size_t i=0;i<roots.Count;++i){root* r=(root*)roots.Data+i;if(r->rejection && !r->covered){fail(out,r->rejection);break;}}
 if(out->locations && !*out->error && !ownerProofValid(&ownerProof,cancel))fail(out,"Compilation owner inputs changed before publication.");
 xlsDependencyCaptureRelease(ownerProof.dependency);xideLanguageInputGuardRelease(ownerProof.guard);
 clearJobs(jobs,XIDE_REFERENCE_MEMBER_WORKERS);xideSemanticReferenceKeyRelease(key);
 for(size_t i=0;i<roots.Count;++i){root* r=(root*)roots.Data+i;free(r->path);free(r->key);xrtFree(r->rejection);releaseRecord(r->cached);}xrtArrayUnit(&roots);
 for(size_t i=0;i<rows.Count;++i){result_row* r=(result_row*)rows.Data+i;xrtFree(r->path);free(r->key);}xrtArrayUnit(&rows);
 for(size_t i=0;i<guards.Count;++i)xideLanguageInputGuardRelease(((xide_language_input_guard**)guards.Data)[i]);
 xrtArrayUnit(&guards);
 for(size_t i=0;i<proofs.Count;++i)xrtValueRelease(((xvalue**)proofs.Data)[i]);
 xrtArrayUnit(&proofs);xrtMapUnit(&known);
 out->stats.chargedBytes=in->store->bytes+in->store->records.Capacity*sizeof(reference_record*)+sizeof(*in->store);out->stats.records=in->store->records.Count;
 if(stopped(cancel)){freeOutput(out,NULL);return XTASK_CANCELLED;}
 if(*out->error){xrtValueRelease(out->locations);out->locations=NULL;}*value=(xtaskvalue){out,freeOutput,NULL};return XTASK_SUCCESS;
}
xide_project_references* xideProjectReferencesCreate(xide_toolchains* cache,const char* compiler,xls_module_store* modules)
{
 if(!cache || !compiler)return NULL;
 xide_project_references* store=xrtCalloc(1,sizeof(*store));if(!store)return NULL;store->cache=cache;store->compiler=xrtStrDup(compiler);
 /* Split the original eight sessions/64MiB known input budget across at
    most four member actors. Additional actor allocation is optional;the
    first one remains required. Active result/transient memory is extra. */
 xls_worker_cache_limits memberLimits={8u/XIDE_REFERENCE_MEMBER_WORKERS,(64u*1024u*1024u)/XIDE_REFERENCE_MEMBER_WORKERS};
 for(size_t i=0;i<XIDE_REFERENCE_MEMBER_WORKERS;++i){xls_worker* worker=xlsWorkerCreateWithModules(NULL,8,&memberLimits,modules);if(!worker)break;store->members[store->memberCount++]=worker;}
 store->analysis=store->members[0];
 /* Member traversal must not evict the requested entry or replace its typed
    navigation table. Separate bounded workers share neutral module assets. */
 xls_worker_cache_limits keyLimits={2,64u*1024u*1024u};store->keys=xlsWorkerCreateWithModules(NULL,8,&keyLimits,modules);
 xtaskpoolconfig config={.Threads=1,.QueueLimit=8};store->pool=xrtTaskPoolCreate(&config);int initialized=xrtArrayInit(&store->records,sizeof(reference_record*));if(!initialized || !store->compiler || !store->analysis || !store->keys || !store->pool){xideProjectReferencesDestroy(store);return NULL;}return store;
}
void xideProjectReferencesDestroy(xide_project_references* store)
{if(!store)return;if(store->pool){xrtTaskPoolCancel(store->pool);xrtTaskPoolDestroy(store->pool);}for(size_t i=0;i<store->records.Count;++i)releaseRecord(records(store)[i]);xrtArrayUnit(&store->records);for(size_t i=0;i<store->memberCount;++i)xlsWorkerDestroy(store->members[i]);xlsWorkerDestroy(store->keys);xrtFree(store->compiler);xrtFree(store);}
xide_reference_query* xideProjectReferencesSubmitWithProof(xide_project_references* store,xls_context* context,const xls_analysis_document* docs,size_t count,const char* trigger,const char* path,int line,int character,int include,int proof)
{
 if(!store || !context || !docs || !count || count>64 || !trigger || !path || line<0 || character<0 || (include!=0 && include!=1) || (proof!=0 && proof!=1))return NULL;
 reference_input* in=xrtCalloc(1,sizeof(*in));xide_reference_query* q=xrtCalloc(1,sizeof(*q));if(!in || !q){xrtFree(in);xrtFree(q);return NULL;}in->store=store;in->scope=xlsContextRetain(context);in->trigger=xrtStrDup(trigger);in->path=xrtStrDup(path);in->line=line;in->character=character;in->include=include;in->proof=proof;int okay=in->trigger && in->path;
 for(size_t i=0;okay && i<count;++i){in->docs[i]=docs[i];in->docs[i].key=docs[i].key ? xrtStrDup(docs[i].key) : NULL;in->docs[i].path=docs[i].path ? xrtStrDup(docs[i].path) : NULL;in->docs[i].text=xlsTextRetain(docs[i].text);++in->count;okay=in->docs[i].key && in->docs[i].path && in->docs[i].text;}
 xtaskargs args={.Destroy=freeInput};if(okay)q->future=xrtTaskSubmit(store->pool,execute,in,&args);if(!q->future){freeInput(in,NULL);xrtFree(q);return NULL;}return q;
}
xide_reference_query* xideProjectReferencesSubmit(xide_project_references* store,xls_context* context,const xls_analysis_document* docs,size_t count,const char* trigger,const char* path,int line,int character,int include)
{return xideProjectReferencesSubmitWithProof(store,context,docs,count,trigger,path,line,character,include,0);}
enum xide_reference_query_state xideProjectReferencesResult(xide_reference_query* q,const xvalue** locationsOut,xide_project_reference_stats* stats,char* error,size_t capacity)
{
 if(locationsOut)*locationsOut=NULL;
 if(stats)*stats=(xide_project_reference_stats){0};
 if(!q)return XIDE_REFERENCES_FAILED;
 xfuturestate state=xrtFutureState(q->future);if(state==XFUTURE_PENDING)return XIDE_REFERENCES_PENDING;if(state==XFUTURE_CANCELLED)return XIDE_REFERENCES_CANCELLED;
 const reference_output* out=state==XFUTURE_RESOLVED ? xrtFutureValue(q->future) : NULL;if(!out || *out->error || !out->locations){if(error && capacity)snprintf(error,capacity,"%s",out && *out->error ? out->error : "Project reference task failed.");return XIDE_REFERENCES_FAILED;}if(locationsOut)*locationsOut=out->locations;if(stats)*stats=out->stats;return XIDE_REFERENCES_READY;
}
xvalue* xideProjectReferencesTake(xide_reference_query* q)
{if(xideProjectReferencesResult(q,NULL,NULL,NULL,0)!=XIDE_REFERENCES_READY)return NULL;reference_output* out=xrtFutureValue(q->future);xvalue* v=out->locations;out->locations=NULL;return v;}
static void wakeQuery(void* user){xide_reference_query* q=user;q->wake(q->user);}
int xideProjectReferencesSetWake(xide_reference_query* q,void (*wake)(void*),void* user)
{if(!q || !wake || q->initialized)return 0;q->wake=wake;q->user=user;if(!xrtFutureWatchInit(&q->watch,wakeQuery,NULL,q))return 0;q->initialized=1;xfuturewatchresult result=xrtFutureWatchAdd(q->future,&q->watch);if(result==XFUTURE_WATCH_ERROR)return 0;q->linked=result==XFUTURE_WATCH_PENDING;if(result==XFUTURE_WATCH_READY)wake(user);return 1;}
void xideProjectReferencesCancel(xide_reference_query* q){if(q)xrtFutureCancel(q->future);}
void xideProjectReferencesQueryDestroy(xide_reference_query* q)
{if(!q)return;if(q->linked)xrtFutureWatchRemove(q->future,&q->watch);xrtFutureDestroy(q->future);xrtFree(q);}
#endif
