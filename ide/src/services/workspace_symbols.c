#include "services/workspace_symbols.h"
#ifdef XLS_COMPILER_XRT
#include "platform/path.h"
#include "analysis/worker_modules.h"
#include "analysis/input_batch.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#define GUARD_BYTES (128u*1024u*1024u)
#define LABEL_BYTES (2u*1024u*1024u)
#define ROOT_LIMIT 65536u
typedef struct overlay_name {char* path;wchar_t* key;int bom;} overlay_name;
typedef struct overlay_names {xatomic32 refs;size_t count,bytes;overlay_name entries[64];} overlay_names;
typedef struct symbol_record {
 xatomic32 refs;size_t scopeIndex,bytes;char* owner;wchar_t* entry;
 xls_context* scope;xide_semantic_symbols* symbols;xide_language_input_guard* guard;overlay_names* overlays;
} symbol_record;
typedef struct symbol_root {char* path;wchar_t* key;size_t scopeIndex;int primary,covered,duplicate,rejected;} symbol_root;
struct xide_workspace_symbols {xtaskpool* pool;xls_worker* actor;xide_toolchains* cache;char* compiler;xide_workspace_symbol_index* index;xarray records;xls_input_batch* history;};
struct xide_workspace_symbol_query {xfuture* future;xfuturewatch watch;int initialized,linked;void (*wake)(void*);void* user;};
typedef struct symbol_input {
 xide_workspace_symbols* store;xls_context* contexts[64];size_t contextCount,count;
 xls_analysis_document docs[64];overlay_names* names;char query[513];size_t limit;
 char hashes[64][65];unsigned char hashed[64];
} symbol_input;
typedef struct symbol_output {xide_workspace_symbol_result* result;xide_workspace_symbol_work work;char error[512];} symbol_output;
static xstrview view(const char* p){return (xstrview){p,strlen(p)};}
static const xvalue* field(const xvalue* v,const char* key){return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,view(key)) : NULL;}
static const char* string(const xvalue* v){xstrview s={0};return v && xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) ? s.Data : NULL;}
static int label(const char* p){size_t n=0;if(p)while(n<=262144 && p[n])++n;return n && n<=262144 && xrtUtf8Valid((xstrview){p,n},NULL);}
static int charge(size_t* n,size_t add,size_t limit){if(*n>limit || add>limit-*n)return 0;*n+=add;return 1;}
static int stopped(void* p){return xrtCancelRequested(p);}
static int fail(symbol_output* out,const char* message){snprintf(out->error,sizeof(out->error),"%s",message);return 0;}
static int bom(const xls_text* t){return xlsTextSize(t)>=3 && !memcmp(xlsTextData(t),"\xef\xbb\xbf",3);}
static overlay_names* namesRetain(overlay_names* p){if(p)xrtAtomic32FetchAdd(&p->refs,1,XMEMORY_RELAXED);return p;}
static void namesRelease(overlay_names* p)
{if(!p || xrtAtomic32FetchSub(&p->refs,1,XMEMORY_ACQ_REL)!=1)return;for(size_t i=0;i<p->count;++i){xrtFree(p->entries[i].path);free(p->entries[i].key);}xrtFree(p);}
static symbol_record* recordRetain(symbol_record* p){if(p)xrtAtomic32FetchAdd(&p->refs,1,XMEMORY_RELAXED);return p;}
static void recordRelease(symbol_record* p)
{if(!p || xrtAtomic32FetchSub(&p->refs,1,XMEMORY_ACQ_REL)!=1)return;xrtFree(p->owner);free(p->entry);xlsContextRelease(p->scope);xideSemanticSymbolsDestroy(p->symbols);xideLanguageInputGuardRelease(p->guard);namesRelease(p->overlays);xrtFree(p);}
static void recordsUnit(xarray* a){for(size_t i=0;i<a->Count;++i)recordRelease(((symbol_record**)a->Data)[i]);xrtArrayUnit(a);}
static void inputFree(void* p,void* user)
{(void)user;symbol_input* in=p;for(size_t i=0;i<in->contextCount;++i)xlsContextRelease(in->contexts[i]);for(size_t i=0;i<in->count;++i){xrtFree((char*)in->docs[i].key);xlsTextRelease(in->docs[i].text);}namesRelease(in->names);xrtFree(in);}
static void outputFree(void* p,void* user){(void)user;symbol_output* out=p;xideWorkspaceSymbolResultDestroy(out->result);xrtFree(out);}
static int findName(const overlay_names* p,const wchar_t* key)
{for(size_t i=0;i<p->count;++i)if(!xidePathNativeKeyCompare(p->entries[i].key,key))return (int)i;return -1;}
static const char* overlayHash(symbol_input* in,size_t at,symbol_output* out)
{
 if(!in->hashed[at]){unsigned char hash[32];size_t skip=bom(in->docs[at].text)?3:0,size=xlsTextSize(in->docs[at].text)-skip;
  if(!xrtSha256(xlsTextData(in->docs[at].text)+skip,size,hash))return NULL;
  for(size_t i=0;i<32;++i)snprintf(in->hashes[at]+2*i,3,"%02x",hash[i]);
  in->hashed[at]=1;out->work.overlayHashBytes+=size;
 }return in->hashes[at];
}
static int current(symbol_record* r,symbol_input* in,xcancel* cancel,symbol_output* out)
{
 const xvalue* files=xideLanguageInputGuardFiles(r->guard);
 for(size_t i=0;i<xrtValueCount(files);++i){
  if(stopped(cancel))return 0;
  const xvalue* file=xrtValueArrayGet(files,i);const char *path=string(field(file,"path")),*origin=string(field(file,"origin"));wchar_t* key=path?xidePathLexicalWide(path):NULL;if(!key || !origin){free(key);return 0;}
  int now=findName(in->names,key),old=findName(r->overlays,key);free(key);int overlay=!strcmp(origin,"overlay");if(overlay!=(now>=0))return 0;
  if(overlay){int64 rawSize=0;const char *hash=overlayHash(in,(size_t)now,out),*a=string(field(file,"textSha256")),*b=string(field(file,"rawSha256"));size_t skip=bom(in->docs[now].text)?3:0;
   if(old<0 || r->overlays->entries[old].bom!=in->names->entries[now].bom || !hash || !a || !b || strcmp(hash,a) || strcmp(hash,b) || !xrtValueGetInt(field(file,"rawSize"),&rawSize) || rawSize<0 || (uint64_t)rawSize!=xlsTextSize(in->docs[now].text)-skip)return 0;
  }
 }
 for(size_t i=0;i<in->names->count;++i){if(stopped(cancel))return 0;if(findName(r->overlays,in->names->entries[i].key)<0 && xideLanguageInputGuardTouches(r->guard,in->names->entries[i].path))return 0;}
 for(size_t i=0;i<r->overlays->count;++i){if(stopped(cancel))return 0;if(findName(in->names,r->overlays->entries[i].key)<0 && xideLanguageInputGuardTouches(r->guard,r->overlays->entries[i].path))return 0;}
 xide_model_error e={0};return xideLanguageInputGuardValidate(r->guard,stopped,cancel,&e);
}
static int rootCompare(const void* av,const void* bv)
{const symbol_root *a=av,*b=bv;if(a->scopeIndex!=b->scopeIndex)return (a->scopeIndex>b->scopeIndex)-(a->scopeIndex<b->scopeIndex);int n=xidePathNativeKeyCompare(a->key,b->key);if(n)return n;if(a->primary!=b->primary)return b->primary-a->primary;return strcmp(a->path,b->path);}
static int rootAdd(xarray* roots,const xls_context_spec* spec,const char* path,size_t scopeIndex,int primary,size_t* bytes)
{
 if(!path || !xidePathHasExtension(path,".xl"))return 1;
 symbol_root root={.scopeIndex=scopeIndex,.primary=primary};root.path=xidePathResolveLexical(spec->directory,path);root.key=root.path?xidePathLexicalWide(root.path):NULL;
 if(!root.path || !root.key || roots->Count>=ROOT_LIMIT || !charge(bytes,strlen(root.path)+1+(wcslen(root.key)+1)*sizeof(wchar_t)+2*sizeof(root),GUARD_BYTES) || !xrtArrayPush(roots,&root)){free(root.path);free(root.key);return 0;}return 1;
}
static int rootsBuild(symbol_input* in,xarray* roots,xcancel* cancel,symbol_output* out)
{
 size_t bytes=0;
 for(size_t i=0;i<in->contextCount;++i){const xls_context_spec* spec=xlsContextSpec(in->contexts[i]);if(!rootAdd(roots,spec,spec->entry,i,1,&bytes))return fail(out,"Workspace entry capacity or allocation failed.");
  for(size_t j=0;j<spec->lists[XLS_CONTEXT_MEMBERS].count;++j){if(stopped(cancel))return 0;if(!rootAdd(roots,spec,spec->lists[XLS_CONTEXT_MEMBERS].items[j],i,0,&bytes))return fail(out,"Workspace member capacity or allocation failed.");}
 }
 if(!xrtArraySort(roots,rootCompare))return fail(out,"Cannot order workspace members.");
 symbol_root* all=(symbol_root*)roots->Data;
 for(size_t i=0;i<roots->Count;++i){if(stopped(cancel))return 0;if(i && all[i].scopeIndex==all[i-1].scopeIndex && !xidePathNativeKeyCompare(all[i].key,all[i-1].key)){all[i].duplicate=1;continue;}++out->work.roots;}
 return 1;
}
static int recordOrder(const symbol_record* r,size_t scopeIndex,const wchar_t* key)
{return r->scopeIndex!=scopeIndex ? (r->scopeIndex>scopeIndex)-(r->scopeIndex<scopeIndex) : xidePathNativeKeyCompare(r->entry,key);}
static int recordCompare(const void* av,const void* bv)
{symbol_record* const* a=av;symbol_record* const* b=bv;return recordOrder(*a,(*b)->scopeIndex,(*b)->entry);}
static symbol_record* cached(symbol_input* in,const symbol_root* root,xcancel* cancel,symbol_output* out)
{
 xarray* a=&in->store->records;symbol_record** records=(symbol_record**)a->Data;size_t lo=0,hi=a->Count;
 while(lo<hi){size_t mid=lo+(hi-lo)/2;if(recordOrder(records[mid],root->scopeIndex,root->key)<0)lo=mid+1;else hi=mid;}
 if(lo<a->Count && !recordOrder(records[lo],root->scopeIndex,root->key) && xlsContextEqual(records[lo]->scope,in->contexts[root->scopeIndex]) && current(records[lo],in,cancel,out))return recordRetain(records[lo]);
 return NULL;
}
static void childWake(void* p){xrtEventSet(p);}
static symbol_record* capture(symbol_input* in,const symbol_root* root,xcancel* cancel,int* rejected,symbol_output* out)
{
 symbol_record* r=NULL;xide_model_error e={0};xls_context_spec spec=*xlsContextSpec(in->contexts[root->scopeIndex]);spec.entry=root->path;spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){0};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xevent* event=xrtEventCreate(false,false);xide_language_analysis_options options={1,childWake,event};
 xide_language_check* check=context && event ? xideLanguageCheckStartDeclarationCapture(context,in->store->cache,in->store->compiler,in->store->actor,in->docs,in->count,&options,&e):NULL;xlsContextRelease(context);xcancelwatch* watch=check?xrtCancelWatch(cancel,childWake,event):NULL;
 if(!check){fail(out,"Cannot start background workspace member analysis.");goto done;}
 while(xideLanguageCheckState(check,NULL,0)==XIDE_CHECK_RUNNING && !stopped(cancel))xrtEventWaitFor(event,XIDE_XRT_TIMEOUT_MS(10));
 if(stopped(cancel)){xideLanguageCheckCancel(check);goto done;}
 r=xrtCalloc(1,sizeof(*r));if(r)xrtAtomic32Init(&r->refs,1);xide_declaration_capture_work work={0};
 if(!r || !xideLanguageCheckCaptureDeclarations(check,&r->symbols,&work)){*rejected=r && xideLanguageCheckDeclarationSourceRejected(check);fail(out,"A workspace member did not produce verified declarations.");goto failed;}
 r->scopeIndex=root->scopeIndex;r->scope=xlsContextRetain(in->contexts[root->scopeIndex]);r->guard=xideLanguageCheckCaptureInputGuard(check,&e);r->entry=xidePathLexicalWide(root->path);r->overlays=namesRetain(in->names);
 size_t size=strlen(root->path)+32;r->owner=xrtMalloc(size);if(r->owner)snprintf(r->owner,size,"%02zu:%s",root->scopeIndex,root->path);r->bytes=sizeof(*r);
 if(!r->owner || !r->entry || !r->guard || !charge(&r->bytes,size+(wcslen(r->entry)+1)*sizeof(wchar_t),GUARD_BYTES) || !charge(&r->bytes,xideLanguageInputGuardReportBytes(r->guard),GUARD_BYTES) || !current(r,in,cancel,out)){fail(out,"Workspace member proof capture or validation failed.");goto failed;}
 out->work.analysisThread=work.workerThread;out->work.analysisBuilds=work.analysis.builds;out->work.analysisHits=work.analysis.hits;++out->work.captured;goto done;
failed:recordRelease(r);r=NULL;
done:if(watch)xrtCancelUnwatch(watch);xideLanguageCheckDestroy(check);if(event)xrtEventDestroy(event);return r;
}
static int cover(symbol_record* record,xarray* roots,xcancel* cancel)
{
 symbol_root* all=(symbol_root*)roots->Data;const xvalue* files=xideLanguageInputGuardFiles(record->guard);
 for(size_t i=0;i<xrtValueCount(files);++i){if(stopped(cancel))return 0;const xvalue* file=xrtValueArrayGet(files,i);int64 kind=0;const char* path=string(field(file,"path"));if(!path || !xrtValueGetInt(field(file,"kind"),&kind))return 0;if(!(kind&1))continue;
  wchar_t* key=xidePathLexicalWide(path);if(!key)return 0;size_t lo=0,hi=roots->Count;while(lo<hi){size_t mid=lo+(hi-lo)/2;int n=all[mid].scopeIndex!=record->scopeIndex ? (all[mid].scopeIndex>record->scopeIndex)-(all[mid].scopeIndex<record->scopeIndex) : xidePathNativeKeyCompare(all[mid].key,key);if(n<0)lo=mid+1;else hi=mid;}
  for(size_t j=lo;j<roots->Count && all[j].scopeIndex==record->scopeIndex && !xidePathNativeKeyCompare(all[j].key,key);++j)all[j].covered=1;
  free(key);
 }return 1;
}
static int account(symbol_record* record,xmap* seen,size_t* bytes)
{
 overlay_names* names=record->overlays;xbytesview key={(const unsigned char*)&names,sizeof(names)};unsigned char one=1;
 if(!charge(bytes,record->bytes+2*sizeof(symbol_record*),GUARD_BYTES))return 0;
 if(!xrtMapHas(seen,key) && (!charge(bytes,names->bytes,GUARD_BYTES) || !xrtMapSet(seen,key,&one)))return 0;
 return 1;
}
static int hasOwner(const xarray* a,const symbol_record* record)
{
 symbol_record** records=(symbol_record**)a->Data;size_t lo=0,hi=a->Count;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(recordOrder(records[mid],record->scopeIndex,record->entry)<0)lo=mid+1;else hi=mid;}
 return lo<a->Count && !recordOrder(records[lo],record->scopeIndex,record->entry) && !strcmp(records[lo]->owner,record->owner);
}
static xtaskoutcome execute(xcancel* cancel,void* data,xtaskvalue* value)
{
 symbol_input* in=data;symbol_output* out=xrtCalloc(1,sizeof(*out));if(!out)return XTASK_FAILED;out->work.coordinatorThread=xrtThreadCurrentId();xarray roots={0},next={0};xmap seen={0};size_t bytes=0;xide_workspace_symbol_update* update=NULL;xide_workspace_symbol_snapshot* snapshot=NULL;xide_model_error e={0};
 if(!xrtArrayInit(&roots,sizeof(symbol_root)) || !xrtArrayInit(&next,sizeof(symbol_record*)) || !xrtMapInit(&seen,1))goto done;
 if(in->contextCount){xls_session_limits limits={0};xlsInputLimits(NULL,&limits);xls_input_batch* history=xlsInputBatchCreateClosedInContext(in->contexts[0],&limits,in->store->history,in->docs,in->count,NULL,stopped,cancel,out->error,sizeof(out->error));if(!history)goto done;if(xlsInputBatchStorageBytes(history)>64u*1024u*1024u){xlsInputBatchDestroy(history);fail(out,"Workspace input history exceeds its storage budget.");goto done;}xlsInputBatchDestroy(in->store->history);in->store->history=history;}
 else{xlsInputBatchDestroy(in->store->history);in->store->history=NULL;}
 if(!rootsBuild(in,&roots,cancel,out))goto done;
 update=xideWorkspaceSymbolIndexBegin(in->store->index,stopped,cancel,&e);if(!update){fail(out,e.message);goto done;}
 symbol_root* all=(symbol_root*)roots.Data;
 /* Configured primaries first;each still has to pass its real program role.
    Later successful importers may cover library-only source rejections. */
 for(int primary=1;primary>=0;--primary)for(size_t i=0;i<roots.Count;++i){symbol_root* root=all+i;if(root->duplicate || root->primary!=primary)continue;if(stopped(cancel))goto done;
  if(root->covered && !root->primary){++out->work.covered;continue;}
  symbol_record* record=cached(in,root,cancel,out);if(record)++out->work.reused;else{int rejected=0;record=capture(in,root,cancel,&rejected,out);if(!record){if(rejected && !root->primary && !stopped(cancel)){root->rejected=1;++out->work.sourceRejections;*out->error=0;continue;}goto done;}}
  if(!account(record,&seen,&bytes) || !xrtArrayPush(&next,&record)){recordRelease(record);fail(out,"Workspace proof retention exceeded its allocation or byte budget.");goto done;}
  if(!cover(record,&roots,cancel) || !xideWorkspaceSymbolIndexSet(update,record->owner,record->scope,record->symbols,&e)){fail(out,*e.message?e.message:"Cannot prepare workspace declaration coverage.");goto done;}
 }
 for(size_t i=0;i<roots.Count;++i)if(!all[i].duplicate && !all[i].covered){fail(out,"Workspace has an uncovered or rejected source member.");goto done;}
 /* The primary-first traversal is not record order;sort the small pointer
    vector before binary lookup/removal and future guarded cache hits. */
 if(!xrtArraySort(&next,recordCompare)){fail(out,"Cannot order workspace proof records.");goto done;}
 for(size_t i=0;i<in->store->records.Count;++i){if(stopped(cancel))goto done;symbol_record* record=((symbol_record**)in->store->records.Data)[i];if(!hasOwner(&next,record) && !xideWorkspaceSymbolIndexRemove(update,record->owner,&e)){fail(out,e.message);goto done;}}
 for(size_t i=0;i<next.Count;++i)if(!current(((symbol_record**)next.Data)[i],in,cancel,out)){fail(out,"Workspace inputs changed before index publication.");goto done;}
 if(!xideWorkspaceSymbolIndexCommit(update,&e)){update=NULL;fail(out,e.message);goto done;}update=NULL;
 recordsUnit(&in->store->records);in->store->records=next;next=(xarray){0};snapshot=xideWorkspaceSymbolIndexAcquire(in->store->index);out->work.index=xideWorkspaceSymbolSnapshotStats(snapshot);out->work.guardBytes=bytes;
 out->result=xideWorkspaceSymbolSnapshotQuery(snapshot,in->query,in->limit,stopped,cancel,&e);if(!out->result){fail(out,e.message);goto done;}
 for(size_t i=0;i<in->store->records.Count;++i)if(!current(((symbol_record**)in->store->records.Data)[i],in,cancel,out)){fail(out,"Workspace inputs changed before query publication.");xideWorkspaceSymbolResultDestroy(out->result);out->result=NULL;break;}
done:
 xideWorkspaceSymbolIndexAbort(update);xideWorkspaceSymbolSnapshotRelease(snapshot);recordsUnit(&next);xrtMapUnit(&seen);
 for(size_t i=0;i<roots.Count;++i){symbol_root* root=(symbol_root*)roots.Data+i;free(root->path);free(root->key);}xrtArrayUnit(&roots);
 if(stopped(cancel)){outputFree(out,NULL);return XTASK_CANCELLED;}
 if(!out->result && !*out->error)fail(out,"Workspace indexing allocation or analysis failed.");
 *value=(xtaskvalue){out,outputFree,NULL};return XTASK_SUCCESS;
}
xide_workspace_symbols* xideWorkspaceSymbolsCreate(xide_toolchains* cache,const char* compiler,xls_module_store* modules)
{
 if(!cache || !label(compiler))return NULL;
 xide_workspace_symbols* store=xrtCalloc(1,sizeof(*store));if(!store)return NULL;store->cache=cache;store->compiler=xrtStrDup(compiler);store->index=xideWorkspaceSymbolIndexCreate(NULL);
 xls_worker_cache_limits limits={8,64u*1024u*1024u};store->actor=xlsWorkerCreateWithModules(NULL,8,&limits,modules);xtaskpoolconfig config={.Threads=1,.QueueLimit=8};store->pool=xrtTaskPoolCreate(&config);int initialized=xrtArrayInit(&store->records,sizeof(symbol_record*));
 if(!store->compiler || !store->index || !store->actor || !store->pool || !initialized){xideWorkspaceSymbolsDestroy(store);return NULL;}return store;
}
void xideWorkspaceSymbolsDestroy(xide_workspace_symbols* store)
{if(!store)return;if(store->pool){xrtTaskPoolCancel(store->pool);xrtTaskPoolDestroy(store->pool);}recordsUnit(&store->records);xlsInputBatchDestroy(store->history);xlsWorkerDestroy(store->actor);xideWorkspaceSymbolIndexDestroy(store->index);xrtFree(store->compiler);xrtFree(store);}
xide_workspace_symbol_query* xideWorkspaceSymbolsSubmit(xide_workspace_symbols* store,xls_context* const* contexts,size_t contextCount,const xls_analysis_document* docs,size_t count,const char* query,size_t limit)
{
 size_t querySize=0;if(query)while(querySize<=512 && query[querySize])++querySize;else querySize=513;
 if(!store || contextCount>64 || (contextCount && !contexts) || (!contextCount && count) || count>64 || (count && !docs) || querySize>512 || !xrtUtf8Valid((xstrview){query,querySize},NULL) || !limit || limit>4096)return NULL;
 symbol_input* in=xrtCalloc(1,sizeof(*in));xide_workspace_symbol_query* q=xrtCalloc(1,sizeof(*q));if(!in || !q){xrtFree(in);xrtFree(q);return NULL;}in->store=store;in->limit=limit;memcpy(in->query,query,querySize+1);in->names=xrtCalloc(1,sizeof(*in->names));if(!in->names){inputFree(in,NULL);xrtFree(q);return NULL;}xrtAtomic32Init(&in->names->refs,1);in->names->bytes=sizeof(*in->names);int okay=1;size_t bindings=0,contextBytes=0,textBytes=0,labelBytes=0;
 for(size_t i=0;okay && i<contextCount;++i){const xls_context_spec* spec=xlsContextSpec(contexts[i]);okay=spec && (spec->kind==XLS_CONTEXT_XLANG || spec->kind==XLS_CONTEXT_PROJECT) && xrtPathIsAbs(spec->entry) && xrtPathIsAbs(spec->directory) && xidePathHasExtension(spec->entry,".xl") && spec->lists[XLS_CONTEXT_MEMBERS].count<ROOT_LIMIT-bindings && charge(&contextBytes,xlsContextBytes(contexts[i]),16u*1024u*1024u);if(!okay)break;bindings+=1+spec->lists[XLS_CONTEXT_MEMBERS].count;in->contexts[in->contextCount++]=xlsContextRetain(contexts[i]);}
 for(size_t i=0;okay && i<count;++i){okay=label(docs[i].key) && label(docs[i].path) && xrtPathIsAbs(docs[i].path) && docs[i].text && docs[i].identity && docs[i].revision && charge(&textBytes,xlsTextStorageBytes(docs[i].text),64u*1024u*1024u) && charge(&labelBytes,strlen(docs[i].key)+strlen(docs[i].path)+2,LABEL_BYTES);if(!okay)break;
  overlay_name* name=in->names->entries+i;name->path=xrtStrDup(docs[i].path);name->key=xidePathLexicalWide(docs[i].path);name->bom=bom(docs[i].text);++in->names->count;in->docs[i]=docs[i];in->docs[i].key=xrtStrDup(docs[i].key);in->docs[i].path=name->path;in->docs[i].text=xlsTextRetain(docs[i].text);++in->count;okay=name->path && name->key && in->docs[i].key && charge(&in->names->bytes,strlen(name->path)+1+(wcslen(name->key)+1)*sizeof(wchar_t),LABEL_BYTES);
  for(size_t j=0;okay && j<i;++j)okay=strcmp(in->docs[j].key,in->docs[i].key) && xidePathNativeKeyCompare(in->names->entries[j].key,name->key);
 }
 xtaskargs args={.Destroy=inputFree};if(okay)q->future=xrtTaskSubmit(store->pool,execute,in,&args);if(!q->future){inputFree(in,NULL);xrtFree(q);return NULL;}return q;
}
enum xide_workspace_symbol_state xideWorkspaceSymbolsResult(xide_workspace_symbol_query* q,const xide_workspace_symbol_result** result,xide_workspace_symbol_work* work,char* error,size_t capacity)
{
 if(result)*result=NULL;
 if(work)*work=(xide_workspace_symbol_work){0};
 if(error && capacity)*error=0;
 if(!q)return XIDE_WORKSPACE_SYMBOL_FAILED;
 xfuturestate state=xrtFutureState(q->future);if(state==XFUTURE_PENDING)return XIDE_WORKSPACE_SYMBOL_PENDING;if(state==XFUTURE_CANCELLED)return XIDE_WORKSPACE_SYMBOL_CANCELLED;
 const symbol_output* out=state==XFUTURE_RESOLVED?xrtFutureValue(q->future):NULL;if(out && work)*work=out->work;
 if(!out || *out->error || !out->result){if(error && capacity)snprintf(error,capacity,"%s",out && *out->error?out->error:"Workspace symbol task failed.");return XIDE_WORKSPACE_SYMBOL_FAILED;}if(result)*result=out->result;return XIDE_WORKSPACE_SYMBOL_READY;
}
xide_workspace_symbol_result* xideWorkspaceSymbolsTake(xide_workspace_symbol_query* q)
{if(xideWorkspaceSymbolsResult(q,NULL,NULL,NULL,0)!=XIDE_WORKSPACE_SYMBOL_READY)return NULL;symbol_output* out=xrtFutureValue(q->future);xide_workspace_symbol_result* result=out->result;out->result=NULL;return result;}
static void wakeQuery(void* p){xide_workspace_symbol_query* q=p;q->wake(q->user);}
int xideWorkspaceSymbolsSetWake(xide_workspace_symbol_query* q,void (*wake)(void*),void* user)
{if(!q || !wake || q->initialized)return 0;q->wake=wake;q->user=user;if(!xrtFutureWatchInit(&q->watch,wakeQuery,NULL,q))return 0;q->initialized=1;xfuturewatchresult state=xrtFutureWatchAdd(q->future,&q->watch);if(state==XFUTURE_WATCH_ERROR)return 0;q->linked=state==XFUTURE_WATCH_PENDING;if(state==XFUTURE_WATCH_READY)wake(user);return 1;}
void xideWorkspaceSymbolsCancel(xide_workspace_symbol_query* q){if(q)xrtFutureCancel(q->future);}
void xideWorkspaceSymbolsQueryDestroy(xide_workspace_symbol_query* q)
{if(!q)return;if(q->linked)xrtFutureWatchRemove(q->future,&q->watch);xrtFutureDestroy(q->future);xrtFree(q);}
#endif
