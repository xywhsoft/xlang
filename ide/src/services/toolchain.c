#include "services/toolchain.h"
#include "platform/process.h"
#include "platform/path.h"
#include <windows.h>
#include <winver.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#define HELP_LIMIT 65535u
#define CACHE_COUNT 16u
#define CACHE_AGE_MS (5u*60u*1000u)
#define QUERY_TIMEOUT_MS 15000u
typedef struct cache_item { xide_toolchain_profile profile; uint64_t tick; } cache_item;
struct xide_toolchains { CRITICAL_SECTION lock; cache_item items[CACHE_COUNT]; size_t next; };
struct xide_toolchain_request {
 CRITICAL_SECTION lock; HANDLE worker; volatile LONG cancel;
 xide_toolchains* cache; char *compiler,*directory; int force;
 enum xide_toolchain_request_state state; char error[512]; xide_toolchain_profile result;
};
static int fail(xide_model_error* error,const char* message) { if(error) { error->code=XIDE_MODEL_INVALID; snprintf(error->message,sizeof(error->message),"%s",message); } return 0; }
static char* copy(const char* s) { if(!s) return NULL; size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static wchar_t* wide(const char* s) { int n=s ? MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0) : 0; wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL; if(w && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n)) { free(w); w=NULL; } return w; }
void xideToolchainProfileClear(xide_toolchain_profile* p) { if(p) { free(p->compiler); free(p->directory); free(p->help); memset(p,0,sizeof(*p)); } }
int xideToolchainProfileCopy(xide_toolchain_profile* out,const xide_toolchain_profile* from)
{
 if(!out || !from || !from->compiler || !from->directory || !from->help) return 0;
 if(out==from) return 1;
 xide_toolchain_profile p=*from; p.compiler=copy(from->compiler); p.directory=copy(from->directory); p.help=copy(from->help);
 if(!p.compiler || !p.directory || !p.help) { xideToolchainProfileClear(&p); return 0; }
 xideToolchainProfileClear(out); *out=p; return 1;
}
static int token(const char* line,const char* name)
{
 size_t n=strlen(name);
 for(const char* p=line;(p=strstr(p,name))!=NULL;++p) {
  int before=p==line || isspace((unsigned char)p[-1]) || strchr("[|(,",p[-1]);
  int after=!p[n] || isspace((unsigned char)p[n]) || strchr("]|),=",p[n]);
  if(before && after) return 1;
 }
 return 0;
}
int xideToolchainParseHelp(const char* help,size_t length,uint32_t* commands,uint32_t* options,xide_model_error* error)
{
 if(error) memset(error,0,sizeof(*error));
 if(!help || !length || length>HELP_LIMIT || !commands || !options || memchr(help,0,length) || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,help,(int)length,NULL,0)) return fail(error,"Compiler help is empty, oversized, contains NUL, or is not valid UTF-8.");
 char* text=malloc(length+1); if(!text) return fail(error,"Cannot allocate compiler help parser."); memcpy(text,help,length); text[length]=0;
 uint32_t c=0,o=0; const char* names[]={"build","run","check","lex","parse","ir","cgen","abi","impdef","c-bridge"};
 for(char* line=text;line && *line;) {
  char* next=strchr(line,'\n'); if(next) *next++=0;
  while(*line && isspace((unsigned char)*line)) ++line;
  if(!strncmp(line,"xl",2) && isspace((unsigned char)line[2])) { line+=2; while(*line && isspace((unsigned char)*line)) ++line; }
  for(unsigned i=0;i<10;++i) {
   size_t n=strlen(names[i]); if(strncmp(line,names[i],n) || (line[n] && !isspace((unsigned char)line[n]))) continue;
   c|=1u<<i;
   if(i==0) { const char* flags[]={"-o","--inc-path","--lib-path","--link","--res","-shared","--save-temps","--emit-header","--backend"}; for(unsigned j=0;j<9;++j) if(token(line,flags[j])) o|=1u<<j; if(token(line,"--shared")) o|=XIDE_TOOL_SHARED; if(token(line,"--input-report")) o|=XIDE_TOOL_BUILD_INPUT_REPORT; }
   if(i==2) { if(token(line,"--json")) o|=XIDE_TOOL_CHECK_JSON; if(token(line,"--source-overlay")) o|=XIDE_TOOL_CHECK_OVERLAY; if(token(line,"--source-report")) o|=XIDE_TOOL_CHECK_SOURCE_REPORT; if(token(line,"--semantic-index")) o|=XIDE_TOOL_CHECK_SEMANTIC_INDEX; if(token(line,"--completion-at")) o|=XIDE_TOOL_CHECK_COMPLETION; }
   if(i==7 && token(line,"--json")) o|=XIDE_TOOL_ABI_JSON;
   if(i==9) { if(token(line,"--json")) o|=XIDE_TOOL_CBRIDGE_JSON; if(token(line,"--c-environment")) o|=XIDE_TOOL_CBRIDGE_ENVIRONMENT; }
   break;
  }
  line=next;
 }
 free(text); *commands=c; *options=o; return 1;
}
int xideToolchainCapture(const char* compiler,xide_toolchain_stamp* out,xide_model_error* error)
{
 if(error) memset(error,0,sizeof(*error));
 if(!compiler || !*compiler || !out) return fail(error,"A compiler path is required.");
 xide_toolchain_stamp stamp={0}; char* base=xidePathDirectory(compiler); if(!base) return fail(error,"Cannot prepare the compiler directory.");
 const char* siblings[]={"xlc.dll","xl.dll"}; int okay=1;
 for(int i=0;i<3 && okay;++i) {
  char* path=i ? xidePathResolveLexical(base,siblings[i-1]) : copy(compiler); wchar_t* w=wide(path);
  if(!path || !w) { free(path); free(w); okay=fail(error,"Cannot prepare toolchain identity paths."); break; }
  DWORD attributes=GetFileAttributesW(w),code=GetLastError(); free(w);
  if(i && attributes==INVALID_FILE_ATTRIBUTES && (code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND)) stamp.present[i]=0;
  else { stamp.present[i]=1; okay=xideFileSnapshotInspect(path,&stamp.files[i],error); }
  free(path);
 }
 free(base); if(okay) *out=stamp; return okay;
}
int xideToolchainStampEqual(const xide_toolchain_stamp* a,const xide_toolchain_stamp* b)
{ if(!a || !b) return 0; for(int i=0;i<3;++i) if(a->present[i]!=b->present[i] || (a->present[i] && !xideFileSnapshotEqual(&a->files[i],&b->files[i]))) return 0; return 1; }
int xideToolchainValidate(const char* compiler,const xide_toolchain_stamp* stamp,xide_model_error* error)
{ xide_toolchain_stamp now={0}; if(!xideToolchainCapture(compiler,&now,error)) return 0; return xideToolchainStampEqual(stamp,&now) ? 1 : fail(error,"Compiler or sibling runtime files changed; request a fresh toolchain query/build."); }
xide_toolchains* xideToolchainsCreate(void) { xide_toolchains* cache=calloc(1,sizeof(*cache)); if(cache) InitializeCriticalSection(&cache->lock); return cache; }
void xideToolchainsDestroy(xide_toolchains* cache) { if(cache) { for(size_t i=0;i<CACHE_COUNT;++i) xideToolchainProfileClear(&cache->items[i].profile); DeleteCriticalSection(&cache->lock); free(cache); } }
static void fileVersion(const char* compiler,char result[64])
{
 wchar_t* w=wide(compiler); if(!w) return; DWORD ignored=0,n=GetFileVersionInfoSizeW(w,&ignored); void* data=n && n<=4u*1024u*1024u ? malloc(n) : NULL;
 if(data && GetFileVersionInfoW(w,0,n,data)) { VS_FIXEDFILEINFO* info=NULL; UINT count=0; if(VerQueryValueW(data,L"\\",(void**)&info,&count) && count>=sizeof(*info) && info->dwSignature==0xfeef04bdu) snprintf(result,64,"%u.%u.%u.%u",HIWORD(info->dwFileVersionMS),LOWORD(info->dwFileVersionMS),HIWORD(info->dwFileVersionLS),LOWORD(info->dwFileVersionLS)); }
 free(data); free(w);
}
static uint64_t fileTime(void) { FILETIME ft; GetSystemTimeAsFileTime(&ft); return ((uint64_t)ft.dwHighDateTime<<32)|ft.dwLowDateTime; }
int xideToolchainObserve(xide_toolchains* cache,const char* compiler,const char* directory,const xide_toolchain_stamp* before,const char* help,size_t length,xide_toolchain_profile* out,xide_model_error* error)
{
 if(!cache || !compiler || !directory || !before || !out) return fail(error,"Cannot record a toolchain observation without its captured request.");
 xide_toolchain_profile p={0}; if(!xideToolchainParseHelp(help,length,&p.commands,&p.options,error)) return 0;
 fileVersion(compiler,p.fileVersion); if(!xideToolchainValidate(compiler,before,error)) return 0;
 p.compiler=copy(compiler); p.directory=copy(directory); p.help=malloc(length+1); p.stamp=*before; p.observedAt=fileTime();
 if(!p.compiler || !p.directory || !p.help) { xideToolchainProfileClear(&p); return fail(error,"Cannot allocate the toolchain observation."); }
 memcpy(p.help,help,length); p.help[length]=0;
 EnterCriticalSection(&cache->lock); size_t slot=CACHE_COUNT;
 for(size_t i=0;i<CACHE_COUNT;++i) if(cache->items[i].profile.compiler && !strcmp(cache->items[i].profile.compiler,compiler) && !strcmp(cache->items[i].profile.directory,directory)) { slot=i; break; }
 if(slot==CACHE_COUNT) { slot=cache->next++%CACHE_COUNT; }
 int okay=xideToolchainProfileCopy(&cache->items[slot].profile,&p); if(okay) cache->items[slot].tick=GetTickCount64(); LeaveCriticalSection(&cache->lock);
 if(!okay) { xideToolchainProfileClear(&p); return fail(error,"Cannot retain the toolchain cache entry."); }
 xideToolchainProfileClear(out); *out=p; return 1;
}
static int cached(xide_toolchains* cache,const char* compiler,const char* directory,const xide_toolchain_stamp* stamp,xide_toolchain_profile* out)
{
 int found=0; EnterCriticalSection(&cache->lock);
 for(size_t i=0;i<CACHE_COUNT;++i) { cache_item* item=&cache->items[i];
  if(item->profile.compiler && !strcmp(item->profile.compiler,compiler) && !strcmp(item->profile.directory,directory) && GetTickCount64()-item->tick<=CACHE_AGE_MS && xideToolchainStampEqual(stamp,&item->profile.stamp)) { found=xideToolchainProfileCopy(out,&item->profile); if(found) out->cached=1; break; }
 }
 LeaveCriticalSection(&cache->lock); return found;
}
static int cancelled(xide_toolchain_request* q) { return InterlockedCompareExchange(&q->cancel,0,0)!=0; }
static DWORD WINAPI queryWorker(void* user)
{
 xide_toolchain_request* q=user; xide_toolchain_profile p={0}; xide_model_error error={0}; xide_toolchain_stamp stamp={0}; char help[HELP_LIMIT+1]={0}; size_t used=0; xide_process* process=NULL; int success=0; uint64_t began=GetTickCount64();
 if(cancelled(q) || !xideToolchainCapture(q->compiler,&stamp,&error)) goto done;
 if(!q->force && cached(q->cache,q->compiler,q->directory,&stamp,&p)) { success=1; goto done; }
 const char* args[]={"--help"}; xide_process_spec spec={.program=q->compiler,.directory=q->directory,.arguments=args,.argumentCount=1};
 process=xideProcessStart(&spec,error.message,sizeof(error.message)); if(!process) goto done;
 int stop=0; xide_process_info info={0}; uint64_t processBegan=GetTickCount64();
 for(;;) {
  if(cancelled(q) || GetTickCount64()-processBegan>QUERY_TIMEOUT_MS) { xideProcessCancel(process); stop=1; if(!cancelled(q)) fail(&error,"Compiler help query exceeded the 15 second limit."); }
  char bytes[8192]; size_t n=xideProcessRead(process,bytes,sizeof(bytes));
  if(n) { if(n>HELP_LIMIT-used) { xideProcessCancel(process); stop=1; fail(&error,"Compiler help exceeds the 64 KiB query limit."); } else if(!stop) { memcpy(help+used,bytes,n); used+=n; help[used]=0; } }
  xideProcessGetInfo(process,&info); if(info.state==XIDE_PROCESS_FINISHED && !n) break; if(!n) Sleep(5);
 }
 if(stop || cancelled(q)) goto done;
 if(!info.started || info.ioError || info.droppedBytes || info.exit.Kind!=XPROCESS_EXIT_CODE || info.exit.Code!=0) { fail(&error,*info.error ? info.error : "Compiler --help failed or its output was incomplete."); goto done; }
 success=xideToolchainObserve(q->cache,q->compiler,q->directory,&stamp,help,used,&p,&error);
done:
 if(process) { xideProcessDestroy(process); } p.durationMs=GetTickCount64()-began;
 EnterCriticalSection(&q->lock); q->state=cancelled(q) ? XIDE_TOOL_QUERY_CANCELLED : success ? XIDE_TOOL_QUERY_READY : XIDE_TOOL_QUERY_FAILED;
 if(q->state==XIDE_TOOL_QUERY_READY) { q->result=p; memset(&p,0,sizeof(p)); } else snprintf(q->error,sizeof(q->error),"%s",cancelled(q) ? "工具链查询已取消。" : *error.message ? error.message : "工具链查询未完成。");
 LeaveCriticalSection(&q->lock); xideToolchainProfileClear(&p); return 0;
}
xide_toolchain_request* xideToolchainRequest(xide_toolchains* cache,const char* compiler,const char* directory,int force,xide_model_error* error)
{
 if(error) { memset(error,0,sizeof(*error)); } if(!cache || !compiler || !*compiler || !directory || !*directory) { fail(error,"Select a compiler before querying its profile."); return NULL; }
 xide_toolchain_request* q=calloc(1,sizeof(*q)); if(!q) { fail(error,"Cannot allocate toolchain query."); return NULL; }
 InitializeCriticalSection(&q->lock); q->cache=cache; q->compiler=xidePathResolve(NULL,compiler); q->directory=xidePathResolve(NULL,directory); q->force=!!force;
 if(q->compiler && q->directory) q->worker=CreateThread(NULL,0,queryWorker,q,0,NULL);
 if(!q->worker) { xideToolchainRequestDestroy(q); fail(error,"Cannot prepare the background toolchain query."); return NULL; } return q;
}
enum xide_toolchain_request_state xideToolchainRequestState(xide_toolchain_request* q,char* message,size_t capacity)
{ if(!q) return XIDE_TOOL_QUERY_FAILED; EnterCriticalSection(&q->lock); enum xide_toolchain_request_state state=q->state; if(message && capacity) snprintf(message,capacity,"%s",q->error); LeaveCriticalSection(&q->lock); return state; }
int xideToolchainRequestResult(xide_toolchain_request* q,xide_toolchain_profile* out)
{ if(!q || !out) return 0; EnterCriticalSection(&q->lock); int okay=q->state==XIDE_TOOL_QUERY_READY && xideToolchainProfileCopy(out,&q->result); LeaveCriticalSection(&q->lock); return okay; }
void xideToolchainRequestCancel(xide_toolchain_request* q) { if(q) InterlockedExchange(&q->cancel,1); }
void xideToolchainRequestDestroy(xide_toolchain_request* q) { if(q) { xideToolchainRequestCancel(q); if(q->worker) { WaitForSingleObject(q->worker,INFINITE); CloseHandle(q->worker); } free(q->compiler); free(q->directory); xideToolchainProfileClear(&q->result); DeleteCriticalSection(&q->lock); free(q); } }
