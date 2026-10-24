#include "services/language_check.h"
#include "platform/process.h"
#include "platform/path.h"
#include "editor/coordinates.h"
#include "protocol/lsp_wire.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define TEXT_LIMIT (8u*1024u*1024u)
#define CHECK_OUTPUT_LIMIT (1024u*1024u)
#include "services/language_check_internal.h"
#ifdef XLS_COMPILER_XRT
#include "analysis/frontend_context.h"
#endif
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* o,const char* k) { return o ? xrtValueObjectGet(o,sv(k)) : NULL; }
static const char* string(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) && s.Size<=INT_MAX && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static int put(xvalue* o,const char* k,xvalue* v) { return v && xrtValueObjectSetNew(o,sv(k),v); }
static int fail(xide_model_error* e,const char* s) { if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int cancelled(xide_language_check* q) { return InterlockedCompareExchange(&q->cancel,0,0)!=0 || (q->externalCancel && q->externalCancel(q->externalUser)); }
int xideLanguageCheckRunCompiler(xide_language_check* q,const char* const* args,size_t count,size_t limit,char** output,int* exitCode,xide_model_error* e)
{
 xprocessenv env={.Name="XLANG_PACKAGE_PATH",.Value=q->packagePath};
 xide_process_spec spec={.program=q->compiler,.directory=q->directory,.arguments=args,.argumentCount=count,.separateStderr=1,.stdoutCapacity=limit>1024u*1024u ? limit : 1024u*1024u,.environment=q->packagePathCaptured ? &env : NULL,.environmentCount=q->packagePathCaptured ? 1 : 0}; xide_process* p=q->nativeBridge ? xideProcessStartExtended(&spec,e->message,sizeof(e->message)) : xideProcessStart(&spec,e->message,sizeof(e->message)); if(!p) return 0;
 char* bytes=malloc(limit+1); size_t used=0; char tail[512]={0}; int okay=bytes!=NULL; if(!okay) fail(e,"无法分配编译器检查缓冲区。"); uint64_t began=GetTickCount64(); xide_process_info info={0};
 for(;;) {
  if(cancelled(q) || !okay || GetTickCount64()-began>30000u) { xideProcessCancel(p); if(!cancelled(q) && okay) { fail(e,"源码检查超过三十秒限制。"); okay=0; } }
  char block[16384]; size_t n=xideProcessRead(p,block,sizeof(block)); if(n && okay) { if(n>limit-used) { fail(e,"编译器检查输出超过限制。"); okay=0; } else { memcpy(bytes+used,block,n); used+=n; } }
  char stderrBytes[512]; size_t errorSize=xideProcessReadError(p,stderrBytes,sizeof(stderrBytes)-1); if(errorSize) { memcpy(tail,stderrBytes,errorSize); tail[errorSize]=0; }
  xideProcessGetInfo(p,&info); if(info.state==XIDE_PROCESS_FINISHED && !n && !errorSize) break; if(!n && !errorSize) Sleep(5);
 }
 if(cancelled(q)) okay=0;
 if(okay && (!info.started || info.ioError || info.droppedBytes || info.exit.Kind!=XPROCESS_EXIT_CODE)) { fail(e,*info.error ? info.error : "编译器输出不完整或异常退出。"); okay=0; }
 if(*tail && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,tail,-1,NULL,0)) strcpy(tail,"stderr contains binary or split UTF-8");
 if(okay && info.exit.Code!=0 && info.exit.Code!=1) { snprintf(e->message,sizeof(e->message),"编译器退出码 %d：%.380s",info.exit.Code,tail); e->code=XIDE_MODEL_INVALID; okay=0; }
 if(okay) { bytes[used]=0; if(memchr(bytes,0,used) || (used && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes,(int)used,NULL,0))) { fail(e,"编译器输出不是有效 UTF-8。"); okay=0; } }
 if(okay) { *output=bytes; *exitCode=info.exit.Code; bytes=NULL; } free(bytes); xideProcessDestroy(p); return okay;
}
static int source(xide_language_check* q,xide_file_snapshot* baseline,int compare,xide_model_error* e)
{
 xide_file_snapshot now={0}; xide_text_file file={0}; int okay=xideFileSnapshotInspect(q->path,&now,e);
 if(okay && now.length>TEXT_LIMIT) okay=fail(e,"保存后检查的源码超过 8 MiB 限制。");
 if(okay) okay=xideTextFileRead(q->path,&file,e);
 if(okay && strcmp(file.text,q->text)) okay=fail(e,"磁盘源码与提交的保存版本不一致；请先保存或重新打开。");
 if(okay && compare && !xideFileSnapshotEqual(&now,baseline)) okay=fail(e,"检查期间源码文件内容或身份已经变化。");
 if(okay && !compare) *baseline=now;
 xideTextFileClear(&file); return okay;
}
static xvalue* position(int line,int character)
{ xvalue* o=xrtValueObject(); if(!o || !put(o,"line",xrtValueInt(line)) || !put(o,"character",xrtValueInt(character))) { xrtValueRelease(o); return NULL; } return o; }
static xvalue* diagnostics(xide_language_check* q,const xvalue* raw,xide_model_error* e)
{
 const xvalue* list=get(raw,"diagnostics"); if(!list || xrtValueType(list)!=XVALUE_ARRAY || xrtValueCount(list)>4096) { fail(e,"编译器诊断数组无效或超过限制。"); return NULL; }
 xvalue* out=xrtValueArray(); if(!out) return NULL;
 for(size_t i=0;i<xrtValueCount(list);++i) {
  if(cancelled(q)) { xrtValueRelease(out); fail(e,"源码检查已取消。"); return NULL; }
  const xvalue* d=xrtValueArrayGet(list,i); const char *file=string(get(d,"file")),*message=string(get(d,"message")); int64 line=0,column=0;
  if(!file || !message || !*message || strlen(message)>65535 || !xrtValueGetInt(get(d,"line"),&line) || !xrtValueGetInt(get(d,"column"),&column) || line<1 || column<1 || line>INT_MAX || column>INT_MAX) goto invalid;
  char* absolute=xidePathResolveLexical(q->directory,file); int same=absolute && xidePathEqual(absolute,q->path); free(absolute); if(!same) goto invalid;
  int offset=xidePositionToOffset(q->text,(int)line,(int)column),actualLine=0,actualColumn=0; xidePositionFromOffset(q->text,offset,&actualLine,&actualColumn); if(actualLine!=line || actualColumn!=column) goto invalid;
  int end=offset; if(q->text[end] && q->text[end]!='\n') { ++end; while(((unsigned char)q->text[end]&0xc0)==0x80) ++end; }
  int sl=0,sc=0,el=0,ec=0; if(!xideLspOffsetToPosition(q->text,offset,&sl,&sc) || !xideLspOffsetToPosition(q->text,end,&el,&ec)) goto invalid;
  xvalue *range=xrtValueObject(),*item=xrtValueObject(); int okay=range && item && put(range,"start",position(sl,sc)) && put(range,"end",position(el,ec));
  if(okay) { okay=put(item,"range",range); range=NULL; }
  if(okay) okay=put(item,"severity",xrtValueInt(1)) && put(item,"message",xrtValueString(sv(message))) && put(item,"source",xrtValueString(sv("xlang saved entry"))) && xrtValueArrayAppend(out,item);
  xrtValueRelease(range); xrtValueRelease(item); if(!okay) goto invalid;
 }
 return out;
invalid: xrtValueRelease(out); fail(e,"编译器诊断位置、来源或字段无法验证。"); return NULL;
}
static DWORD WINAPI worker(void* user)
{
 xide_language_check* q=user; xide_model_error e={0}; xide_toolchain_stamp stamp={0}; xide_toolchain_profile profile={0}; xide_file_snapshot file={0}; char *help=NULL,*json=NULL; xvalue *raw=NULL,*result=NULL,*list=NULL; int code=0,success=0; uint64_t began=GetTickCount64();
 if(q->snapshotCount) { success=xideLanguageSnapshotRun(q,&result,&e); goto done; }
 if(cancelled(q) || !source(q,&file,0,&e) || !xideToolchainCapture(q->compiler,&stamp,&e)) goto done;
 const char* helpArgs[]={"--help"}; if(!xideLanguageCheckRunCompiler(q,helpArgs,1,65535,&help,&code,&e) || code!=0 || !xideToolchainObserve(q->cache,q->compiler,q->directory,&stamp,help,strlen(help),&profile,&e)) goto done;
 if(!(profile.commands&XIDE_TOOL_CHECK) || !(profile.options&XIDE_TOOL_CHECK_JSON)) { fail(&e,"当前编译器未声明 check --json；保存后诊断不可用。"); goto done; }
 const char* args[]={"check",q->path,"--json"}; if(cancelled(q) || !xideLanguageCheckRunCompiler(q,args,3,CHECK_OUTPUT_LIMIT,&json,&code,&e)) goto done;
 raw=xrtJsonParse(sv(json)); bool okay=false; const char* phase=string(get(raw,"phase"));
 if(!raw || xrtValueType(raw)!=XVALUE_OBJECT || !xrtValueGetBool(get(raw,"ok"),&okay) || okay!=(code==0) || !phase || !*phase || strlen(phase)>64) { fail(&e,"编译器检查响应无效或退出状态不一致。"); goto done; }
 list=diagnostics(q,raw,&e); if(!list || (!okay && !xrtValueCount(list))) { if(!e.code) fail(&e,"检查失败但没有可验证的诊断。"); goto done; }
 if(!source(q,&file,1,&e) || !xideToolchainValidate(q->compiler,&stamp,&e) || cancelled(q)) goto done;
 result=xrtValueObject(); success=result && put(result,"ok",xrtValueBool(okay)) && put(result,"phase",xrtValueString(sv(phase))) && put(result,"version",xrtValueInt(q->version)) && put(result,"durationMs",xrtValueInt((int64)(GetTickCount64()-began))) && xrtValueObjectSet(result,sv("diagnostics"),list);
done:
 free(help); free(json); xrtValueRelease(raw); xrtValueRelease(list); xideToolchainProfileClear(&profile);
 EnterCriticalSection(&q->lock); q->state=cancelled(q) ? XIDE_CHECK_CANCELLED : success ? XIDE_CHECK_READY : XIDE_CHECK_FAILED;
 if(q->state==XIDE_CHECK_READY) { q->result=result; result=NULL; if(q->snapshotCount) q->guardState=XIDE_GUARD_RUNNING; } else snprintf(q->error,sizeof(q->error),"%s",cancelled(q) ? "源码检查已取消。" : *e.message ? e.message : "未取得可验证的源码分析结果。");
 LeaveCriticalSection(&q->lock); xrtValueRelease(result);
 if(q->wake) q->wake(q->wakeUser);
 if(success && q->snapshotCount) while(WaitForSingleObject(q->stopEvent,1000)==WAIT_TIMEOUT) {
  xide_model_error changed={0}; if(xideLanguageSnapshotGuard(q,&changed)) continue;
  if(!cancelled(q)) { EnterCriticalSection(&q->lock); q->guardState=XIDE_GUARD_INVALID; snprintf(q->guardError,sizeof(q->guardError),"%s",*changed.message ? changed.message : "发布后检查输入无法复核。"); LeaveCriticalSection(&q->lock); if(q->wake) q->wake(q->wakeUser); }
  break;
 }
 return 0;
}
xide_language_check* xideLanguageCheckStart(xide_toolchains* cache,const char* compiler,const char* directory,const char* path,const char* text,int version,xide_model_error* e)
{
 if(!cache || !compiler || !directory || !path || !text || strlen(text)>TEXT_LIMIT || !xidePathHasExtension(path,".xl")) { fail(e,"保存后检查需要命名的 .xl 文件、编译器和有界正文。"); return NULL; }
 xide_language_check* q=calloc(1,sizeof(*q)); if(!q) return NULL; InitializeCriticalSection(&q->lock); q->cache=cache; q->compiler=xidePathResolveLexical(NULL,compiler); q->directory=xidePathResolveLexical(NULL,directory); q->path=xidePathResolveLexical(q->directory,path); q->text=copy(text); q->version=version;
 if(q->compiler && q->directory && q->path && q->text) q->worker=CreateThread(NULL,0,worker,q,0,NULL);
 if(!q->worker) { xideLanguageCheckDestroy(q); fail(e,"无法创建后台源码检查。"); return NULL; } return q;
}
enum xide_check_state xideLanguageCheckState(xide_language_check* q,char* message,size_t capacity)
{ if(!q) return XIDE_CHECK_FAILED; EnterCriticalSection(&q->lock); enum xide_check_state state=q->state; if(message && capacity) snprintf(message,capacity,"%s",q->error); LeaveCriticalSection(&q->lock); return state; }
xvalue* xideLanguageCheckResult(xide_language_check* q) { if(!q) return NULL; EnterCriticalSection(&q->lock); xvalue* out=q->state==XIDE_CHECK_READY && !cancelled(q) && q->guardState!=XIDE_GUARD_INVALID ? xrtValueDeepClone(q->result) : NULL; LeaveCriticalSection(&q->lock); return out; }
enum xide_guard_state xideLanguageCheckGuardState(xide_language_check* q,char* message,size_t capacity)
{ if(!q) return XIDE_GUARD_NONE; EnterCriticalSection(&q->lock); enum xide_guard_state state=q->guardState; if(message && capacity) snprintf(message,capacity,"%s",q->guardError); LeaveCriticalSection(&q->lock); return state; }
struct xide_language_input_guard {
 xatomic32 references,invalid; char *compiler,*directory,*packagePath;
 int nativeBackend; xvalue *files,*resolution; xide_toolchain_stamp toolchain;
#ifdef XLS_COMPILER_XRT
 xls_context* context;
#endif
};
xide_language_input_guard* xideLanguageInputGuardRetain(xide_language_input_guard* guard)
{ if(guard) xrtAtomic32FetchAdd(&guard->references,1,XMEMORY_RELAXED); return guard; }
void xideLanguageInputGuardRelease(xide_language_input_guard* guard)
{
 if(!guard || xrtAtomic32FetchSub(&guard->references,1,XMEMORY_ACQ_REL)!=1) return;
 xrtFree(guard->compiler); xrtFree(guard->directory); xrtFree(guard->packagePath);
#ifdef XLS_COMPILER_XRT
 xlsContextRelease(guard->context);
#endif
 xrtValueRelease(guard->files); xrtValueRelease(guard->resolution); xrtFree(guard);
}
xide_language_input_guard* xideLanguageCheckCaptureInputGuard(xide_language_check* q,xide_model_error* error)
{
 if(!q) { fail(error,"No checked input proof is available."); return NULL; }
 xide_language_input_guard* guard=xrtCalloc(1,sizeof(*guard));
 if(!guard) { fail(error,"Unable to retain checked input proof."); return NULL; }
 xrtAtomic32Init(&guard->references,1); xrtAtomic32Init(&guard->invalid,0);
 EnterCriticalSection(&q->lock);
 int okay=q->state==XIDE_CHECK_READY && q->guardState==XIDE_GUARD_RUNNING && !cancelled(q) && q->guardFiles && q->guardResolution;
 if(okay) {
  guard->compiler=xrtStrDup(q->compiler); guard->directory=xrtStrDup(q->directory);
  if(q->packagePath) guard->packagePath=xrtStrDup(q->packagePath);
  guard->files=xrtValueDeepClone(q->guardFiles); guard->resolution=xrtValueDeepClone(q->guardResolution);
  guard->nativeBackend=q->nativeSemantic; guard->toolchain=q->guardToolchain;
#ifdef XLS_COMPILER_XRT
  guard->context=xlsContextRetain(q->context);
#endif
  okay=guard->compiler && guard->directory && (!q->packagePath || guard->packagePath) && guard->files && guard->resolution;
 }
 LeaveCriticalSection(&q->lock);
 if(!okay) { xideLanguageInputGuardRelease(guard); fail(error,"Unable to retain a current checked input proof."); return NULL; }
 return guard;
}
const xvalue* xideLanguageInputGuardFiles(const xide_language_input_guard* guard)
{ return guard ? guard->files : NULL; }
int xideLanguageInputGuardSameContext(const xide_language_input_guard* left,const xide_language_input_guard* right)
{
 if(!left || !right) return 0;
#ifdef XLS_COMPILER_XRT
 return xlsFrontendContextsEqual(left->context,right->context);
#else
 return 1;
#endif
}
const xls_context* xideLanguageInputGuardContext(const xide_language_input_guard* guard)
{
#ifdef XLS_COMPILER_XRT
 return guard ? guard->context : NULL;
#else
 (void)guard;return NULL;
#endif
}
typedef struct guard_path { const xvalue* value; wchar_t* native; } guard_path;
static int compareGuardPath(const void* a,const void* b)
{
 const guard_path *x=a,*y=b; int order=xidePathNativeKeyCompare(x->native,y->native);
 return order ? order : strcmp(string(x->value),string(y->value));
}
xvalue* xideLanguageInputGuardsPaths(const xide_language_input_guard* const* guards,size_t count,int (*stopped)(void*),void* user)
{
 if(count>64 || (count && !guards) || (stopped && stopped(user))) return NULL;
 size_t total=0,used=0;
 for(size_t g=0;g<count;++g) {
  if(!guards[g]) return NULL;
  size_t files=xrtValueCount(guards[g]->files),probes=xrtValueCount(get(guards[g]->resolution,"probes"));
  if(files>512 || probes>2048) return NULL;
  total+=files+probes;
 }
 guard_path* paths=xrtCalloc(total+1,sizeof(*paths)); xvalue* out=NULL;
 if(!paths) return NULL;
 for(size_t g=0;g<count;++g) {
 const xvalue* lists[]={guards[g]->files,get(guards[g]->resolution,"probes")};
 size_t counts[]={xrtValueCount(lists[0]),xrtValueCount(lists[1])};
 for(size_t i=0;i<2;++i) for(size_t j=0;j<counts[i];++j) {
  if(used%16==0 && stopped && stopped(user)) goto done;
  const xvalue* value=get(xrtValueArrayGet(lists[i],j),"path");const char* path=string(value);
  wchar_t* native=path ? xidePathNativeWide(path) : NULL;
  if(!native) goto done;
  paths[used++]=(guard_path){value,native};
 }
 }
 if(used) qsort(paths,used,sizeof(*paths),compareGuardPath);
 out=xrtValueArray();if(!out) goto done;
 for(size_t i=0;i<used;++i) {
  if((i%16==0 && stopped && stopped(user)) ||
     ((!i || xidePathNativeKeyCompare(paths[i-1].native,paths[i].native)) && !xrtValueArrayAppend(out,(xvalue*)paths[i].value))) {
   xrtValueRelease(out);out=NULL;goto done;
  }
 }
 if(stopped && stopped(user)) {xrtValueRelease(out);out=NULL;}
done:
 for(size_t i=0;i<used;++i) free(paths[i].native);
 xrtFree(paths);return out;
}
xvalue* xideLanguageInputGuardPaths(const xide_language_input_guard* guard,int (*stopped)(void*),void* user)
{ return guard ? xideLanguageInputGuardsPaths(&guard,1,stopped,user) : NULL; }
int xideLanguageInputGuardCurrent(const xide_language_input_guard* guard)
{ return guard && !xrtAtomic32Load(&guard->invalid,XMEMORY_ACQUIRE); }
void xideLanguageInputGuardInvalidate(xide_language_input_guard* guard)
{ if(guard) xrtAtomic32Store(&guard->invalid,1,XMEMORY_RELEASE); }
int xideLanguageInputGuardTouches(const xide_language_input_guard* guard,const char* path)
{
 if(!guard || !path) return 0;
 const xvalue* lists[]={guard->files,get(guard->resolution,"probes")};
 for(size_t i=0;i<2;++i) for(size_t j=0;j<xrtValueCount(lists[i]);++j) {
  const char* input=string(get(xrtValueArrayGet(lists[i],j),"path"));
  if(input && (!strcmp(input,path) || xidePathLexicalEqual(input,path))) return 1;
 }
 return 0;
}
static bool countGuardBytes(xbytesview data,void* user)
{ size_t* bytes=user; if(data.Size>SIZE_MAX-*bytes) return false; *bytes+=data.Size; return true; }
size_t xideLanguageInputGuardReportBytes(const xide_language_input_guard* guard)
{
 if(!guard) return SIZE_MAX;
 xjsonwriteconfig config; xrtJsonWriteConfigInit(&config); config.MaxOutputBytes=SIZE_MAX;
 size_t bytes=0;
 if(!xrtJsonWrite(guard->files,&config,countGuardBytes,&bytes) || !xrtJsonWrite(guard->resolution,&config,countGuardBytes,&bytes)) return SIZE_MAX;
#ifdef XLS_COMPILER_XRT
 size_t contextBytes=guard->context ? xlsContextBytes(guard->context) : 0;
 if(contextBytes>SIZE_MAX-bytes) return SIZE_MAX;
 bytes+=contextBytes;
#endif
 return bytes;
}
int xideLanguageInputGuardValidate(xide_language_input_guard* guard,int (*stopped)(void*),void* user,xide_model_error* error)
{
 if(!guard || xrtAtomic32Load(&guard->invalid,XMEMORY_ACQUIRE)) return fail(error,"Checked diagnostic inputs are no longer current.");
 if(stopped && stopped(user)) return fail(error,"Input validation was cancelled.");
 /* Each validation owns its cancellation view. Reports remain read-only;
    consumers never mutate a producer check or share its worker lifetime. */
 xide_language_check view={0}; view.compiler=guard->compiler; view.directory=guard->directory;
 view.packagePath=guard->packagePath; view.nativeSemantic=guard->nativeBackend;
#ifdef XLS_COMPILER_XRT
 view.context=guard->context;
#endif
 view.guardFiles=guard->files; view.guardResolution=guard->resolution; view.guardToolchain=guard->toolchain;
 view.externalCancel=stopped; view.externalUser=user;
 int okay=xideLanguageSnapshotGuard(&view,error);
 if(stopped && stopped(user)) return fail(error,"Input validation was cancelled.");
 if(!okay) xrtAtomic32Store(&guard->invalid,1,XMEMORY_RELEASE);
 return okay && !xrtAtomic32Load(&guard->invalid,XMEMORY_ACQUIRE);
}
int xideLanguageCheckWorkerFinished(xide_language_check* q) { return !q || !q->worker || WaitForSingleObject(q->worker,0)==WAIT_OBJECT_0; }
void* xideLanguageCheckWorkerSignal(xide_language_check* q) { return q ? q->worker : NULL; }
static const xvalue* reportedPath(const xvalue* items,const char* path)
{
 /* Most compiler paths have identical spelling: avoid preparing platform keys
    for every unrelated input. Alias fallback preserves existing path rules. */
 for(size_t i=0;i<xrtValueCount(items);++i) { const xvalue* item=xrtValueArrayGet(items,i); const char* name=string(get(item,"path")); if(name && !strcmp(name,path)) return item; }
 for(size_t i=0;i<xrtValueCount(items);++i) { const xvalue* item=xrtValueArrayGet(items,i); const char* name=string(get(item,"path")); if(name && xidePathLexicalEqual(name,path)) return item; }
 return NULL;
}
static xvalue* definitionProof(xide_language_check* q,const xvalue* location)
{
 const char* uri=string(get(location,"uri")); char* path=uri ? xideLspUriToPath(uri,NULL) : NULL;
 const xvalue* actual=path ? reportedPath(q->guardFiles,path) : NULL;
 const xvalue* group=actual ? reportedPath(get(q->result,"documents"),path) : NULL; free(path);
 if(!group) return NULL;
 const char* fields[]={"path","origin","version","normalizedSize","normalizedSha256","input"};
 xvalue *out=xrtValueObject(),*document=xrtValueObject(); int okay=out && document && put(out,"schema",xrtValueInt(1));
 for(size_t i=0;okay && i<sizeof(fields)/sizeof(*fields);++i) { const xvalue* value=get(group,fields[i]); if(value) okay=put(document,fields[i],xrtValueDeepClone(value)); }
 if(okay) { okay=put(out,"document",document); document=NULL; }
 xrtValueRelease(document); if(!okay) { xrtValueRelease(out); out=NULL; } return out;
}
static xvalue* definition(xide_language_check* q,const char* path,int version,int line,int character,int proof)
{
 if(!q || !path) return NULL;
 EnterCriticalSection(&q->lock); int match=0;
 for(size_t i=0;i<q->snapshotCount;++i) if(xidePathLexicalEqual(path,q->snapshots[i].path) && version==q->snapshots[i].version) match=1;
 xvalue* out=match && q->state==XIDE_CHECK_READY && !cancelled(q) && q->guardState==XIDE_GUARD_RUNNING && q->definitions ? xideSemanticDefinition(q->definitions,path,line,character) : NULL;
 if(proof && out && xrtValueType(out)!=XVALUE_NULL && !put(out,"xlang",definitionProof(q,out))) { xrtValueRelease(out); out=NULL; }
 LeaveCriticalSection(&q->lock); return out;
}
xvalue* xideLanguageCheckDefinition(xide_language_check* q,const char* path,int version,int line,int character)
{ return definition(q,path,version,line,character,0); }
xvalue* xideLanguageCheckDefinitionWithProof(xide_language_check* q,const char* path,int version,int line,int character)
{ return definition(q,path,version,line,character,1); }
xvalue* xideLanguageCheckHover(xide_language_check* q,const char* path,int version,int line,int character)
{
 if(!q || !path) return NULL;
 EnterCriticalSection(&q->lock); int match=0;
 for(size_t i=0;i<q->snapshotCount;++i) if(xidePathLexicalEqual(path,q->snapshots[i].path) && version==q->snapshots[i].version) match=1;
 xvalue* out=match && q->state==XIDE_CHECK_READY && !cancelled(q) && q->guardState==XIDE_GUARD_RUNNING && q->definitions ? xideSemanticHover(q->definitions,path,line,character) : NULL;
 LeaveCriticalSection(&q->lock); return out;
}
xvalue* xideLanguageCheckDocumentSymbols(xide_language_check* q,const char* path,int version)
{
 if(!q || !path) return NULL;
 EnterCriticalSection(&q->lock); int match=0;
 for(size_t i=0;i<q->snapshotCount;++i) if(xidePathLexicalEqual(path,q->snapshots[i].path) && version==q->snapshots[i].version) match=1;
 xvalue* out=match && q->state==XIDE_CHECK_READY && !cancelled(q) && q->guardState==XIDE_GUARD_RUNNING && q->definitions ? xideSemanticDocumentSymbols(q->definitions,path) : NULL;
 LeaveCriticalSection(&q->lock); return out;
}
xvalue* xideLanguageCheckDocumentHighlights(xide_language_check* q,const char* path,int version,int line,int character)
{
 if(!q || !path) return NULL;
 EnterCriticalSection(&q->lock); int match=0;
 for(size_t i=0;i<q->snapshotCount;++i) if(xidePathLexicalEqual(path,q->snapshots[i].path) && version==q->snapshots[i].version) match=1;
 xvalue* out=match && q->state==XIDE_CHECK_READY && !cancelled(q) && q->guardState==XIDE_GUARD_RUNNING && q->definitions ? xideSemanticDocumentHighlights(q->definitions,path,line,character) : NULL;
 LeaveCriticalSection(&q->lock); return out;
}
void xideLanguageCheckCancel(xide_language_check* q) { if(q) { InterlockedExchange(&q->cancel,1); if(q->stopEvent) SetEvent(q->stopEvent); } }
void xideLanguageCheckDestroy(xide_language_check* q)
{
 if(!q) return;
 xideLanguageCheckCancel(q); if(q->worker) { WaitForSingleObject(q->worker,INFINITE); CloseHandle(q->worker); }
 for(size_t i=0;i<q->snapshotCount;++i) {
  free((char*)q->snapshots[i].path);
#ifdef XLS_COMPILER_XRT
  free(q->snapshotKeys[i]);
  if(q->snapshotTexts[i]) xlsTextRelease(q->snapshotTexts[i]); else
#endif
  free((char*)q->snapshots[i].text);
 }
#ifdef XLS_COMPILER_XRT
 free(q->semanticTarget); xlsContextRelease(q->context);
#endif
 free(q->snapshots); if(q->stopEvent) CloseHandle(q->stopEvent); xrtValueRelease(q->guardFiles); xrtValueRelease(q->guardResolution); xideSemanticDefinitionsDestroy(q->definitions); free(q->queryPath); free(q->packagePath); free(q->compiler); free(q->directory); free(q->path); free(q->text); xrtValueRelease(q->result); DeleteCriticalSection(&q->lock); free(q);
}

static xide_language_check* createSnapshots(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,const char* queryPath,int line,int character,int diskEntry,xide_model_error* e)
{
 if(!cache || !compiler || !directory || !entry || (count && !documents) || (!diskEntry && !count) || count>64 || !xidePathHasExtension(entry,".xl")) { fail(e,"全文快照检查需要命名的 .xl 入口和至多 64 份文档。"); return NULL; }
 xide_language_check* q=calloc(1,sizeof(*q)); if(!q) return NULL;
 InitializeCriticalSection(&q->lock); q->cache=cache; q->compiler=xidePathResolveLexical(NULL,compiler); q->directory=xidePathResolveLexical(NULL,directory); q->path=xidePathResolveLexical(q->directory,entry); q->snapshots=calloc(count ? count : 1,sizeof(*q->snapshots));
 size_t bytes=0; int found=0;
 if(!q->compiler || !q->directory || !q->path || !q->snapshots) goto invalid;
 for(size_t i=0;i<count;++i) {
  const xide_language_snapshot* d=&documents[i]; size_t n=d->text ? strlen(d->text) : TEXT_LIMIT+1u;
  if(!d->path || !xidePathHasExtension(d->path,".xl") || n>TEXT_LIMIT || n>32u*1024u*1024u-bytes || (n && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,d->text,(int)n,NULL,0))) goto invalid;
  char *path=xidePathResolveLexical(q->directory,d->path),*text=copy(d->text); if(!path || !text) { free(path); free(text); goto invalid; }
  for(size_t j=0;j<i;++j) if(xidePathLexicalEqual(path,q->snapshots[j].path) || xidePathEqual(path,q->snapshots[j].path)) { free(path); free(text); goto invalid; }
  q->snapshots[q->snapshotCount++]=(xide_language_snapshot){path,text,d->version}; bytes+=n;
  if(xidePathLexicalEqual(path,q->path)) { q->version=d->version; found=1; }
 }
 if(!found && !diskEntry) goto invalid;
 if(queryPath) {
  q->queryPath=xidePathResolveLexical(q->directory,queryPath); found=0;
  if(!q->queryPath) goto invalid;
  for(size_t i=0;i<count;++i) if(xidePathLexicalEqual(q->queryPath,q->snapshots[i].path)) {
   const char* text=q->snapshots[i].text; int offset=0,bom=strlen(text)>=3 && !memcmp(text,"\xef\xbb\xbf",3) ? 3 : 0;
   if(!xideLspPositionToOffset(text,line,character,&offset) || offset<bom) goto invalid;
   q->queryOffset=offset-bom; q->queryVersion=q->snapshots[i].version; found=1;
  }
  if(!found) goto invalid;
 }
 return q;
invalid:
 xideLanguageCheckDestroy(q); fail(e,"无法创建全文快照检查：正文、路径别名、入口或容量无效。"); return NULL;
}

static xide_language_check* startSnapshots(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,const char* queryPath,int line,int character,const xide_language_completion_options* options,xide_model_error* e)
{
#ifndef XLS_COMPILER_XRT
 if(options && options->nativeBackend) { fail(e,"Native semantic analysis requires the standalone compiler-XRT host."); return NULL; }
#endif
 xide_language_check* q=createSnapshots(cache,compiler,directory,entry,documents,count,queryPath,line,character,0,e);
 if(!q) return NULL;
 if(options) { q->nativeSemantic=options->nativeBackend; q->wake=options->wake; q->wakeUser=options->user; }
 q->stopEvent=CreateEventW(NULL,TRUE,FALSE,NULL); if(q->stopEvent) q->worker=CreateThread(NULL,0,worker,q,0,NULL);
 if(q->worker) return q;
 xideLanguageCheckDestroy(q); fail(e,"Cannot start copied snapshot worker."); return NULL;
}

xide_language_check* xideLanguageCheckStartSnapshots(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,xide_model_error* e)
{ return xideLanguageCheckStartSnapshotsEx(cache,compiler,directory,entry,documents,count,NULL,e); }
xide_language_check* xideLanguageCheckStartSnapshotsEx(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,const xide_language_analysis_options* options,xide_model_error* e)
{ return startSnapshots(cache,compiler,directory,entry,documents,count,NULL,0,0,options,e); }
xide_language_check* xideLanguageCheckStartCompletionSnapshots(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,const char* queryPath,int line,int character,xide_model_error* e)
{ return xideLanguageCheckStartCompletionSnapshotsEx(cache,compiler,directory,entry,documents,count,queryPath,line,character,NULL,e); }
xide_language_check* xideLanguageCheckStartCompletionSnapshotsEx(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,const char* queryPath,int line,int character,const xide_language_completion_options* options,xide_model_error* e)
{ if(!queryPath) { fail(e,"补全查询需要打开文档路径。"); return NULL; } return startSnapshots(cache,compiler,directory,entry,documents,count,queryPath,line,character,options,e); }

#ifdef XLS_COMPILER_XRT
static int retainedLabel(const char* s)
{ size_t n=s ? strlen(s) : 0; return n && n<=262144 && xrtUtf8Valid((xstrview){s,n},NULL); }
static xide_language_check* startRetained(xls_context* context,int configured,int signatureMode,unsigned signatureFlags,xide_toolchains* cache,const char* compiler,
 const char* directory,xls_worker* actor,const xls_analysis_document* documents,size_t count,
 const char* target,const char* queryPath,int line,int character,
 const xide_language_analysis_options* options,xide_model_error* e)
{
 if((configured && !context) || !cache || !compiler || !directory || !documents || !count || count>64 || !retainedLabel(target) ||
    (!queryPath && !actor) || (queryPath && (line<0 || character<0)) || (options && !options->nativeBackend)) {
  fail(e,"Invalid retained native analysis request."); return NULL;
 }
 xide_language_check* q=calloc(1,sizeof(*q)); if(!q) return NULL;
 InitializeCriticalSection(&q->lock); q->cache=cache; q->nativeSemantic=q->retainedSnapshots=1; q->semanticActor=actor;
 q->context=xlsContextRetain(context);q->configuredEntry=configured;
 q->signatureMode=signatureMode;q->signatureFlags=signatureFlags;
 q->compiler=xidePathResolveLexical(NULL,compiler); q->directory=xidePathResolveLexical(NULL,directory);
 q->semanticTarget=configured ? NULL : copy(target);q->path=configured ? copy(xlsContextSpec(context)->entry) : NULL;
 q->snapshots=calloc(count,sizeof(*q->snapshots)); q->captureThread=xrtThreadCurrentId();
 if(!q->compiler || !q->directory || (!configured && !q->semanticTarget) || (configured && !q->path) || !q->snapshots) goto invalid;
 size_t bytes=0;int foundTarget=0;
 for(size_t i=0;i<count;++i) {
  const xls_analysis_document* d=&documents[i]; size_t n=xlsTextSize(d->text);
  if(!d->text || !retainedLabel(d->key) || !retainedLabel(d->path) || !xrtPathIsAbs(d->path) ||
     !xidePathHasExtension(d->path,".xl") || d->revision>UINT32_MAX || n>TEXT_LIMIT || n>32u*1024u*1024u-bytes) goto invalid;
  char *path=xidePathResolveLexical(q->directory,d->path),*key=copy(d->key);
  if(!path || !key) { free(path); free(key); goto invalid; }
  /* Host paths use '/', while native callers may have canonical XRT '\\'
     paths. Verify lexical identity, then keep the context's exact entry
     spelling for the core's immutable batch/context equality contract. */
  if(context && ((!configured && !strcmp(key,target)) || (configured && xidePathLexicalEqual(path,xlsContextSpec(context)->entry)))) {
   const char* entry=xlsContextSpec(context)->entry;
   if(!xidePathLexicalEqual(path,entry)) { free(path); free(key); goto invalid; }
   free(path);path=copy(entry);if(!path) {free(key);goto invalid;}
   if(configured) {if(q->semanticTarget) {free(path);free(key);goto invalid;}q->semanticTarget=copy(key);if(!q->semanticTarget) {free(path);free(key);goto invalid;}}
  }
  for(size_t j=0;j<i;++j) if(xidePathLexicalEqual(path,q->snapshots[j].path) || !strcmp(key,q->snapshotKeys[j])) { free(path); free(key); goto invalid; }
  q->snapshotKeys[i]=key; q->snapshotTexts[i]=xlsTextRetain(d->text); q->snapshotIdentities[i]=d->identity;
  q->snapshots[q->snapshotCount++]=(xide_language_snapshot){path,xlsTextData(d->text),(int)((int64_t)d->revision+INT_MIN)};
  bytes+=n;
  if(!strcmp(key,target)) { if(!configured) q->path=copy(path);q->version=q->snapshots[i].version;foundTarget=1;if(!q->path) goto invalid; }
 }
 if(!q->path || !foundTarget) goto invalid;
 if(!xlsFrontendContextValid(context,q->path,e ? e->message : NULL,e ? sizeof(e->message) : 0)) {
  xideLanguageCheckDestroy(q); if(e) e->code=XIDE_MODEL_INVALID; return NULL;
 }
 if(context && !xidePathLexicalEqual(q->directory,xlsContextSpec(context)->directory)) {
  xideLanguageCheckDestroy(q); fail(e,"Host build directory does not match the immutable native context."); return NULL;
 }
 if(queryPath) {
  q->queryPath=xidePathResolveLexical(q->directory,queryPath); int found=0;
  if(!q->queryPath) goto invalid;
  for(size_t i=0;i<count;++i) if(xidePathLexicalEqual(q->queryPath,q->snapshots[i].path)) {
   size_t offset=0,bom=xlsTextSize(q->snapshotTexts[i])>=3 && !memcmp(q->snapshots[i].text,"\xef\xbb\xbf",3) ? 3 : 0;
   if(!xlsTextOffset(q->snapshotTexts[i],(xls_position){(size_t)line,(size_t)character},&offset) || offset<bom) goto invalid;
   q->queryOffset=(int)(offset-bom); q->queryVersion=q->snapshots[i].version; found=1;
  }
  if(!found) goto invalid;
 }
 if(options) { q->wake=options->wake; q->wakeUser=options->user; }
 q->stopEvent=CreateEventW(NULL,TRUE,FALSE,NULL); if(q->stopEvent) q->worker=CreateThread(NULL,0,worker,q,0,NULL);
 if(q->worker) return q;
invalid:
 xideLanguageCheckDestroy(q); fail(e,"Cannot capture retained native analysis inputs."); return NULL;
}
xide_language_check* xideLanguageCheckStartRetainedInContext(xls_context* context,xide_toolchains* cache,const char* compiler,
 const char* directory,xls_worker* actor,const xls_analysis_document* documents,size_t count,
 const char* target,const char* queryPath,int line,int character,
 const xide_language_analysis_options* options,xide_model_error* error)
{ return startRetained(context,0,0,0,cache,compiler,directory,actor,documents,count,target,queryPath,line,character,options,error); }
xide_language_check* xideLanguageCheckStartConfigured(xls_context* context,xide_toolchains* cache,const char* compiler,
 xls_worker* actor,const xls_analysis_document* documents,size_t count,const char* target,const char* queryPath,int line,int character,
 const xide_language_analysis_options* options,xide_model_error* error)
{ return startRetained(context,1,0,0,cache,compiler,context ? xlsContextSpec(context)->directory : NULL,actor,documents,count,target,queryPath,line,character,options,error); }
xide_language_check* xideLanguageCheckStartSignature(xls_context* context,int configured,
 xide_toolchains* cache,const char* compiler,const char* directory,xls_worker* actor,
 const xls_analysis_document* documents,size_t count,const char* trigger,
 const char* queryPath,int line,int character,unsigned flags,
 const xide_language_analysis_options* options,xide_model_error* error)
{
 if(!queryPath || !actor || (flags&~3u)){fail(error,"Invalid signature query mode/options.");return NULL;}
 return startRetained(context,configured,1,flags,cache,compiler,directory,actor,documents,count,trigger,queryPath,line,character,options,error);
}
int xideLanguageCheckMatchesContext(const xide_language_check* check,const xls_context* context)
{ return check && xlsFrontendContextsEqual(check->context,context); }
xide_language_check* xideLanguageCheckStartRetained(xide_toolchains* cache,const char* compiler,
 const char* directory,xls_worker* actor,const xls_analysis_document* documents,size_t count,
 const char* target,const char* queryPath,int line,int character,
 const xide_language_analysis_options* options,xide_model_error* error)
{ return xideLanguageCheckStartRetainedInContext(NULL,cache,compiler,directory,actor,documents,count,target,queryPath,line,character,options,error); }
#endif

static int bridgeArgument(const char* path)
{ size_t n=path ? strlen(path) : 0; return n && n<=32767 && path[0]!='-' && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,(int)n,NULL,0); }
xide_language_check* xideLanguageBridgeRun(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,const xide_language_bridge_options* options,xide_model_error* e)
{
 if(count>64 || (count && !documents) || !options || options->includeCount>128 || options->cSourceCount>2048 || (options->includeCount && !options->includes) || (options->cSourceCount && !options->cSources) || (options->shared!=0 && options->shared!=1)) { fail(e,"Invalid native bridge configuration."); return NULL; }
 for(size_t i=0;i<count;++i) if(!documents || documents[i].version<=0) { fail(e,"Native bridge overlays require positive versions."); return NULL; }
 xide_language_check* q=createSnapshots(cache,compiler,directory,entry,documents,count,NULL,0,0,1,e); if(!q) return NULL;
 const char* args[2305]; size_t n=0; int okay=1;
 if(options->shared) args[n++]="-shared";
 for(size_t i=0;okay && i<options->includeCount;++i) { okay=bridgeArgument(options->includes[i]); if(okay) { args[n++]="--inc-path"; args[n++]=options->includes[i]; } }
 for(size_t i=0;okay && i<options->cSourceCount;++i) { okay=bridgeArgument(options->cSources[i]) && xidePathHasExtension(options->cSources[i],".c"); if(okay) args[n++]=options->cSources[i]; }
 q->nativeBridge=1; q->bridgeArgs=args; q->bridgeArgCount=n; q->externalCancel=options->cancelled; q->externalUser=options->user;
 if(okay) okay=xideLanguageSnapshotRun(q,&q->result,e);
 q->bridgeArgs=NULL; q->bridgeArgCount=0;
 if(!okay) { xideLanguageCheckDestroy(q); if(e && !*e->message) fail(e,"Native bridge query failed."); return NULL; }
 /* Retain report metadata, not a second copy of all overlay bodies. */
 for(size_t i=0;i<q->snapshotCount;++i) { free((char*)q->snapshots[i].text); q->snapshots[i].text=NULL; }
 q->state=XIDE_CHECK_READY; q->guardState=XIDE_GUARD_RUNNING; return q;
}
xvalue* xideLanguageBridgeTake(xide_language_check* q)
{
 if(!q || !q->nativeBridge || cancelled(q) || q->guardState!=XIDE_GUARD_RUNNING) return NULL;
 xvalue* out=q->result; q->result=NULL; return out;
}
int xideLanguageBridgeGuard(xide_language_check* q,xide_model_error* e)
{
 if(!q || !q->nativeBridge || q->guardState!=XIDE_GUARD_RUNNING || cancelled(q)) return fail(e,"Native bridge input lease is invalid.");
 if(xideLanguageSnapshotGuard(q,e)) return 1;
 q->guardState=XIDE_GUARD_INVALID; return 0;
}
