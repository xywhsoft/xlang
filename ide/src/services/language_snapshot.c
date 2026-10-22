/* Immutable open-document overlays and verified per-file diagnostics.
   Runtime is owned by XGE in the GUI, compiler XRT in the standalone LS. */
#include "services/language_check_internal.h"
#include "services/semantic_completion.h"
#include "services/c_bridge_map.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include "editor/coordinates.h"
#include "protocol/lsp_wire.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#ifdef XLS_COMPILER_XRT
#include "analysis/semantic.h"
#include "analysis/check.h"
static int nativeCancelled(void* user);
#endif
#define FILE_LIMIT (8u*1024u*1024u)
#define GRAPH_LIMIT (64u*1024u*1024u)
#define OUTPUT_LIMIT (4u*1024u*1024u)
typedef struct source_item { char *path,*text,*original; const xvalue* report; int overlay; } source_item;
typedef struct source_graph { source_item* items; size_t count,bytes; const xvalue* resolution; } source_graph;
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* o,const char* key) { return o && xrtValueType(o)==XVALUE_OBJECT ? xrtValueObjectGet(o,sv(key)) : NULL; }
static const char* string(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) && s.Size<=INT_MAX && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static int put(xvalue* o,const char* key,xvalue* value) { return value && xrtValueObjectSetNew(o,sv(key),value); }
static int fail(xide_model_error* e,const char* message) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s",message); return 0; }
static int cancelled(xide_language_check* q) { return InterlockedCompareExchange(&q->cancel,0,0)!=0 || (q->externalCancel && q->externalCancel(q->externalUser)); }
#ifdef XLS_COMPILER_XRT
static int nativeCancelled(void* user) { return cancelled(user); }
static void nativeQueryWake(void* user) { SetEvent((HANDLE)user); }
static xls_semantic_check* retainedCheck(xide_language_check* q,xide_model_error* e)
{
 xls_analysis_document documents[64];
 for(size_t i=0;i<q->snapshotCount;++i) documents[i]=(xls_analysis_document){q->snapshotKeys[i],q->snapshots[i].path,
  q->snapshotTexts[i],q->snapshotIdentities[i],(uint64_t)((int64_t)q->snapshots[i].version-INT_MIN)};
 HANDLE event=CreateEventW(NULL,FALSE,FALSE,NULL);
 xls_query* query=event ? xlsWorkerCheck(q->semanticActor,documents,q->snapshotCount,q->semanticTarget) : NULL;
 xls_semantic_check* result=NULL;
 if(!query || !xlsQuerySetWake(query,nativeQueryWake,event)) { fail(e,"Cannot submit retained semantic analysis."); goto done; }
 for(;;) {
  if(cancelled(q)) break;
  enum xls_query_state state=xlsQueryCheckResult(query,&result,&q->semanticStats,&q->semanticThread,e->message,sizeof(e->message));
  if(state==XLS_QUERY_READY) break;
  if(state!=XLS_QUERY_PENDING) { e->code=XIDE_MODEL_INVALID; break; }
  HANDLE waits[2]={q->stopEvent,event};
  DWORD wait=WaitForMultipleObjects(2,waits,FALSE,INFINITE);
  if(wait==WAIT_FAILED) { fail(e,"Cannot wait for retained semantic analysis."); break; }
 }
done:
 if(cancelled(q) || !result) { xlsQueryCancel(query); xlsSemanticCheckDestroy(result); result=NULL; }
 /* Removing a Future watch joins callbacks, not the running analysis. The
    actor owns its captured inputs and cooperates with cancellation itself. */
 xlsQueryDestroy(query); if(event) CloseHandle(event); return result;
}
static xls_semantic_completion* retainedCompletion(xide_language_check* q,xide_model_error* e)
{
 xls_analysis_document documents[64]; const char* target=NULL;
 for(size_t i=0;i<q->snapshotCount;++i) {
  documents[i]=(xls_analysis_document){q->snapshotKeys[i],q->snapshots[i].path,q->snapshotTexts[i],
   q->snapshotIdentities[i],(uint64_t)((int64_t)q->snapshots[i].version-INT_MIN)};
  if(xidePathLexicalEqual(q->queryPath,q->snapshots[i].path)) target=q->snapshotKeys[i];
 }
 HANDLE event=CreateEventW(NULL,FALSE,FALSE,NULL);
 xls_query* query=event && target ? xlsWorkerCompletion(q->semanticActor,documents,q->snapshotCount,
  q->semanticTarget,target,(size_t)q->queryOffset) : NULL;
 xls_semantic_completion* result=NULL;
 if(!query || !xlsQuerySetWake(query,nativeQueryWake,event)) { fail(e,"Cannot submit retained cursor analysis."); goto done; }
 for(;;) {
  if(cancelled(q)) break;
  enum xls_query_state state=xlsQueryCompletionResult(query,&result,&q->completionStats,&q->semanticThread,e->message,sizeof(e->message));
  if(state==XLS_QUERY_READY) break;
  if(state!=XLS_QUERY_PENDING) { e->code=XIDE_MODEL_INVALID; break; }
  HANDLE waits[2]={q->stopEvent,event};
  if(WaitForMultipleObjects(2,waits,FALSE,INFINITE)==WAIT_FAILED) { fail(e,"Cannot wait for retained cursor analysis."); break; }
 }
done:
 if(cancelled(q) || !result) { xlsQueryCancel(query); xlsSemanticCompletionDestroy(result); result=NULL; }
 xlsQueryDestroy(query); if(event) CloseHandle(event); return result;
}
static int nativeAnalysis(xide_language_check* q,char** json,int* code,xls_semantic_check** checked,xide_model_error* e)
{
 if(q->retainedSnapshots) for(size_t i=0;i<q->snapshotCount;++i) {
  if(cancelled(q)) return 0;
  for(size_t j=0;j<i;++j) if(xidePathEqual(q->snapshots[i].path,q->snapshots[j].path)) return fail(e,"Retained source paths resolve to the same physical file.");
 }
 xls_semantic_input inputs[64];
 for(size_t i=0;i<q->snapshotCount;++i) inputs[i]=(xls_semantic_input){q->snapshots[i].path,q->snapshots[i].text,strlen(q->snapshots[i].text)};
 size_t size=0; char* report=NULL;
 if(q->queryPath) {
  xls_semantic_completion* result=q->semanticActor ? retainedCompletion(q,e) : xlsSemanticCompletionCreate(q->path,q->queryPath,(size_t)q->queryOffset,
   inputs,q->snapshotCount,nativeCancelled,q,e->message,sizeof(e->message));
  if(!result) { e->code=XIDE_MODEL_INVALID; return 0; }
  report=xlsSemanticCompletionJson(result,&size);
  *code=xlsSemanticCompletionInfo(result).available ? 0 : 1;
  xlsSemanticCompletionDestroy(result);
 } else {
  xls_semantic_check* result=q->semanticActor ? retainedCheck(q,e) :
   xlsSemanticCheckCreate(q->path,inputs,q->snapshotCount,1,nativeCancelled,q,e->message,sizeof(e->message));
  if(!result) { e->code=XIDE_MODEL_INVALID; return 0; }
  *checked=result;
  report=xlsSemanticCheckReportJson(result,&size);
  *code=xlsSemanticCheckInfo(result).accepted ? 0 : 1;
 }
 /* Keep the legacy adapter's malloc ownership at this one compatibility
    boundary; the analysis API itself has typed facts and explicit ownership. */
 if(report && size<=(q->queryPath ? 12u*1024u*1024u : XLS_CHECK_REPORT_BYTES) && !cancelled(q)) {
  *json=malloc(size+1); if(*json) memcpy(*json,report,size+1);
 }
 xlsSemanticStringFree(report);
 return *json!=NULL;
}
#endif
static int integer(const xvalue* v,int64* n,int64 min,int64 max) { return xrtValueGetInt(v,n) && *n>=min && *n<=max; }
static int hash(const void* bytes,size_t size,const xvalue* value)
{
 const char* expected=string(value); unsigned char digest[32]; char hex[65];
 if(!expected || strlen(expected)!=64 || !xrtSha256(bytes,size,digest)) return 0;
 for(int i=0;i<32;++i) snprintf(hex+2*i,3,"%02x",digest[i]);
 return !strcmp(expected,hex);
}
static int absolute(const char* path)
{
 if(!path) return 0;
 int drive=(path[0]>='A' && path[0]<='Z') || (path[0]>='a' && path[0]<='z');
 return (drive && path[1]==':' && (path[2]=='/' || path[2]=='\\')) || (path[0]=='/' && path[1]=='/') || (path[0]=='\\' && path[1]=='\\');
}
static int formatted(const xvalue* value,const char* expected) { const char* text=string(value); return text && !strcmp(text,expected); }
static int infoEqual(const xfileinfo* a,const xfileinfo* b)
{
 uint32 required=XFILE_INFO_SIZE|XFILE_INFO_MODIFY_TIME|XFILE_INFO_IDENTITY;
 return a->Type==XFILE_TYPE_FILE && b->Type==XFILE_TYPE_FILE && (a->Available&required)==required && (b->Available&required)==required && a->Available==b->Available && a->Size==b->Size && a->Device==b->Device && a->Identity==b->Identity && a->Modified==b->Modified && a->Attributes==b->Attributes && (!(a->Available&XFILE_INFO_CHANGE_TIME) || a->Changed==b->Changed);
}
static int metadata(const xvalue* item,const xfileinfo* info)
{
 const xvalue* m=get(item,"metadata"); int64 available=0,attributes=0; char text[32];
 if(!integer(get(m,"available"),&available,0,UINT_MAX) || !integer(get(m,"attributes"),&attributes,0,UINT_MAX) || available!=info->Available || attributes!=info->Attributes) return 0;
 snprintf(text,sizeof(text),"%016llx",(unsigned long long)info->Device); if(!formatted(get(m,"device"),text)) return 0;
 snprintf(text,sizeof(text),"%016llx",(unsigned long long)info->Identity); if(!formatted(get(m,"identity"),text)) return 0;
 snprintf(text,sizeof(text),"%lld",(long long)info->Modified); if(!formatted(get(m,"modifiedUs"),text)) return 0;
 snprintf(text,sizeof(text),"%lld",(long long)info->Changed); return formatted(get(m,"changedUs"),text);
}
/* Compiler positions count Unicode scalars; CRLF/CR are one line break. */
static char* normalized(const char* text,size_t size)
{
 char* out=malloc(size+1); if(!out) return NULL; size_t at=0;
 for(size_t i=0;i<size;++i) { if(text[i]=='\r') { if(i+1<size && text[i+1]=='\n') ++i; out[at++]='\n'; } else out[at++]=text[i]; }
 out[at]=0; return out;
}
static char* diskText(xide_language_check* q,const char* path,const xvalue* item,char** original,xide_model_error* e)
{
 int64 rawSize=0,textSize=0; xfileinfo first,last,named; unsigned char probe; size_t done=0,probeSize=0,bom=0; char *raw=NULL,*text=NULL;
 if(!integer(get(item,"rawSize"),&rawSize,0,FILE_LIMIT) || !integer(get(item,"textSize"),&textSize,0,FILE_LIMIT)) { fail(e,"源码报告长度无效。"); return NULL; }
 xfile file=xideIoOpen(path,XFILE_READ);
 int okay=file && xideIoReceiptFileStat(file,&first) && infoEqual(&first,&first) && first.Size==(uint64)rawSize && metadata(item,&first);
 if(okay) { raw=malloc((size_t)rawSize+1); okay=raw!=NULL; }
 if(okay) okay=!cancelled(q) && xrtReadFull(file,raw,(size_t)rawSize,&done) && done==(size_t)rawSize && xrtRead(file,&probe,1,&probeSize) && !probeSize && xideIoReceiptFileStat(file,&last) && infoEqual(&first,&last) && xideIoReceiptStat(path,&named) && infoEqual(&first,&named) && hash(raw,done,get(item,"rawSha256"));
 if(file && !xrtClose(file)) okay=0;
 if(okay) {
  raw[done]=0; if(done>=3 && !memcmp(raw,"\xef\xbb\xbf",3)) bom=3;
  okay=done-bom==(size_t)textSize && !memchr(raw+bom,0,done-bom) && (!(done-bom) || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,raw+bom,(int)(done-bom),NULL,0)) && hash(raw+bom,done-bom,get(item,"textSha256"));
 }
 if(okay) { text=normalized(raw+bom,done-bom); if(text && original) { memmove(raw,raw+bom,done-bom+1); *original=raw; raw=NULL; } }
 free(raw); if(!text) fail(e,"磁盘输入与编译器使用的源码摘要、身份或编码不一致。"); return text;
}
int xideLanguageCheckPackagePath(char** out,xide_model_error* e)
{
 SetLastError(ERROR_SUCCESS); DWORD n=GetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",NULL,0),code=GetLastError(); *out=NULL;
 if(!n) { if(code==ERROR_ENVVAR_NOT_FOUND) return 1; if(code==ERROR_SUCCESS) { *out=calloc(1,1); return *out!=NULL; } return fail(e,"无法读取包搜索环境。"); }
 if(n>32768) return fail(e,"包搜索环境超过限制。");
 wchar_t* w=malloc((size_t)n*sizeof(*w)); if(!w) return 0;
 DWORD got=GetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",w,n); int bytes=got && got<n ? WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,NULL,0,NULL,NULL) : 0;
 char* text=bytes ? malloc((size_t)bytes) : NULL;
 int okay=text && WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,text,bytes,NULL,NULL); free(w);
 if(!okay) { free(text); return fail(e,"包搜索环境无法稳定转换为 UTF-8。"); } *out=text; return 1;
}
static int resolution(xide_language_check* q,const xvalue* value,xide_model_error* e)
{
 int64 schema=0; bool observed=false; const xvalue* probes=get(value,"probes"),*env=get(value,"XLANG_PACKAGE_PATH");
 if(!integer(get(value,"schema"),&schema,1,1) || !formatted(get(value,"scope"),"xlang-import-resolution") || !probes || xrtValueType(probes)!=XVALUE_ARRAY || xrtValueCount(probes)>2048 || !xrtValueGetBool(get(value,"packagePathObserved"),&observed) || !env) return fail(e,"编译器未返回可验证的导入候选报告。");
 const char* package=string(env); if(!package && xrtValueType(env)!=XVALUE_NULL) return fail(e,"包路径报告无效。");
 if(!observed && package) return fail(e,"未观察的包路径报告无效。");
 if(observed && (package && q->packagePath ? strcmp(package,q->packagePath)!=0 : package!=q->packagePath)) return fail(e,"编译器包路径与本次不可变环境不一致。");
 char* now=NULL; if(!xideLanguageCheckPackagePath(&now,e)) return 0;
 int same=now && q->packagePath ? !strcmp(now,q->packagePath) : now==q->packagePath; free(now); if(!same) return fail(e,"检查期间包搜索环境已变化。");
 size_t bytes=0;
 for(size_t i=0;i<xrtValueCount(probes);++i) {
  if(cancelled(q)) return 0;
  const xvalue* probe=xrtValueArrayGet(probes,i); const char* path=string(get(probe,"path")); bool exists=false; char* resolved=path ? xidePathResolveLexical(NULL,path) : NULL;
  int valid=resolved && absolute(path) && xidePathLexicalEqual(path,resolved) && strlen(path)<=32767 && xrtValueGetBool(get(probe,"exists"),&exists); free(resolved);
  if(!valid || strlen(path)+1>1024u*1024u-bytes || (xrtFileExists((char*)path)!=0)!=exists) return fail(e,"导入候选的文件状态已变化或报告无效。"); 
  bytes+=strlen(path)+1;
  for(size_t j=0;j<i;++j) if(xidePathLexicalEqual(path,string(get(xrtValueArrayGet(probes,j),"path")))) return fail(e,"导入候选报告含重复路径。");
 }
 return 1;
}
static void clearGraph(source_graph* g) { for(size_t i=0;i<g->count;++i) { free(g->items[i].path); free(g->items[i].text); free(g->items[i].original); } free(g->items); memset(g,0,sizeof(*g)); }
static int overlayIndex(xide_language_check* q,const char* path)
{ for(size_t i=0;i<q->snapshotCount;++i) if(xidePathLexicalEqual(path,q->snapshots[i].path)) return (int)i; return -1; }
static int loadGraph(xide_language_check* q,const xvalue* raw,source_graph* g,xide_model_error* e)
{
 const xvalue* report=get(raw,"sourceReport"),*files=get(report,"files"); int64 schema=0; bool complete=false;
 if(!integer(get(report,"schema"),&schema,1,1) || !formatted(get(report,"scope"),"loaded-sources-and-manifests") || !xrtValueGetBool(get(report,"complete"),&complete) || !complete || !formatted(get(report,"error"),"") || !formatted(get(report,"errorFile"),"") || !files || xrtValueType(files)!=XVALUE_ARRAY || !xrtValueCount(files) || xrtValueCount(files)>512) return fail(e,"源码快照报告缺失、不完整或无法验证。");
 g->items=calloc(xrtValueCount(files),sizeof(*g->items)); if(!g->items) return 0;
 int entry=0;
 for(size_t i=0;i<xrtValueCount(files);++i) {
  const xvalue* item=xrtValueArrayGet(files,i); const char *path=string(get(item,"path")),*origin=string(get(item,"origin")); int64 size=0,rawSize=0,kind=0;
  if(cancelled(q) || !path || !origin || !integer(get(item,"kind"),&kind,1,3) || !integer(get(item,"textSize"),&size,0,FILE_LIMIT) || !integer(get(item,"rawSize"),&rawSize,0,FILE_LIMIT) || (size_t)size>GRAPH_LIMIT-g->bytes) return fail(e,"源码报告字段或总容量无效。");
  char* resolved=xidePathResolveLexical(NULL,path); if(!resolved || !xidePathLexicalEqual(resolved,path) || !absolute(path)) { free(resolved); return fail(e,"源码报告必须使用绝对路径。"); }
  for(size_t j=0;j<g->count;++j) if(xidePathLexicalEqual(resolved,g->items[j].path)) { free(resolved); return fail(e,"源码报告含重复路径。"); }
  int overlay=overlayIndex(q,resolved); char *text=NULL,*original=NULL;
  if(!strcmp(origin,"overlay")) {
   const char* submitted=overlay>=0 ? q->snapshots[overlay].text : ""; size_t submittedSize=strlen(submitted),bom=submittedSize>=3 && !memcmp(submitted,"\xef\xbb\xbf",3) ? 3 : 0;
   /* CLI reads the UTF-8 snapshot file after removing its BOM. The report
      hashes that actual compiler input; LSP positions retain the open BOM. */
   if(overlay>=0 && (kind&1) && size==(int64)(submittedSize-bom) && rawSize==size && hash(submitted+bom,(size_t)size,get(item,"textSha256")) && hash(submitted+bom,(size_t)size,get(item,"rawSha256"))) { text=normalized(submitted,submittedSize); if(text) { original=malloc((size_t)size+1); if(original) memcpy(original,submitted+bom,(size_t)size+1); } }
  } else if(!strcmp(origin,"disk") && overlay<0) text=diskText(q,resolved,item,&original,e);
  if(!text || !original) { free(text); free(original); free(resolved); if(!e->code) fail(e,"源码报告与提交的文档覆盖不一致。"); return 0; }
  g->items[g->count++]=(source_item){resolved,text,original,item,overlay}; g->bytes+=(size_t)size;
  if(xidePathLexicalEqual(resolved,q->path) && (overlay>=0 || q->nativeBridge) && kind==1) entry=1;
 }
 g->resolution=get(raw,"resolutionReport"); return entry && resolution(q,g->resolution,e) ? 1 : fail(e,*e->message ? "入口或导入候选报告无法验证。" : "源码报告缺少覆盖后的入口。");
}
static int validateGraph(xide_language_check* q,source_graph* g,xide_model_error* e)
{
 for(size_t i=0;i<g->count;++i) {
  source_item* item=&g->items[i]; if(item->overlay>=0) continue;
  char* text=diskText(q,item->path,item->report,NULL,e); if(!text) return 0;
  int same=!strcmp(text,item->text); free(text); if(!same) return fail(e,"检查期间磁盘源码已变化。");
 }
 return resolution(q,g->resolution,e);
}
/* Reports were fully validated on capture. Retain owned reports, not graph
   text or borrowed JSON. Each disk input is read/hash-checked again, including
   manifests and same-byte identity replacement; overlays remain caller-owned. */
int xideLanguageSnapshotGuard(xide_language_check* q,xide_model_error* e)
{
 for(size_t i=0;i<xrtValueCount(q->guardFiles);++i) {
  if(cancelled(q)) return 0;
  const xvalue* item=xrtValueArrayGet(q->guardFiles,i);
  if(formatted(get(item,"origin"),"overlay")) continue;
  char* text=diskText(q,string(get(item,"path")),item,NULL,e); if(!text) return 0; free(text);
 }
 char* now=NULL; if(!xideLanguageCheckPackagePath(&now,e)) return 0;
 int same=now && q->packagePath ? !strcmp(now,q->packagePath) : now==q->packagePath; free(now);
 if(!same) return fail(e,"发布后包搜索环境已变化。");
 const xvalue* probes=get(q->guardResolution,"probes");
 for(size_t i=0;i<xrtValueCount(probes);++i) {
  if(cancelled(q)) return 0;
  const xvalue* p=xrtValueArrayGet(probes,i); bool exists=false; xrtValueGetBool(get(p,"exists"),&exists);
  if((xrtFileExists((char*)string(get(p,"path")))!=0)!=exists) return fail(e,"发布后导入候选的普通文件状态已变化。");
 }
 return !cancelled(q) && (q->nativeSemantic || xideToolchainValidate(q->compiler,&q->guardToolchain,e));
}
static xvalue* position(int line,int character)
{ xvalue* out=xrtValueObject(); if(!out || !put(out,"line",xrtValueInt(line)) || !put(out,"character",xrtValueInt(character))) { xrtValueRelease(out); return NULL; } return out; }
static xvalue* diagnostic(const char* text,const char* file,const xvalue* raw,int bomPrefix)
{
 const char* message=string(get(raw,"message")); int64 line=0,column=0;
 if(!message || !*message || strlen(message)>65535 || !integer(get(raw,"line"),&line,1,INT_MAX) || !integer(get(raw,"column"),&column,1,INT_MAX)) return NULL;
 if(line==1 && bomPrefix) ++column;
 int offset=xidePositionToOffset(text,(int)line,(int)column),actualLine=0,actualColumn=0; xidePositionFromOffset(text,offset,&actualLine,&actualColumn); if(actualLine!=line || actualColumn!=column) return NULL;
 int end=offset; if(text[end] && text[end]!='\n') { ++end; while(((unsigned char)text[end]&0xc0)==0x80) ++end; }
 int sl=0,sc=0,el=0,ec=0; if(!xideLspOffsetToPosition(text,offset,&sl,&sc) || !xideLspOffsetToPosition(text,end,&el,&ec)) return NULL;
 xvalue *range=xrtValueObject(),*out=xrtValueObject(); int okay=range && out && put(range,"start",position(sl,sc)) && put(range,"end",position(el,ec));
 if(okay) { okay=put(out,"range",range); range=NULL; }
 if(okay) okay=put(out,"file",xrtValueString(sv(file))) && put(out,"severity",xrtValueInt(1)) && put(out,"message",xrtValueString(sv(message))) && put(out,"source",xrtValueString(sv("xlang document snapshots")));
 xrtValueRelease(range); if(!okay) { xrtValueRelease(out); out=NULL; } return out;
}
static xvalue* group(const char* path,const char* text,int version,const xvalue* disk)
{
 unsigned char digest[32]; char hex[65]; size_t length=strlen(text);
 if(!xrtSha256(text,length,digest)) return NULL;
 for(int i=0;i<32;++i) snprintf(hex+2*i,3,"%02x",digest[i]);
 xvalue* out=xrtValueObject(); int okay=out && put(out,"path",xrtValueString(sv(path))) && put(out,"diagnostics",xrtValueArray()) && put(out,"origin",xrtValueString(sv(disk ? "disk" : "overlay"))) && put(out,"normalizedSize",xrtValueInt((int64)length)) && put(out,"normalizedSha256",xrtValueString(sv(hex))) && (disk ? put(out,"input",xrtValueDeepClone(disk)) : put(out,"version",xrtValueInt(version)));
 if(!okay) { xrtValueRelease(out); return NULL; } return out;
}
static int buildDiagnostics(xide_language_check* q,const xvalue* raw,source_graph* g,xvalue* result,xide_model_error* e)
{
 const xvalue* list=get(raw,"diagnostics"); if(!list || xrtValueType(list)!=XVALUE_ARRAY || xrtValueCount(list)>4096) return fail(e,"诊断数组无效或超过限制。");
 xvalue *groups=xrtValueArray(),*flat=xrtValueArray(); int okay=groups && flat;
 for(size_t i=0;okay && i<q->snapshotCount;++i) { char* text=normalized(q->snapshots[i].text,strlen(q->snapshots[i].text)); xvalue* item=text ? group(q->snapshots[i].path,text,q->snapshots[i].version,NULL) : NULL; free(text); okay=item && xrtValueArrayAppend(groups,item); xrtValueRelease(item); }
 for(size_t i=0;okay && i<g->count;++i) if(g->items[i].overlay<0) { xvalue* item=group(g->items[i].path,g->items[i].text,0,g->items[i].report); okay=item && xrtValueArrayAppend(groups,item); xrtValueRelease(item); }
 for(size_t i=0;okay && i<xrtValueCount(list);++i) {
  const xvalue* d=xrtValueArrayGet(list,i); const char* file=string(get(d,"file")); char* resolved=file ? xidePathResolveLexical(q->directory,file) : NULL; source_item* source=NULL; xvalue* target=NULL;
  for(size_t j=0;resolved && j<g->count;++j) if(xidePathLexicalEqual(resolved,g->items[j].path)) { source=&g->items[j]; break; }
  for(size_t j=0;source && j<xrtValueCount(groups);++j) { xvalue* item=xrtValueArrayGet(groups,j); if(xidePathLexicalEqual(source->path,string(get(item,"path")))) { target=get(item,"diagnostics"); break; } }
  xvalue* item=source && target ? diagnostic(source->text,source->path,d,source->overlay>=0 && strlen(source->text)>=3 && !memcmp(source->text,"\xef\xbb\xbf",3)) : NULL; free(resolved);
  okay=!cancelled(q) && item && xrtValueArrayAppend(target,item) && xrtValueArrayAppend(flat,item); xrtValueRelease(item);
 }
 if(okay) okay=xrtValueObjectSet(result,sv("documents"),groups) && xrtValueObjectSet(result,sv("diagnostics"),flat);
 xrtValueRelease(groups); xrtValueRelease(flat); return okay ? 1 : fail(e,"分文件诊断位置、来源或正文无法验证。");
}
static wchar_t* wide(const char* text)
{ int n=text ? MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,NULL,0) : 0; wchar_t* out=n ? malloc((size_t)n*sizeof(*out)) : NULL; if(out && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,out,n)) { free(out); out=NULL; } return out; }
/* Non-inheritable handles keep snapshots immutable, and the kernel removes
   only files it owns even if the service terminates during a check. */
static HANDLE writeSnapshot(xide_language_check* q,const char* path,const char* text,xide_model_error* e)
{
 wchar_t* w=wide(path); HANDLE h=w ? CreateFileW(w,GENERIC_WRITE|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL) : INVALID_HANDLE_VALUE; free(w);
 if(h==INVALID_HANDLE_VALUE) { fail(e,"无法创建检查专用快照。"); return INVALID_HANDLE_VALUE; }
 size_t size=strlen(text),at=0;
 while(at<size && !cancelled(q)) { DWORD wrote=0,n=(DWORD)(size-at>65536u ? 65536u : size-at); if(!WriteFile(h,text+at,n,&wrote,NULL) || !wrote) break; at+=wrote; }
 if(at!=size || cancelled(q) || !FlushFileBuffers(h)) { CloseHandle(h); fail(e,"快照写入失败或已取消。"); return INVALID_HANDLE_VALUE; }
 return h;
}
static char* temporary(xide_model_error* e)
{
 wchar_t temp[32768]; DWORD n=GetTempPathW(32768,temp); if(!n || n>=32768) { fail(e,"无法取得快照临时目录。"); return NULL; }
 int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,temp,-1,NULL,0,NULL,NULL); char* base=size ? malloc((size_t)size) : NULL;
 if(base) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,temp,-1,base,size,NULL,NULL);
 char *id=xideCreateId("xide-check"),*path=base && id ? xidePathResolveLexical(base,id) : NULL; free(base); free(id);
 if(!path) fail(e,"无法创建检查专用的 GUID 文件命名空间。");
 return path;
}
int xideLanguageSnapshotRun(xide_language_check* q,xvalue** output,xide_model_error* e)
{
 xide_toolchain_stamp stamp={0}; xide_toolchain_profile profile={0}; source_graph graph={0}; char *help=NULL,*json=NULL,*temp=NULL,*files[64]={0}; int indexed=0; xvalue *raw=NULL,*result=NULL;
#ifdef XLS_COMPILER_XRT
 xls_semantic_check* nativeCheck=NULL;
#endif
 HANDLE leases[64]; for(size_t i=0;i<64;++i) leases[i]=INVALID_HANDLE_VALUE;
 char cursor[32]; snprintf(cursor,sizeof(cursor),"%d",q->queryOffset);
 const char* args[2560]={"check",q->path,"--json","--source-report"}; size_t count=4; int code=0,success=0; bool okay=false; uint64_t began=GetTickCount64();
 if(cancelled(q) || !xideLanguageCheckPackagePath(&q->packagePath,e)) goto done;
 q->packagePathCaptured=1;
#ifdef XLS_COMPILER_XRT
 if(q->nativeSemantic) {
  if(q->nativeBridge || !nativeAnalysis(q,&json,&code,&nativeCheck,e)) goto done;
  indexed=q->queryPath==NULL;
  goto analysed;
 }
#endif
 if(!xideToolchainCapture(q->compiler,&stamp,e)) goto done;
 const char* helpArgs[]={"--help"}; if(!xideLanguageCheckRunCompiler(q,helpArgs,1,65535,&help,&code,e) || code!=0 || !xideToolchainObserve(q->cache,q->compiler,q->directory,&stamp,help,strlen(help),&profile,e)) goto done;
 uint32_t required=XIDE_TOOL_CHECK_JSON|XIDE_TOOL_CHECK_OVERLAY|XIDE_TOOL_CHECK_SOURCE_REPORT;
 if(!q->nativeBridge && (!(profile.commands&XIDE_TOOL_CHECK) || (profile.options&required)!=required)) { fail(e,"编译器未声明 check 的 JSON、源码覆盖和来源报告；全文快照检查不可用。"); goto done; }
 if(q->nativeBridge) {
  uint32_t bridgeRequired=XIDE_TOOL_CBRIDGE_JSON|XIDE_TOOL_CBRIDGE_ENVIRONMENT|XIDE_TOOL_CHECK_OVERLAY;
  if(!(profile.commands&XIDE_TOOL_CBRIDGE) || (profile.options&bridgeRequired)!=bridgeRequired || q->bridgeArgCount>2305) { fail(e,"Selected compiler does not declare native C bridge/environment support."); e->code=XIDE_MODEL_LEGACY; goto done; }
  args[0]="c-bridge"; args[3]="--c-environment";
  for(size_t i=0;i<q->bridgeArgCount;++i) args[count++]=q->bridgeArgs[i];
 } else if(q->queryPath) {
  if(!(profile.options&XIDE_TOOL_CHECK_COMPLETION)) { fail(e,"编译器未声明 --completion-at；语义补全不可用。"); goto done; }
  args[count++]="--completion-at"; args[count++]=q->queryPath; args[count++]=cursor;
 } else indexed=(profile.options&XIDE_TOOL_CHECK_SEMANTIC_INDEX)!=0;
 if(indexed) args[3]="--semantic-index";
 temp=temporary(e); if(!temp) goto done;
 for(size_t i=0;i<q->snapshotCount;++i) {
  size_t capacity=strlen(temp)+32; files[i]=malloc(capacity); if(files[i]) snprintf(files[i],capacity,"%s-%zu.utf8",temp,i);
  const char* text=q->snapshots[i].text;
  if(cancelled(q) || !files[i] || (leases[i]=writeSnapshot(q,files[i],text,e))==INVALID_HANDLE_VALUE) { if(!e->code) fail(e,"无法写入检查专用的临时快照。"); goto done; }
  args[count++]="--source-overlay"; args[count++]=q->snapshots[i].path; args[count++]=files[i];
 }
 if(!xideLanguageCheckRunCompiler(q,args,count,q->nativeBridge ? 80u*1024u*1024u : indexed || q->queryPath ? 12u*1024u*1024u : OUTPUT_LIMIT,&json,&code,e)) goto done;
#ifdef XLS_COMPILER_XRT
analysed:
#endif
 if(q->nativeBridge) {
  xjsonreadconfig config; xrtJsonReadConfigInit(&config); config.MaxInputBytes=80u*1024u*1024u; config.MaxStringBytes=24u*1024u*1024u; config.MaxValues=2097152; config.MaxContainerItems=32768; config.MaxDepth=32; raw=xrtJsonRead(sv(json),&config);
 }
#ifdef XLS_COMPILER_XRT
 else if(q->nativeSemantic && !q->queryPath) {
  xjsonreadconfig config; xrtJsonReadConfigInit(&config); config.MaxInputBytes=XLS_CHECK_REPORT_BYTES;
  config.MaxStringBytes=1024u*1024u; config.MaxValues=4u*1024u*1024u; config.MaxContainerItems=65536; config.MaxDepth=64;
  raw=xrtJsonRead(sv(json),&config);
 }
#endif
 else raw=xrtJsonParse(sv(json));
 const char* phase=string(get(raw,"phase"));
 if(!raw || xrtValueType(raw)!=XVALUE_OBJECT || !xrtValueGetBool(get(raw,"ok"),&okay) || okay!=(code==0) || !phase || !*phase || strlen(phase)>64) { fail(e,"快照检查响应或退出状态无效。"); goto done; }
 if(q->nativeBridge && !okay) {
  const char* message=string(get(xrtValueArrayGet(get(raw,"diagnostics"),0),"message"));
  fail(e,message && *message ? message : "Native C declarations/environment could not be generated."); goto done;
 }
 if(!loadGraph(q,raw,&graph,e)) goto done;
 if(q->nativeBridge) {
  const xvalue* bridge=get(raw,"cBridge"); bool complete=false;
  if(!bridge || !xrtValueGetBool(get(bridge,"complete"),&complete) || !complete) { fail(e,"Compiler did not return a complete native declaration bridge."); goto done; }
  result=xrtValueObject(); if(!result || !xrtValueObjectSet(result,sv("cBridge"),bridge) || !xrtValueObjectSet(result,sv("sourceFiles"),get(get(raw,"sourceReport"),"files"))) goto done;
  xide_semantic_source sources[512];int versions[512];
  for(size_t i=0;i<graph.count;++i) {
   int64 kind=0;int overlay=graph.items[i].overlay;xrtValueGetInt(get(graph.items[i].report,"kind"),&kind);
   versions[i]=overlay>=0 ? q->snapshots[overlay].version : 0;
   sources[i]=(xide_semantic_source){graph.items[i].path,graph.items[i].original,(int)kind,overlay>=0 && strlen(q->snapshots[overlay].text)>=3 && !memcmp(q->snapshots[overlay].text,"\xef\xbb\xbf",3) ? 1 : 0};
  }
  xide_model_error mapError={0};xvalue* bindings=xideCBridgeMapBindings(get(bridge,"declarationMap"),string(get(bridge,"declarations")),sources,get(get(raw,"sourceReport"),"files"),versions,graph.count,&mapError);
  /* Optional navigation failure keeps verified ordinary ABI analysis usable. */
  int mapped=bindings!=NULL;
  if(!bindings) bindings=xrtValueNull();
  if(!put(result,"sourceBindings",bindings) || !put(result,"sourceBindingsValid",xrtValueBool(mapped))) goto done;
 } else if(q->queryPath) {
  const xvalue* diagnostics=get(raw,"diagnostics");
  if(strcmp(phase,"completion") || !diagnostics || xrtValueType(diagnostics)!=XVALUE_ARRAY || xrtValueCount(diagnostics)) { fail(e,"补全查询不得被普通检查或诊断替代。"); goto done; }
  xide_semantic_source sources[512];
  for(size_t i=0;i<graph.count;++i) { int64 kind=0; xrtValueGetInt(get(graph.items[i].report,"kind"),&kind); sources[i]=(xide_semantic_source){graph.items[i].path,graph.items[i].original,(int)kind,graph.items[i].overlay>=0 && strlen(q->snapshots[graph.items[i].overlay].text)>=3 && !memcmp(q->snapshots[graph.items[i].overlay].text,"\xef\xbb\xbf",3) ? 1 : 0}; }
  result=xideSemanticCompletion(get(raw,"completion"),sources,graph.count,q->queryPath,q->queryOffset,q->queryVersion,okay,e); if(!result) goto done;
 } else {
  result=xrtValueObject(); if(!result || !buildDiagnostics(q,raw,&graph,result,e) || (!okay && !xrtValueCount(get(result,"diagnostics")))) { if(!e->code) fail(e,"检查失败但没有可验证的分文件诊断。"); goto done; }
 }
 if(indexed) {
  const xvalue* index=get(raw,"semanticIndex");
#ifdef XLS_COMPILER_XRT
  if(nativeCheck) {
   xls_check_info info=xlsSemanticCheckInfo(nativeCheck);
   if(info.accepted!=okay || info.hasIndex!=okay) { fail(e,"原生语义索引与同次检查结果不一致。"); goto done; }
  } else
#endif
  {
   bool complete=false; const xvalue* nodes=get(index,"nodes");
   if(!index || !xrtValueGetBool(get(index,"complete"),&complete) || complete!=okay || !nodes || xrtValueType(nodes)!=XVALUE_ARRAY || (!okay && xrtValueCount(nodes))) { fail(e,"语义索引状态与同次检查结果不一致。"); goto done; }
  }
  if(okay) {
   xide_semantic_source sources[512];
   for(size_t i=0;i<graph.count;++i) { int64 kind=0; xrtValueGetInt(get(graph.items[i].report,"kind"),&kind); sources[i]=(xide_semantic_source){graph.items[i].path,graph.items[i].original,(int)kind,graph.items[i].overlay>=0 && strlen(q->snapshots[graph.items[i].overlay].text)>=3 && !memcmp(q->snapshots[graph.items[i].overlay].text,"\xef\xbb\xbf",3) ? 1 : 0}; }
#ifdef XLS_COMPILER_XRT
   if(nativeCheck) q->definitions=xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(nativeCheck),sources,graph.count,e);
   else
#endif
   q->definitions=xideSemanticDefinitionsCreate(index,sources,graph.count,e);
   if(!q->definitions) goto done;
  }
 }
 if(!q->queryPath && !q->nativeBridge && !put(result,"semanticDefinitions",xrtValueBool(q->definitions!=NULL))) goto done;
#ifdef XLS_COMPILER_XRT
 if(q->semanticActor && q->queryPath) {
  xvalue* stats=xrtValueObject();
  int okayStats=stats && put(stats,"builds",xrtValueInt((int64)q->completionStats.builds)) &&
   put(stats,"cacheHits",xrtValueInt((int64)q->completionStats.hits)) && put(stats,"invalidations",xrtValueInt((int64)q->completionStats.invalidations)) &&
   put(stats,"documents",xrtValueInt((int64)q->completionStats.documents)) && put(stats,"textBytes",xrtValueInt((int64)q->completionStats.textBytes)) &&
   put(stats,"workerThreadId",xrtValueInt((int64)q->semanticThread)) && put(stats,"captureThreadId",xrtValueInt((int64)q->captureThread)) &&
   put(stats,"adapterThreadId",xrtValueInt((int64)xrtThreadCurrentId())) &&
   put(stats,"cursorFullParses",xrtValueInt((int64)q->completionStats.cursorFullParses)) &&
   put(stats,"cursorFragmentParses",xrtValueInt((int64)q->completionStats.cursorFragmentParses)) &&
   put(stats,"cursorReusedDeclarations",xrtValueInt((int64)q->completionStats.cursorReusedDeclarations)) &&
   put(stats,"cursorParsedBytes",xrtValueInt((int64)q->completionStats.cursorParsedBytes)) &&
   put(stats,"cursorSyntaxHits",xrtValueInt((int64)q->completionStats.cursorSyntaxHits)) &&
   put(stats,"cursorFreshBuilds",xrtValueInt((int64)q->completionStats.cursorFreshBuilds));
  if(!okayStats) { xrtValueRelease(stats); goto done; }
  if(!put(result,"analysis",stats)) goto done;
 }
 if(q->semanticActor && !q->queryPath) {
  xvalue* stats=xrtValueObject();
  int okayStats=stats && put(stats,"builds",xrtValueInt((int64)q->semanticStats.builds)) && put(stats,"cacheHits",xrtValueInt((int64)q->semanticStats.hits)) &&
   put(stats,"invalidations",xrtValueInt((int64)q->semanticStats.invalidations)) && put(stats,"documents",xrtValueInt((int64)q->semanticStats.documents)) &&
   put(stats,"textBytes",xrtValueInt((int64)q->semanticStats.textBytes)) && put(stats,"workerThreadId",xrtValueInt((int64)q->semanticThread)) &&
   put(stats,"captureThreadId",xrtValueInt((int64)q->captureThread)) && put(stats,"adapterThreadId",xrtValueInt((int64)xrtThreadCurrentId())) &&
   put(stats,"entryFullParses",xrtValueInt((int64)q->semanticStats.entryFullParses)) && put(stats,"entryFragmentParses",xrtValueInt((int64)q->semanticStats.entryFragmentParses)) &&
   put(stats,"entryReusedDeclarations",xrtValueInt((int64)q->semanticStats.entryReusedDeclarations)) && put(stats,"entryParsedBytes",xrtValueInt((int64)q->semanticStats.entryParsedBytes)) &&
   put(stats,"entrySyntaxHits",xrtValueInt((int64)q->semanticStats.entrySyntaxHits)) &&
   put(stats,"semanticFunctionsChecked",xrtValueInt((int64)q->semanticStats.semanticFunctionsChecked)) &&
   put(stats,"semanticFunctionsReused",xrtValueInt((int64)q->semanticStats.semanticFunctionsReused)) &&
   put(stats,"semanticContextHits",xrtValueInt((int64)q->semanticStats.semanticContextHits)) &&
   put(stats,"semanticMemoBytes",xrtValueInt((int64)q->semanticStats.semanticMemoBytes));
  if(!okayStats) { xrtValueRelease(stats); goto done; }
  if(!put(result,"analysis",stats)) goto done;
 }
 if(nativeCheck && !xlsSemanticCheckValidate(nativeCheck)) { fail(e,"原生分析的输入在发布前已改变。"); goto done; }
#endif
 if(!validateGraph(q,&graph,e) || (!q->nativeSemantic && !xideToolchainValidate(q->compiler,&stamp,e)) || cancelled(q)) goto done;
 q->guardFiles=xrtValueDeepClone(get(get(raw,"sourceReport"),"files"));
 q->guardResolution=xrtValueDeepClone(get(raw,"resolutionReport")); q->guardToolchain=stamp;
 if(!q->guardFiles || !q->guardResolution) { fail(e,"无法保留发布后的输入报告。"); goto done; }
 success=put(result,"ok",xrtValueBool(okay)) && put(result,"phase",xrtValueString(sv(phase))) && put(result,"version",xrtValueInt(q->queryPath ? q->queryVersion : q->version)) && put(result,"scope",xrtValueString(sv(q->nativeBridge ? "native-c-bridge" : q->queryPath ? "cursor-query" : "entry-with-open-overlays"))) && put(result,"guardScope",xrtValueString(sv("background-poll"))) && put(result,"durationMs",xrtValueInt((int64)(GetTickCount64()-began)));
 if(success) { *output=result; result=NULL; }
done:
#ifdef XLS_COMPILER_XRT
 xlsSemanticCheckDestroy(nativeCheck);
#endif
 for(size_t i=0;i<q->snapshotCount;++i) { if(leases[i]!=INVALID_HANDLE_VALUE) CloseHandle(leases[i]); free(files[i]); } free(temp);
 clearGraph(&graph); xrtValueRelease(raw); xrtValueRelease(result); free(help); free(json); xideToolchainProfileClear(&profile); return success;
}
