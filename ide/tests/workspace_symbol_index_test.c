#include "services/workspace_symbol_index.h"
#include "services/language_check.h"
#include "platform/path.h"
#include <xrt.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "declaration_symbols_source.h"
static size_t operations,faultAt;static int tracking,forceCollisions;static size_t wideKeys;
static size_t largeCount,largeSize,largeAllocs,refuseLargeAt,mergePolls;static int observeMerge,mergeStarted,mergeReleased,cancelWhileScratchLive;static void* mergeScratch;
static int operation(void){return !tracking || ++operations!=faultAt;}
static void* indexMalloc(size_t n){return operation() ? xrtMalloc(n) : NULL;}
static void* indexCalloc(size_t n,size_t size){int large=largeCount && n==largeCount && size==largeSize;if(large && ++largeAllocs==refuseLargeAt)return NULL;void* p=operation() ? xrtCalloc(n,size) : NULL;if(p && large && observeMerge && largeAllocs==2){mergeStarted=1;mergeScratch=p;mergeReleased=0;}return p;}
static void indexFree(void* p){if(mergeStarted && p==mergeScratch)mergeReleased=1;xrtFree(p);}
static char* indexDup(const char* p){return operation() ? xrtStrDup(p) : NULL;}
static bool arrayInit(xarray* a,size_t size){return operation() && xrtArrayInit(a,size);}
static bool arrayReserve(xarray* a,size_t n){return operation() && xrtArrayReserve(a,n);}
static bool arrayPush(xarray* a,const void* p){return operation() && xrtArrayPush(a,p);}
static bool arrayInsert(xarray* a,size_t at,const void* p,size_t n){return operation() && xrtArrayInsert(a,at,p,n);}
static bool mapInit(xmap* m,size_t size){return operation() && xrtMapInit(m,size);}
static bool mapSet(xmap* m,xbytesview key,const void* p){return operation() && xrtMapSet(m,key,p);}
static wchar_t* indexPathKey(const char* p);
static void freePathKey(void* p){if(p)--wideKeys;free(p);}
static uint64_t scopeHash(const xls_context* c){return forceCollisions ? 1 : xlsContextHash(c);}
#define xrtMalloc indexMalloc
#define xrtCalloc indexCalloc
#define xrtFree indexFree
#define xrtStrDup indexDup
#define xrtArrayInit arrayInit
#define xrtArrayReserve arrayReserve
#define xrtArrayPush arrayPush
#define xrtArrayInsert arrayInsert
#define xrtMapInit mapInit
#define xrtMapSet mapSet
#define xidePathLexicalWide indexPathKey
#define xlsContextHash scopeHash
#define free freePathKey
#include "../src/services/workspace_symbol_index.c"
#undef xrtMalloc
#undef xrtCalloc
#undef xrtFree
#undef xrtStrDup
#undef xrtArrayInit
#undef xrtArrayReserve
#undef xrtArrayPush
#undef xrtArrayInsert
#undef xrtMapInit
#undef xrtMapSet
#undef xidePathLexicalWide
#undef xlsContextHash
#undef free
static wchar_t* indexPathKey(const char* p){wchar_t* result=operation() ? xidePathLexicalWide(p) : NULL;if(result)++wideKeys;return result;}
static size_t checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"workspace_symbol_index_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static uint64_t liveBlocks(void){xmemstats s;xrtMemStatsGet(&s);return s.BlockAllocCalls-s.BlockFreeCalls;}
static int waitReady(xide_language_check* q){for(unsigned i=0;i<15000;++i){char error[512]={0};enum xide_check_state state=xideLanguageCheckState(q,error,sizeof(error));if(state!=XIDE_CHECK_RUNNING){if(state!=XIDE_CHECK_READY)fprintf(stderr,"capture:%s\n",error);return state==XIDE_CHECK_READY;}xrtSleep(1);}return 0;}
static xide_semantic_symbols* capture(xls_context* scope,xls_worker* worker,xide_toolchains* cache,const char* compiler,const char* source)
{
 const char* entry=xlsContextSpec(scope)->entry;if(!xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)source,strlen(source)}))return NULL;
 xide_language_analysis_options options={.nativeBackend=1};xide_model_error error={0};xide_language_check* q=xideLanguageCheckStartDeclarationCapture(scope,cache,compiler,worker,NULL,0,&options,&error);xide_semantic_symbols* symbols=NULL;
 if(q && waitReady(q))xideLanguageCheckCaptureDeclarations(q,&symbols,NULL);
 xideLanguageCheckDestroy(q);return symbols;
}
static int setOne(xide_workspace_symbol_index* index,const char* key,xls_context* scope,xide_semantic_symbols* symbols)
{xide_model_error e={0};xide_workspace_symbol_update* u=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);if(!u)return 0;if(!xideWorkspaceSymbolIndexSet(u,key,scope,symbols,&e)){xideWorkspaceSymbolIndexAbort(u);return 0;}return xideWorkspaceSymbolIndexCommit(u,&e);}
static int hasName(xide_workspace_symbol_snapshot* s,const char* query,const char* name)
{xide_model_error e={0};xide_workspace_symbol_result* r=xideWorkspaceSymbolSnapshotQuery(s,query,4096,NULL,NULL,&e);if(!r)return 0;int found=0;for(size_t i=0;i<xideWorkspaceSymbolResultCount(r);++i){xide_workspace_symbol_view v;if(xideWorkspaceSymbolResultAt(r,i,&v) && !strcmp(v.declaration.name,name))found=1;}xideWorkspaceSymbolResultDestroy(r);return found;}
/* Independent membership oracle: DP processes the complete scalar sequence
 * backwards instead of the production greedy UTF8/substr paths. */
static int includes(const char* value,xstrview term)
{
 uint32 needle[128],haystack[8192];xutfresult a=xrtUtf8To32Buffer(term,needle,128,XUTF_STRICT),b=xrtUtf8To32Buffer((xstrview){value,strlen(value)},haystack,8192,XUTF_STRICT);
 if(a.Status!=XUTF_OK || b.Status!=XUTF_OK || a.Written>128)return 0;
 unsigned char dp[129]={1};for(size_t i=0;i<b.Written;++i)for(size_t j=a.Written;j>0;--j){uint32 x=haystack[i],y=needle[j-1];if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;if(x==y && dp[j-1])dp[j]=1;}return dp[a.Written]!=0;
}
static int oracle(const char* qualified,const char* query)
{const char* p=query;while(*p){while(*p==' ')++p;if(!*p)break;const char* begin=p;while(*p && *p!=' ')++p;if(!includes(qualified,(xstrview){begin,(size_t)(p-begin)}))return 0;}return 1;}
static int checkQuery(xide_workspace_symbol_snapshot* s,const char* query)
{
 xide_workspace_symbol_stats stats=xideWorkspaceSymbolSnapshotStats(s);size_t expected=0;for(size_t i=0;i<stats.rows;++i){xide_workspace_symbol_view v;CHECK(xideWorkspaceSymbolSnapshotResolve(s,stats.generation,i,&v));expected+=oracle(v.qualified,query);}
 xide_model_error e={0};xide_workspace_symbol_result* full=xideWorkspaceSymbolSnapshotQuery(s,query,4096,NULL,NULL,&e);CHECK(full && xideWorkspaceSymbolResultMatches(full)==expected && xideWorkspaceSymbolResultCount(full)==expected);
 for(size_t i=0;i<expected;++i){xide_workspace_symbol_view v;CHECK(xideWorkspaceSymbolResultAt(full,i,&v) && oracle(v.qualified,query));for(size_t j=0;j<i;++j){xide_workspace_symbol_view other;CHECK(xideWorkspaceSymbolResultAt(full,j,&other) && v.ticket!=other.ticket);}}
 for(size_t k=1;k<=7;++k){xide_workspace_symbol_result* part=xideWorkspaceSymbolSnapshotQuery(s,query,k,NULL,NULL,&e);CHECK(part && xideWorkspaceSymbolResultMatches(part)==expected && xideWorkspaceSymbolResultCount(part)==(expected<k ? expected : k));for(size_t i=0;i<xideWorkspaceSymbolResultCount(part);++i){xide_workspace_symbol_view a,b;CHECK(xideWorkspaceSymbolResultAt(full,i,&a) && xideWorkspaceSymbolResultAt(part,i,&b) && a.ticket==b.ticket && a.score==b.score);}xideWorkspaceSymbolResultDestroy(part);}
 xideWorkspaceSymbolResultDestroy(full);return 1;
}
static int cancelledAt(void* p){size_t* state=p;if(++state[0]>=state[1])state[2]=1;return state[2]!=0;}
typedef struct fixture {xls_context *debug,*equal,*release;xide_semantic_symbols *original,*changed;char *directory,*entry,*compiler;} fixture;
static int setup(fixture* f)
{
 f->directory=xrtPathAbs("build/workspace-symbol-index");CHECK(f->directory && xrtDirCreateAll(f->directory));f->entry=xrtPathJoin(f->directory,"符号😀.xl");f->compiler=xrtPathJoin(f->directory,"absent-xl.exe");CHECK(f->entry && f->compiler);
 char* project=xrtPathJoin(f->directory,"fixture.xlp");CHECK(project);xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.entry=f->entry,.directory=f->directory,.configuration="Debug",.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};f->debug=xlsContextCreate(&spec,1024*1024,NULL);f->equal=xlsContextCreate(&spec,1024*1024,NULL);spec.configuration="Release";f->release=xlsContextCreate(&spec,1024*1024,NULL);xrtFree(project);CHECK(f->debug && f->equal && f->release);
 const char* extra="func RB() as int\n return 0\nend\nfunc readBuffer() as int\n return 0\nend\nfunc refreshBuffer() as int\n return 0\nend\nfunc really_big_buffer() as int\n return 0\nend\nfunc read_buffer() as int\n return 0\nend\nfunc readFile() as int\n return 0\nend\nfunc 计算值() as int\n return 0\nend\nfunc 计算缓存() as int\n return 0\nend\n";
 xstrbuf source={0};CHECK(xrtStrBufAppend(&source,xrtStrView(declarationSymbolsBody)) && xrtStrBufAppend(&source,xrtStrView(extra)));
 xls_worker* worker=xlsWorkerCreate(NULL,8);xide_toolchains* cache=xideToolchainsCreate();CHECK(worker && cache);f->original=capture(f->debug,worker,cache,f->compiler,source.Data);CHECK(f->original && xideSemanticSymbolsCount(f->original)==42);
 char* place=strstr(source.Data,"readFile");CHECK(place);memcpy(place,"openFile",8);f->changed=capture(f->debug,worker,cache,f->compiler,source.Data);CHECK(f->changed && xideSemanticSymbolsCount(f->changed)==42);
 xrtStrBufFree(&source);xlsWorkerDestroy(worker);xideToolchainsDestroy(cache);return 1;
}
static void clear(fixture* f){xlsContextRelease(f->debug);xlsContextRelease(f->equal);xlsContextRelease(f->release);xideSemanticSymbolsDestroy(f->original);xideSemanticSymbolsDestroy(f->changed);xrtFree(f->directory);xrtFree(f->entry);xrtFree(f->compiler);}
static int basics(fixture* f)
{
 xide_workspace_symbol_index* index=xideWorkspaceSymbolIndexCreate(NULL);CHECK(index);xide_workspace_symbol_snapshot* empty=xideWorkspaceSymbolIndexAcquire(index);CHECK(empty && !xideWorkspaceSymbolSnapshotStats(empty).rows && checkQuery(empty,""));xideWorkspaceSymbolSnapshotRelease(empty);
 xide_model_error e={0};xide_workspace_symbol_update* u=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);CHECK(u && xideWorkspaceSymbolIndexSet(u,"b",f->equal,f->original,&e) && xideWorkspaceSymbolIndexSet(u,"a",f->debug,f->original,&e));forceCollisions=1;CHECK(xideWorkspaceSymbolIndexSet(u,"release",f->release,f->original,&e) && xideWorkspaceSymbolIndexCommit(u,&e));forceCollisions=0;
 xide_workspace_symbol_snapshot* first=xideWorkspaceSymbolIndexAcquire(index);xide_workspace_symbol_stats stats=xideWorkspaceSymbolSnapshotStats(first);CHECK(stats.parts==3 && stats.scopes==2 && stats.rows==84 && stats.duplicateRows==42 && stats.generation==1 && stats.bytes<4u*1024u*1024u);
 const char* queries[]={"","rb","read","Box current","current Box","Box.current","计值","计算","NativeBox read","T","noSuchThing","r b"};for(size_t i=0;i<sizeof(queries)/sizeof(*queries);++i)CHECK(checkQuery(first,queries[i]));
 xide_workspace_symbol_result* rb=xideWorkspaceSymbolSnapshotQuery(first,"rb",5,NULL,NULL,&e);xide_workspace_symbol_view firstHit;CHECK(rb && xideWorkspaceSymbolResultAt(rb,0,&firstHit) && !strcmp(firstHit.declaration.name,"RB") && firstHit.score==30000);xideWorkspaceSymbolResultDestroy(rb);
 CHECK(hasName(first,"readFile","readFile"));u=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);CHECK(u && xideWorkspaceSymbolIndexSet(u,"a",f->debug,f->changed,&e) && xideWorkspaceSymbolIndexRemove(u,"b",&e) && xideWorkspaceSymbolIndexRemove(u,"release",&e) && xideWorkspaceSymbolIndexCommit(u,&e));
 xide_workspace_symbol_snapshot* second=xideWorkspaceSymbolIndexAcquire(index);CHECK(xideWorkspaceSymbolSnapshotStats(second).generation==2 && xideWorkspaceSymbolSnapshotStats(second).rows==42 && hasName(second,"openFile","openFile") && !hasName(second,"readFile","readFile") && hasName(first,"readFile","readFile"));
 CHECK(!xideWorkspaceSymbolSnapshotResolve(second,firstHit.generation,firstHit.ticket,&firstHit) && !firstHit.owner && !xideWorkspaceSymbolSnapshotResolve(second,2,SIZE_MAX,&firstHit));
 char invalid[]={ (char)0xc0,(char)0xaf,0 };CHECK(!xideWorkspaceSymbolSnapshotQuery(second,invalid,5,NULL,NULL,&e) && e.code==XIDE_MODEL_INVALID && !xideWorkspaceSymbolSnapshotQuery(second,"x",0,NULL,NULL,&e) && !xideWorkspaceSymbolSnapshotQuery(second,NULL,1,NULL,NULL,&e));
 xide_workspace_symbol_update *old=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e),*winner=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);CHECK(old && winner && xideWorkspaceSymbolIndexRemove(winner,"a",&e) && xideWorkspaceSymbolIndexCommit(winner,&e) && !xideWorkspaceSymbolIndexCommit(old,&e) && e.code==XIDE_MODEL_CONFLICT);
 empty=xideWorkspaceSymbolIndexAcquire(index);CHECK(!xideWorkspaceSymbolSnapshotStats(empty).rows);xideWorkspaceSymbolSnapshotRelease(empty);
 xideWorkspaceSymbolIndexDestroy(index);CHECK(checkQuery(first,"Box current") && checkQuery(second,"计值"));xideWorkspaceSymbolSnapshotRelease(first);xideWorkspaceSymbolSnapshotRelease(second);return 1;
}
static int faults(fixture* f)
{
 xide_model_error e={0};xide_workspace_symbol_index* warm=xideWorkspaceSymbolIndexCreate(NULL);CHECK(warm && setOne(warm,"a",f->debug,f->original));xide_workspace_symbol_snapshot* s=xideWorkspaceSymbolIndexAcquire(warm);CHECK(checkQuery(s,"rb"));xideWorkspaceSymbolSnapshotRelease(s);xideWorkspaceSymbolIndexDestroy(warm);
 warm=xideWorkspaceSymbolIndexCreate(NULL);CHECK(warm && setOne(warm,"a",f->debug,f->original));tracking=1;operations=faultAt=0;xide_workspace_symbol_update* u=xideWorkspaceSymbolIndexBegin(warm,NULL,NULL,&e);CHECK(u && xideWorkspaceSymbolIndexSet(u,"a",f->debug,f->changed,&e) && xideWorkspaceSymbolIndexCommit(u,&e));tracking=0;size_t total=operations;xideWorkspaceSymbolIndexDestroy(warm);
 for(size_t fail=1;fail<=total;++fail){uint64_t before=liveBlocks();size_t paths=wideKeys;xide_workspace_symbol_index* index=xideWorkspaceSymbolIndexCreate(NULL);CHECK(index && setOne(index,"a",f->debug,f->original));tracking=1;operations=0;faultAt=fail;u=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);int okay=0;if(u){int set=xideWorkspaceSymbolIndexSet(u,"a",f->debug,f->changed,&e);okay=xideWorkspaceSymbolIndexCommit(u,&e);CHECK(!set || !okay);}tracking=0;CHECK(!okay && operations>=fail && e.code);s=xideWorkspaceSymbolIndexAcquire(index);CHECK(xideWorkspaceSymbolSnapshotStats(s).generation==1 && hasName(s,"readFile","readFile") && !hasName(s,"openFile","openFile"));xideWorkspaceSymbolSnapshotRelease(s);xideWorkspaceSymbolIndexDestroy(index);CHECK(liveBlocks()==before && wideKeys==paths);}
 xide_workspace_symbol_index* index=xideWorkspaceSymbolIndexCreate(NULL);CHECK(index && setOne(index,"a",f->debug,f->original));s=xideWorkspaceSymbolIndexAcquire(index);tracking=1;operations=faultAt=0;xide_workspace_symbol_result* q=xideWorkspaceSymbolSnapshotQuery(s,"rb",5,NULL,NULL,&e);tracking=0;CHECK(q && operations==2);xideWorkspaceSymbolResultDestroy(q);uint64_t before=liveBlocks();
 for(size_t fail=1;fail<=2;++fail){tracking=1;operations=0;faultAt=fail;q=xideWorkspaceSymbolSnapshotQuery(s,"rb",5,NULL,NULL,&e);tracking=0;CHECK(!q && e.code && liveBlocks()==before);}
 xideWorkspaceSymbolSnapshotRelease(s);xideWorkspaceSymbolIndexDestroy(index);printf("Index mandatory refusal points=%zu; query allocations=2; all old snapshots preserved.\n",total);return 1;
}
static int cancellations(fixture* f)
{
 xide_model_error e={0};xide_workspace_symbol_index* index=xideWorkspaceSymbolIndexCreate(NULL);CHECK(index && setOne(index,"a",f->debug,f->original));size_t state[3]={0,SIZE_MAX,0};xide_workspace_symbol_update* u=xideWorkspaceSymbolIndexBegin(index,cancelledAt,state,&e);CHECK(u && xideWorkspaceSymbolIndexSet(u,"a",f->debug,f->changed,&e) && xideWorkspaceSymbolIndexCommit(u,&e));size_t gates=state[0];xideWorkspaceSymbolIndexDestroy(index);
 for(size_t gate=1;gate<=gates;++gate){index=xideWorkspaceSymbolIndexCreate(NULL);CHECK(index && setOne(index,"a",f->debug,f->original));size_t stop[3]={0,gate,0};u=xideWorkspaceSymbolIndexBegin(index,cancelledAt,stop,&e);int okay=0;if(u){xideWorkspaceSymbolIndexSet(u,"a",f->debug,f->changed,&e);okay=xideWorkspaceSymbolIndexCommit(u,&e);}CHECK(!okay && stop[2]);xide_workspace_symbol_snapshot* s=xideWorkspaceSymbolIndexAcquire(index);CHECK(xideWorkspaceSymbolSnapshotStats(s).generation==1 && hasName(s,"readFile","readFile"));xideWorkspaceSymbolSnapshotRelease(s);xideWorkspaceSymbolIndexDestroy(index);}
 index=xideWorkspaceSymbolIndexCreate(NULL);CHECK(index && setOne(index,"a",f->debug,f->original));xide_workspace_symbol_snapshot* s=xideWorkspaceSymbolIndexAcquire(index);state[0]=state[2]=0;state[1]=SIZE_MAX;xide_workspace_symbol_result* q=xideWorkspaceSymbolSnapshotQuery(s,"rb",5,cancelledAt,state,&e);CHECK(q);xideWorkspaceSymbolResultDestroy(q);
 for(size_t gate=1;gate<=state[0];++gate){size_t stop[3]={0,gate,0};q=xideWorkspaceSymbolSnapshotQuery(s,"rb",5,cancelledAt,stop,&e);CHECK(!q && stop[2]);}
 xideWorkspaceSymbolSnapshotRelease(s);u=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);CHECK(u);xideWorkspaceSymbolIndexDestroy(index);CHECK(!xideWorkspaceSymbolIndexCommit(u,&e));printf("Workspace update cancellation gates=%zu; all visited.\n",gates);return 1;
}
static int budgets(fixture* f)
{
 xide_model_error e={0};xide_workspace_symbol_index* index=xideWorkspaceSymbolIndexCreate(&(xide_workspace_symbol_limits){.rows=42,.parts=1});CHECK(index && setOne(index,"a",f->debug,f->original));xide_workspace_symbol_update* u=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);CHECK(u && !xideWorkspaceSymbolIndexSet(u,"b",f->debug,f->original,&e) && !xideWorkspaceSymbolIndexCommit(u,&e));xideWorkspaceSymbolIndexDestroy(index);
 index=xideWorkspaceSymbolIndexCreate(&(xide_workspace_symbol_limits){.rows=42});CHECK(index && setOne(index,"a",f->debug,f->original));u=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);CHECK(u && xideWorkspaceSymbolIndexSet(u,"b",f->release,f->original,&e) && !xideWorkspaceSymbolIndexCommit(u,&e));xide_workspace_symbol_snapshot* s=xideWorkspaceSymbolIndexAcquire(index);CHECK(xideWorkspaceSymbolSnapshotStats(s).parts==1);xideWorkspaceSymbolSnapshotRelease(s);xideWorkspaceSymbolIndexDestroy(index);
 index=xideWorkspaceSymbolIndexCreate(&(xide_workspace_symbol_limits){.bytes=1024});CHECK(index);u=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);CHECK(u && !xideWorkspaceSymbolIndexSet(u,"a",f->debug,f->original,&e) && !xideWorkspaceSymbolIndexCommit(u,&e));xideWorkspaceSymbolIndexDestroy(index);return 1;
}
typedef struct reentrant {xide_workspace_symbol_index* index;xide_workspace_symbol_update* winner;size_t calls;int armed,closed,won;} reentrant;
static int competingCommit(void* p)
{reentrant* r=p;if(r->armed && ++r->calls==2){r->won=xideWorkspaceSymbolIndexCommit(r->winner,NULL);r->winner=NULL;}return 0;}
static int closingIndex(void* p){reentrant* r=p;if(!r->closed){r->closed=1;xideWorkspaceSymbolIndexDestroy(r->index);}return 0;}
typedef struct snapshot_closer {xide_workspace_symbol_snapshot* snapshot;int closed;} snapshot_closer;
static int closeSnapshot(void* p){snapshot_closer* c=p;if(!c->closed){c->closed=1;xideWorkspaceSymbolSnapshotRelease(c->snapshot);}return 0;}
static int reentrancy(fixture* f)
{
 xide_model_error e={0};xide_workspace_symbol_index* index=xideWorkspaceSymbolIndexCreate(NULL);CHECK(index && setOne(index,"a",f->debug,f->original));reentrant state={.index=index};state.winner=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);CHECK(state.winner && xideWorkspaceSymbolIndexSet(state.winner,"a",f->debug,f->changed,&e));
 xide_workspace_symbol_update* loser=xideWorkspaceSymbolIndexBegin(index,competingCommit,&state,&e);CHECK(loser && xideWorkspaceSymbolIndexSet(loser,"b",f->debug,f->original,&e));state.armed=1;CHECK(!xideWorkspaceSymbolIndexCommit(loser,&e) && state.won && !state.winner && e.code==XIDE_MODEL_CONFLICT);xide_workspace_symbol_snapshot* s=xideWorkspaceSymbolIndexAcquire(index);CHECK(xideWorkspaceSymbolSnapshotStats(s).generation==2 && hasName(s,"openFile","openFile") && !hasName(s,"readFile","readFile"));xideWorkspaceSymbolSnapshotRelease(s);xideWorkspaceSymbolIndexDestroy(index);
 index=xideWorkspaceSymbolIndexCreate(NULL);CHECK(index);state=(reentrant){.index=index};CHECK(!xideWorkspaceSymbolIndexBegin(index,closingIndex,&state,&e) && state.closed);
 index=xideWorkspaceSymbolIndexCreate(NULL);CHECK(index && setOne(index,"a",f->debug,f->original));s=xideWorkspaceSymbolIndexAcquire(index);xideWorkspaceSymbolIndexDestroy(index);snapshot_closer closer={s,0};xide_workspace_symbol_result* result=xideWorkspaceSymbolSnapshotQuery(s,"Box current",10,closeSnapshot,&closer,&e);xide_workspace_symbol_view v;CHECK(result && closer.closed && xideWorkspaceSymbolResultAt(result,0,&v) && !strcmp(v.qualified,"Box.current"));xideWorkspaceSymbolResultDestroy(result);
 index=xideWorkspaceSymbolIndexCreate(NULL);CHECK(index);index->current->stats.generation=UINT64_MAX;xide_workspace_symbol_update* exhausted=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);CHECK(exhausted && !xideWorkspaceSymbolIndexCommit(exhausted,&e) && index->current->stats.generation==UINT64_MAX);xideWorkspaceSymbolIndexDestroy(index);return 1;
}
static int cancelMerge(void* p){(void)p;if(!mergeStarted)return 0;if(++mergePolls==5)cancelWhileScratchLive=!mergeReleased;return mergePolls>=5;}
static DWORD WINAPI readSnapshot(void* p)
{xide_workspace_symbol_snapshot* s=p;for(unsigned i=0;i<100;++i){xide_workspace_symbol_result* r=xideWorkspaceSymbolSnapshotQuery(s,"Box current",10,NULL,NULL,NULL);if(!r || xideWorkspaceSymbolResultCount(r)!=2)return 1;xideWorkspaceSymbolResultDestroy(r);}xideWorkspaceSymbolSnapshotRelease(s);return 0;}
static int scale(fixture* f)
{
 xide_workspace_symbol_index* index=xideWorkspaceSymbolIndexCreate(NULL);CHECK(index);xide_model_error e={0};xide_workspace_symbol_update* u=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);CHECK(u);
 for(unsigned i=0;i<2500;++i){char key[24],configuration[32];snprintf(key,sizeof(key),"owner-%04u",i);snprintf(configuration,sizeof(configuration),"config-%u",i);xls_context_spec spec=*xlsContextSpec(f->debug);spec.configuration=configuration;xls_context* scope=xlsContextCreate(&spec,1024*1024,NULL);CHECK(scope && xideWorkspaceSymbolIndexSet(u,key,scope,f->original,&e));xlsContextRelease(scope);}
 CHECK(xideWorkspaceSymbolIndexCommit(u,&e));xide_workspace_symbol_snapshot* s=xideWorkspaceSymbolIndexAcquire(index);xide_workspace_symbol_stats stats=xideWorkspaceSymbolSnapshotStats(s);CHECK(stats.rows==105000 && stats.scopes==2500 && stats.bytes<=128u*1024u*1024u);
 for(size_t k=1;k<=64;k*=2){xide_workspace_symbol_result* result=xideWorkspaceSymbolSnapshotQuery(s,"rb",k,NULL,NULL,&e);CHECK(result && xideWorkspaceSymbolResultMatches(result)==12500 && xideWorkspaceSymbolResultCount(result)==k);for(size_t i=0;i<k;++i){xide_workspace_symbol_view v;CHECK(xideWorkspaceSymbolResultAt(result,i,&v) && !strcmp(v.declaration.name,"RB") && v.score==30000);}xideWorkspaceSymbolResultDestroy(result);}
 largeCount=stats.rows;largeSize=sizeof(symbol_entry);largeAllocs=0;refuseLargeAt=2;u=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);CHECK(u && xideWorkspaceSymbolIndexSet(u,"owner-0000",f->debug,f->changed,&e) && !xideWorkspaceSymbolIndexCommit(u,&e) && largeAllocs==2);refuseLargeAt=0;largeAllocs=0;observeMerge=1;mergeStarted=0;mergePolls=0;
 cancelWhileScratchLive=0;u=xideWorkspaceSymbolIndexBegin(index,cancelMerge,NULL,&e);CHECK(u && xideWorkspaceSymbolIndexSet(u,"owner-0000",f->debug,f->changed,&e) && !xideWorkspaceSymbolIndexCommit(u,&e) && mergeStarted && mergePolls>=5);CHECK(cancelWhileScratchLive && mergeReleased);observeMerge=0;largeCount=0;mergeStarted=0;mergeScratch=NULL;xide_workspace_symbol_snapshot* after=xideWorkspaceSymbolIndexAcquire(index);CHECK(xideWorkspaceSymbolSnapshotStats(after).generation==1 && xideWorkspaceSymbolSnapshotStats(after).rows==stats.rows);xideWorkspaceSymbolSnapshotRelease(after);
 u=xideWorkspaceSymbolIndexBegin(index,NULL,NULL,&e);CHECK(u && xideWorkspaceSymbolIndexSet(u,"owner-0000",s->parts[0]->scope,f->changed,&e) && xideWorkspaceSymbolIndexCommit(u,&e));after=xideWorkspaceSymbolIndexAcquire(index);CHECK(after->parts[0]!=s->parts[0] && after->stats.generation==2);for(size_t i=1;i<s->stats.parts;++i)CHECK(after->parts[i]==s->parts[i]);xideWorkspaceSymbolSnapshotRelease(after);
 printf("Workspace index scale: %zu rows,%zu scopes,%zu known retained bytes; merge refusal/cancellation preserve old data;one changed shard,2499 unchanged shard pointers reused.\n",stats.rows,stats.scopes,stats.bytes);xideWorkspaceSymbolSnapshotRelease(s);xideWorkspaceSymbolIndexDestroy(index);
 index=xideWorkspaceSymbolIndexCreate(NULL);CHECK(index && setOne(index,"a",f->debug,f->original));s=xideWorkspaceSymbolIndexAcquire(index);HANDLE readers[4];for(size_t i=0;i<4;++i){readers[i]=CreateThread(NULL,0,readSnapshot,xideWorkspaceSymbolSnapshotRetain(s),0,NULL);CHECK(readers[i]);}CHECK(setOne(index,"a",f->debug,f->changed));xideWorkspaceSymbolIndexDestroy(index);CHECK(WaitForMultipleObjects(4,readers,TRUE,10000)==WAIT_OBJECT_0);for(size_t i=0;i<4;++i){DWORD code=1;CHECK(GetExitCodeThread(readers[i],&code) && !code);CloseHandle(readers[i]);}xideWorkspaceSymbolSnapshotRelease(s);return 1;
}
int main(void)
{fixture f={0};if(!setup(&f) || !basics(&f) || !faults(&f) || !cancellations(&f) || !budgets(&f) || !reentrancy(&f) || !scale(&f))return 1;clear(&f);if(wideKeys)return 1;printf("workspace_symbol_index_test passed: %zu checks; real checked declarations,atomic publication,old snapshots,scope collision isolation,deduplication,UTF8 DP membership,best-K,refusals,cancellation,budgets,reentrant publication,105k rows and concurrent lifetime.\n",checks);return 0;}
