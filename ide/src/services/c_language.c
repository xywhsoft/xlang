#include "services/c_language.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#define C_DOCUMENTS 65u /* 64 user files plus one owned bridge indexing document. */
#define C_REQUESTS 64u
typedef struct c_input { uint64_t id; char *path,*uri,*text; int version; } c_input;
typedef struct c_request { int id,index; enum xide_c_query query; } c_request;
struct xide_c_session {
 xide_language_client* client; xvalue* commands;
 c_input documents[C_DOCUMENTS]; size_t count;
 char* draftDirectory; HANDLE draftFiles[C_DOCUMENTS+1]; size_t draftFileCount;
 c_request requests[C_REQUESTS]; int nextId,supported[4];
 uint64_t started,generation; int completion,tracing,traceSeen; enum xide_c_state state; char error[768];
 void (*wake)(void*); void* wakeUser;
};
static volatile LONG64 cGeneration;
static void cdraftClear(xide_c_session* s);
static xstrview csv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* cget(const xvalue* v,const char* key) { return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,csv(key)) : NULL; }
static int cput(xvalue* v,const char* key,xvalue* child) { return xrtValueObjectSetNew(v,csv(key),child); }
static const char* cstring(const xvalue* v)
{ xstrview s={0}; return v && xrtValueGetString(v,&s) && s.Size<=131072 && !memchr(s.Data,0,s.Size) && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static int cint(const xvalue* v,int* out) { int64 n=0; if(!v || !xrtValueGetInt(v,&n) || n<0 || n>INT_MAX) return 0; *out=(int)n; return 1; }
static char* ccopy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int cerror(xide_model_error* e,const char* text) { if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%.*s",(int)sizeof(e->message)-1,text); } return 0; }
static void cfailed(xide_c_session* s,const char* message)
{ s->state=XIDE_C_FAILED; if(message!=s->error) snprintf(s->error,sizeof(s->error),"%s",message); memset(s->requests,0,sizeof(s->requests)); xideLanguageClientStop(s->client); s->client=NULL; cdraftClear(s); }
static int csend(xide_c_session* s,const char* method,xvalue* params,int id)
{
 xvalue* message=xrtValueObject(); int ok=message && cput(message,"jsonrpc",xrtValueString(csv("2.0"))) && cput(message,"method",xrtValueString(csv(method)));
 if(ok && params) { ok=cput(message,"params",params); params=NULL; } if(ok && id) ok=cput(message,"id",xrtValueInt(id));
 if(ok) ok=xideLanguageClientSend(s->client,message,NULL);
 xrtValueRelease(message); xrtValueRelease(params); return ok;
}
static int cposition(const char* text,const xvalue* position,int* offset)
{ int line=0,character=0; return cint(cget(position,"line"),&line) && cint(cget(position,"character"),&character) && xideLspPositionToOffset(text,line,character,offset); }
static int crange(const char* text,const xvalue* range,int* start,int* end)
{ return cposition(text,cget(range,"start"),start) && cposition(text,cget(range,"end"),end) && *start<=*end; }
static int cabsolute(const char* path)
{
 size_t n=path ? strlen(path) : 0; if(n<3 || n>32767) return 0;
 return (((path[0]>='A' && path[0]<='Z') || (path[0]>='a' && path[0]<='z')) && path[1]==':' && (path[2]=='/' || path[2]=='\\')) || (n>4 && ((path[0]=='/' && path[1]=='/') || (path[0]=='\\' && path[1]=='\\')));
}
static int cdocument(xide_c_session* s,const char* path)
{ for(size_t i=0;i<s->count;++i) if(xidePathLexicalEqual(path,s->documents[i].path)) return (int)i; return -1; }
static int ccommandMap(xide_c_session* s,const xvalue* database,xvalue** out)
{
 if(!database || xrtValueType(database)!=XVALUE_ARRAY || xrtValueCount(database)>(s->completion ? 2049u : 2048u)) return 0;
 size_t bytes=0; char* encoded=xrtJsonStringify(database,false,&bytes); xrtFree(encoded); if(!bytes || bytes>4u*1024u*1024u) return 0;
 xvalue* map=xrtValueObject(); if(!map) return 0; int ok=1;
 for(size_t i=0;ok && i<xrtValueCount(database);++i) {
  const xvalue* item=xrtValueArrayGet(database,i); const char *file=cstring(cget(item,"file")),*directory=cstring(cget(item,"directory")); xvalue* args=cget(item,"arguments"); char *absolute=NULL,*base=NULL;
  ok=cabsolute(file) && cabsolute(directory) && args && xrtValueType(args)==XVALUE_ARRAY && xrtValueCount(args)>0 && xrtValueCount(args)<=1024;
  if(ok) { absolute=xidePathResolveLexical(NULL,file); base=xidePathResolveLexical(NULL,directory); ok=absolute && base && xidePathLexicalEqual(file,absolute) && xidePathLexicalEqual(directory,base) && !cget(map,absolute); }
  for(size_t j=0;ok && j<xrtValueCount(map);++j) { xstrview key={0}; xrtValueObjectAt(map,j,&key); ok=!xidePathLexicalEqual(key.Data,absolute); }
  for(size_t j=0;ok && j<xrtValueCount(args);++j) { const char* arg=cstring(xrtValueArrayGet(args,j)); ok=arg && (j || *arg); }
  xvalue* command=ok ? xrtValueObject() : NULL;
  if(ok) ok=command && cput(command,"workingDirectory",xrtValueString(csv(base))) && cput(command,"compilationCommand",xrtValueDeepClone(args)) && xrtValueObjectSet(map,csv(absolute),command);
  free(absolute); free(base); xrtValueRelease(command);
 }
 for(size_t i=0;ok && i<s->count;++i) {
  int found=0; for(size_t j=0;j<xrtValueCount(map);++j) { xstrview key={0}; xrtValueObjectAt(map,j,&key); if(xidePathLexicalEqual(key.Data,s->documents[i].path)) found=1; } ok=found;
 }
 if(!ok) { xrtValueRelease(map); return 0; } *out=map; return 1;
}
static int cenvelope(const xvalue* message)
{
 const char* rpc=cstring(cget(message,"jsonrpc")); const xvalue *method=cget(message,"method"),*id=cget(message,"id"),*result=cget(message,"result"),*error=cget(message,"error"); int64 number=0;
 if(!rpc || strcmp(rpc,"2.0") || (id && !cstring(id) && !xrtValueGetInt(id,&number))) return 0;
 if(method) {
  const char* name=cstring(method); const xvalue* params=cget(message,"params");
  return name && *name && !result && !error && (!params || xrtValueType(params)==XVALUE_OBJECT || xrtValueType(params)==XVALUE_ARRAY);
 }
 if(!id || (result!=NULL)==(error!=NULL)) return 0;
 return !error || (xrtValueGetInt(cget(error,"code"),&number) && number>=INT_MIN && number<=INT_MAX && cstring(cget(error,"message")));
}
static xvalue* copenMessage(const c_input* d)
{
 xvalue *p=xrtValueObject(),*doc=xrtValueObject();
 int ok=p && doc && cput(doc,"uri",xrtValueString(csv(d->uri))) && cput(doc,"languageId",xrtValueString(csv("c"))) && cput(doc,"version",xrtValueInt(1)) && cput(doc,"text",xrtValueString(csv(d->text))) && xrtValueObjectSet(p,csv("textDocument"),doc);
 xrtValueRelease(doc); if(!ok) { xrtValueRelease(p); return NULL; } return p;
}
/* JSON escaping can grow a small raw document beyond the 8 MiB frame bound.
   Preflight the complete immutable open batch before launching a process. */
static int copenBudget(const xide_c_session* s)
{
 size_t total=0;
 for(size_t i=0;i<s->count;++i) {
  xvalue *params=copenMessage(&s->documents[i]),*message=xrtValueObject();
  int ok=params && message && cput(message,"jsonrpc",xrtValueString(csv("2.0"))) && cput(message,"method",xrtValueString(csv("textDocument/didOpen"))) && xrtValueObjectSet(message,csv("params"),params);
  size_t bytes=0; char* frame=ok ? xideLspFrame(message,&bytes,NULL) : NULL;
  ok=frame && bytes<=56u*1024u*1024u-total; total+=ok ? bytes : 0;
  free(frame); xrtValueRelease(message); xrtValueRelease(params); if(!ok) return 0;
 }
 return 1;
}
static wchar_t* cwide(const char* text)
{
 int n=text ? MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,NULL,0) : 0;
 wchar_t* result=n && n<=32768 ? malloc((size_t)n*sizeof(wchar_t)) : NULL;
 if(result && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,result,n)) { free(result);return NULL; } return result;
}
/* The complete copied batch must be visible before clangd's first preamble.
   didOpen/forceRebuild ordering alone cannot guarantee this: AST requests may
   use a previous preamble while a forced rebuild runs asynchronously. Files
   are exclusively created, held against writes, and deleted on handle close.
   The overlay preserves original names and ordinary on-disk include fallback. */
static int cdraftFile(xide_c_session* s,const char* name,const char* bytes,size_t length,char** path)
{
 *path=NULL;if(s->draftFileCount>=C_DOCUMENTS+1) return 0;
 char* p=xidePathResolveLexical(s->draftDirectory,name); wchar_t* w=cwide(p);
 HANDLE file=w ? CreateFileW(w,GENERIC_WRITE|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL) : INVALID_HANDLE_VALUE;free(w);
 size_t at=0;while(file!=INVALID_HANDLE_VALUE && at<length) { DWORD written=0,n=(DWORD)(length-at>65536 ? 65536 : length-at);if(!WriteFile(file,bytes+at,n,&written,NULL) || !written) break;at+=written; }
 if(file==INVALID_HANDLE_VALUE || at!=length || !FlushFileBuffers(file)) { if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);free(p);return 0; }
 s->draftFiles[s->draftFileCount++]=file;*path=p;return 1;
}
static int cdraftOverlay(xide_c_session* s)
{
 wchar_t temp[32768]; DWORD n=GetTempPathW(32768,temp); if(!n || n>=32768) return 0;
 int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,temp,-1,NULL,0,NULL,NULL); char* base=bytes ? malloc((size_t)bytes) : NULL;
 if(base) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,temp,-1,base,bytes,NULL,NULL);
 char *canonical=base ? xidePathResolve(NULL,base) : NULL,*id=xideCreateId("xide-c-drafts");
 s->draftDirectory=canonical && id ? xidePathResolveLexical(canonical,id) : NULL;free(base);free(canonical);free(id);
 wchar_t* w=cwide(s->draftDirectory); int created=w && CreateDirectoryW(w,NULL);free(w);
 if(!created) { free(s->draftDirectory);s->draftDirectory=NULL;return 0; }
 xvalue *vfs=xrtValueObject(),*roots=xrtValueArray();int ok=vfs && roots;
 for(size_t i=0;ok && i<s->count;++i) {
  char name[40];snprintf(name,sizeof(name),"draft-%zu.txt",i);char* external=NULL;xvalue* file=xrtValueObject();
  ok=file && cdraftFile(s,name,s->documents[i].text,strlen(s->documents[i].text),&external) && cput(file,"type",xrtValueString(csv("file"))) && cput(file,"name",xrtValueString(csv(s->documents[i].path))) && cput(file,"external-contents",xrtValueString(csv(external))) && xrtValueArrayAppend(roots,file);
  free(external);xrtValueRelease(file);
 }
 if(ok) ok=cput(vfs,"version",xrtValueInt(0)) && cput(vfs,"case-sensitive",xrtValueBool(false)) && cput(vfs,"use-external-names",xrtValueBool(false)) && cput(vfs,"redirecting-with",xrtValueString(csv("fallthrough"))) && xrtValueObjectSet(vfs,csv("roots"),roots);
 size_t length=0;char* json=ok ? xrtJsonStringify(vfs,false,&length) : NULL;char* overlay=NULL;
 ok=json && length<=4u*1024u*1024u && cdraftFile(s,"drafts.vfs.json",json,length,&overlay);xrtFree(json);xrtValueRelease(vfs);xrtValueRelease(roots);
 for(size_t i=0;ok && i<xrtValueCount(s->commands);++i) {
  xstrview key={0};xvalue* row=xrtValueObjectAt(s->commands,i,&key);const xvalue* old=cget(row,"compilationCommand");xvalue* args=xrtValueArray();int inserted=0;ok=args!=NULL;
  for(size_t j=0;ok && j<xrtValueCount(old);++j) {
   const char* arg=cstring(xrtValueArrayGet(old,j));
   if(j && !inserted && !strcmp(arg,"--")) { ok=xrtValueArrayAppendNew(args,xrtValueString(csv("-ivfsoverlay"))) && xrtValueArrayAppendNew(args,xrtValueString(csv(overlay)));inserted=1; }
   if(ok) ok=xrtValueArrayAppend(args,xrtValueArrayGet(old,j));
  }
  if(ok && !inserted) ok=xrtValueArrayAppendNew(args,xrtValueString(csv("-ivfsoverlay"))) && xrtValueArrayAppendNew(args,xrtValueString(csv(overlay)));
  if(ok) { ok=xrtValueCount(args)<=1026 && xrtValueObjectSet(row,csv("compilationCommand"),args); } xrtValueRelease(args);
 }
 free(overlay);length=0;json=ok ? xrtJsonStringify(s->commands,false,&length) : NULL;ok=json && length<=4u*1024u*1024u;xrtFree(json);return ok;
}
static void cdraftClear(xide_c_session* s)
{
 for(size_t i=0;i<s->draftFileCount;++i) { CloseHandle(s->draftFiles[i]); } s->draftFileCount=0;
 wchar_t* w=cwide(s->draftDirectory);if(w) RemoveDirectoryW(w);free(w);free(s->draftDirectory);s->draftDirectory=NULL;
}
static int cinitialize(xide_c_session* s)
{
 xvalue *p=xrtValueObject(),*caps=xrtValueObject(),*general=xrtValueObject(),*encodings=xrtValueArray(),*text=xrtValueObject(),*completion=xrtValueObject(),*item=xrtValueObject(),*diagnostics=xrtValueObject(),*symbols=xrtValueObject(),*hover=xrtValueObject(),*formats=xrtValueArray(),*options=xrtValueObject();
 int ok=p && caps && general && encodings && text && completion && item && diagnostics && symbols && hover && formats && options && xrtValueArrayAppendNew(encodings,xrtValueString(csv("utf-16"))) && cput(general,"positionEncodings",xrtValueRetain(encodings)) && cput(caps,"general",xrtValueRetain(general)) && cput(item,"snippetSupport",xrtValueBool(false)) && cput(completion,"completionItem",xrtValueRetain(item)) && cput(text,"completion",xrtValueRetain(completion)) && cput(diagnostics,"versionSupport",xrtValueBool(true)) && cput(text,"publishDiagnostics",xrtValueRetain(diagnostics)) && cput(symbols,"hierarchicalDocumentSymbolSupport",xrtValueBool(true)) && cput(text,"documentSymbol",xrtValueRetain(symbols)) && xrtValueArrayAppendNew(formats,xrtValueString(csv("plaintext"))) && cput(hover,"contentFormat",xrtValueRetain(formats)) && cput(text,"hover",xrtValueRetain(hover)) && cput(caps,"textDocument",xrtValueRetain(text)) && cput(p,"capabilities",xrtValueRetain(caps)) && cput(options,"compilationDatabaseChanges",xrtValueRetain(s->commands)) && cput(p,"initializationOptions",xrtValueRetain(options));
 if(ok) { xvalue* experimental=xrtValueObject(); ok=experimental && cput(experimental,"xlangInputTrace",xrtValueInt(1)) && cput(caps,"experimental",xrtValueRetain(experimental)); xrtValueRelease(experimental); }
 if(ok) ok=csend(s,"initialize",xrtValueRetain(p),1);
 xrtValueRelease(p); xrtValueRelease(caps); xrtValueRelease(general); xrtValueRelease(encodings); xrtValueRelease(text); xrtValueRelease(completion); xrtValueRelease(item); xrtValueRelease(diagnostics); xrtValueRelease(symbols); xrtValueRelease(hover); xrtValueRelease(formats); xrtValueRelease(options); return ok;
}
static xide_c_session* startSession(const char* clangd,const char* directory,const xvalue* commands,const xide_c_document* documents,size_t count,xide_model_error* e,int completion)
{
 if(!clangd || !cabsolute(directory) || !documents || !count || count>(completion ? C_DOCUMENTS : 64u)) { cerror(e,"C session requires 1..64 named documents."); return NULL; }
 xide_c_session* s=calloc(1,sizeof(*s)); if(!s) { cerror(e,"Cannot allocate C session."); return NULL; } s->completion=completion; s->nextId=1; s->generation=(uint64_t)InterlockedIncrement64(&cGeneration); s->started=GetTickCount64(); size_t total=0; int ok=1;
 for(size_t i=0;ok && i<count;++i) {
  const xide_c_document* d=&documents[i]; size_t bytes=d->text ? strlen(d->text) : 0;
  ok=d->id && cabsolute(d->path) && (xidePathHasExtension(d->path,".c") || xidePathHasExtension(d->path,".h")) && d->text && d->version>0 && bytes<=8u*1024u*1024u && bytes<=32u*1024u*1024u-total && (!bytes || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,d->text,(int)bytes,NULL,0));
  for(size_t j=0;ok && j<i;++j) ok=d->id!=s->documents[j].id && !xidePathEqual(d->path,s->documents[j].path) && !xidePathLexicalEqual(d->path,s->documents[j].path);
  if(!ok) break;
  c_input* b=&s->documents[s->count++]; b->id=d->id; b->version=d->version; b->path=xidePathResolveLexical(NULL,d->path); b->uri=b->path ? xideLspPathToUri(b->path,e) : NULL; b->text=ccopy(d->text); ok=b->path && b->uri && b->text; total+=bytes;
 }
 if(ok) ok=ccommandMap(s,commands,&s->commands) && copenBudget(s) && cdraftOverlay(s);
 const char* args[]={"--background-index=0","--enable-config=0","--clang-tidy=0","--use-dirty-headers=1","--header-insertion=never","--header-insertion-decorators=0","--completion-style=detailed","--function-arg-placeholders=0","--pch-storage=memory","--log=error","--limit-results=4096","-j=2","--completion-parse=always","--skip-preamble-build=1"};
 /* clangd 23's precompiled forced header can reapply command-line macros
    after a header's undef/redefine. Parse forced-header sessions without PCH;
    ordinary sessions retain the existing preamble behavior. */
 int forced=0;
 for(size_t i=0;ok && !forced && i<xrtValueCount(commands);++i) {
  const xvalue* row=cget(xrtValueArrayGet(commands,i),"arguments");
  for(size_t j=1;j<xrtValueCount(row);++j) { const char* arg=cstring(xrtValueArrayGet(row,j)); if(arg && (!strcmp(arg,"-include") || !strcmp(arg,"-imacros"))) { forced=1; break; } }
 }
 xide_language_client_spec spec={clangd,directory,args,sizeof(args)/sizeof(*args)-(forced && !completion ? 0 : 1)};
 if(ok) { s->client=xideLanguageClientStartProcess(&spec,e); ok=s->client && cinitialize(s); }
 if(!ok) { xideCSessionDestroy(s); cerror(e,"Invalid C inputs/commands or stdio startup failure."); return NULL; } return s;
}
xide_c_session* xideCSessionStart(const char* clangd,const char* directory,const xvalue* commands,const xide_c_document* documents,size_t count,xide_model_error* e)
{ return startSession(clangd,directory,commands,documents,count,e,0); }
xide_c_session* xideCSessionStartCompletion(const char* clangd,const char* directory,const xvalue* commands,const xide_c_document* documents,size_t count,xide_model_error* e)
{ return startSession(clangd,directory,commands,documents,count,e,1); }
uint64_t xideCSessionGeneration(const xide_c_session* s) { return s ? s->generation : 0; }
void xideCSessionSetWake(xide_c_session* s,void (*wake)(void*),void* user) { if(s) { s->wake=wake; s->wakeUser=user; xideLanguageClientSetWake(s->client,wake,user); } }
enum xide_c_state xideCSessionState(xide_c_session* s,char* error,size_t capacity)
{
 if(!s) return XIDE_C_STOPPED;
 if(s->state<XIDE_C_FAILED && xideLanguageClientState(s->client,s->error,sizeof(s->error))>=XIDE_LS_STOPPED) cfailed(s,*s->error ? s->error : "C language server stopped.");
 if(s->state==XIDE_C_STARTING && GetTickCount64()-s->started>30000) cfailed(s,"C initialization timed out.");
 if(error && capacity) { snprintf(error,capacity,"%s",s->error); } return s->state;
}
int xideCSessionSupports(xide_c_session* s,enum xide_c_query query)
{ return s && query>=XIDE_C_COMPLETION && query<=XIDE_C_SYMBOLS && xideCSessionState(s,NULL,0)==XIDE_C_READY && s->supported[query]; }
int xideCSessionTracing(const xide_c_session* s) { return s && s->tracing; }
static int cprovider(const xvalue* v)
{ bool b=false; return v && (xrtValueType(v)==XVALUE_OBJECT || (xrtValueGetBool(v,&b) && b)); }
static int cready(xide_c_session* s,const xvalue* message)
{
 const xvalue* caps=cget(cget(message,"result"),"capabilities"); const char* encoding=cstring(cget(caps,"positionEncoding")); int sync=0; xvalue* syncValue=cget(caps,"textDocumentSync");
 if(xrtValueType(syncValue)==XVALUE_OBJECT) { bool open=false; if(!xrtValueGetBool(cget(syncValue,"openClose"),&open) || !open) return 0; syncValue=cget(syncValue,"change"); }
 if(cget(message,"error") || !caps || (cget(caps,"positionEncoding") && (!encoding || strcmp(encoding,"utf-16"))) || !cint(syncValue,&sync) || (sync!=1 && sync!=2)) return 0;
 const xvalue* trace=cget(cget(caps,"experimental"),"xlangInputTrace"); int version=0;
 if(trace && (!cint(trace,&version) || version!=1)) return 0;
 s->tracing=version==1; if(s->traceSeen && !s->tracing) return 0;
 s->supported[XIDE_C_COMPLETION]=cprovider(cget(caps,"completionProvider")); s->supported[XIDE_C_DEFINITION]=cprovider(cget(caps,"definitionProvider")); s->supported[XIDE_C_HOVER]=cprovider(cget(caps,"hoverProvider")); s->supported[XIDE_C_SYMBOLS]=cprovider(cget(caps,"documentSymbolProvider"));
 if(!csend(s,"initialized",xrtValueObject(),0)) return 0;
 for(int headers=1;headers>=0;--headers) for(size_t i=0;i<s->count;++i) {
  c_input* d=&s->documents[i]; if(xidePathHasExtension(d->path,".h")!=headers) continue;
  xvalue* p=copenMessage(d); if(!p || !csend(s,"textDocument/didOpen",p,0)) return 0;
 }
 /* Every parse already sees the same immutable VFS batch. Refresh diagnostics
    at wire revision 2; this notification is not a preamble readiness barrier.
    Wire revisions are separate from IDE versions; discard revision-1 diags. */
 for(size_t i=0;i<s->count;++i) {
  xvalue *p=xrtValueObject(),*doc=xrtValueObject(),*changes=xrtValueArray();
  int ok=p && doc && changes && cput(doc,"uri",xrtValueString(csv(s->documents[i].uri))) && cput(doc,"version",xrtValueInt(2)) && xrtValueObjectSet(p,csv("textDocument"),doc) && xrtValueObjectSet(p,csv("contentChanges"),changes) && cput(p,"forceRebuild",xrtValueBool(true)) && cput(p,"wantDiagnostics",xrtValueBool(true));
  if(ok) ok=csend(s,"textDocument/didChange",xrtValueRetain(p),0);
  xrtValueRelease(p); xrtValueRelease(doc); xrtValueRelease(changes); if(!ok) return 0;
 }
 s->state=XIDE_C_READY; return 1;
}
int xideCSessionRequest(xide_c_session* s,enum xide_c_query query,uint64_t documentId,int offset,xide_model_error* e)
{
 if(!xideCSessionSupports(s,query) || s->nextId==INT_MAX) return cerror(e,"C query is unavailable.");
 int index=-1,slot=-1; for(size_t i=0;i<s->count;++i) if(s->documents[i].id==documentId) index=(int)i;
 for(int i=0;i<(int)C_REQUESTS;++i) if(!s->requests[i].id && slot<0) slot=i;
 int line=0,character=0; if(index<0 || slot<0 || (query!=XIDE_C_SYMBOLS && !xideLspOffsetToPosition(s->documents[index].text,offset,&line,&character))) return cerror(e,"Invalid C document, position or request capacity.");
 xvalue *p=xrtValueObject(),*doc=xrtValueObject(),*position=xrtValueObject();
 int ok=p && doc && position && cput(doc,"uri",xrtValueString(csv(s->documents[index].uri))) && xrtValueObjectSet(p,csv("textDocument"),doc);
 if(ok && query!=XIDE_C_SYMBOLS) ok=cput(position,"line",xrtValueInt(line)) && cput(position,"character",xrtValueInt(character)) && xrtValueObjectSet(p,csv("position"),position);
 static const char* methods[]={"textDocument/completion","textDocument/definition","textDocument/hover","textDocument/documentSymbol"}; int id=++s->nextId;
 if(ok) ok=csend(s,methods[query],xrtValueRetain(p),id);
 xrtValueRelease(p); xrtValueRelease(doc); xrtValueRelease(position);
 if(!ok) { cfailed(s,"Cannot send C query."); return cerror(e,s->error); }
 s->requests[slot]=(c_request){id,index,query}; return id;
}
int xideCSessionCancel(xide_c_session* s,int id)
{
 if(!s) return 0;
 for(int i=0;i<(int)C_REQUESTS;++i) if(s->requests[i].id==id && id>1) {
  s->requests[i].id=0; xvalue* p=xrtValueObject(); int ok=p && cput(p,"id",xrtValueInt(id)); if(ok) return csend(s,"$/cancelRequest",p,0); xrtValueRelease(p); return 0;
 } return 0;
}
typedef struct c_points { xide_lsp_position* items; size_t count,capacity; } c_points;
static int caddRange(c_points* points,const xvalue* range)
{
 int sl=0,sc=0,el=0,ec=0;
 if(points->count+2>points->capacity || !cint(cget(cget(range,"start"),"line"),&sl) || !cint(cget(cget(range,"start"),"character"),&sc) || !cint(cget(cget(range,"end"),"line"),&el) || !cint(cget(cget(range,"end"),"character"),&ec) || sl>el || (sl==el && sc>ec)) return 0;
 points->items[points->count++]=(xide_lsp_position){sl,sc,0}; points->items[points->count++]=(xide_lsp_position){el,ec,0}; return 1;
}
static int cdiagnostics(const c_input* doc,const xvalue* values)
{
 if(!values || xrtValueType(values)!=XVALUE_ARRAY || xrtValueCount(values)>4096) return 0;
 size_t count=xrtValueCount(values); c_points points={calloc(count ? count*2 : 1,sizeof(xide_lsp_position)),0,count*2}; if(!points.items) return 0; int ok=1;
 for(size_t i=0;ok && i<count;++i) { const xvalue* d=xrtValueArrayGet(values,i); int severity=0; const char* message=cstring(cget(d,"message")); ok=message && *message && caddRange(&points,cget(d,"range")) && (!cget(d,"severity") || (cint(cget(d,"severity"),&severity) && severity>=1 && severity<=4)); }
 if(ok) { ok=xideLspPositionsToOffsets(doc->text,points.items,points.count); } free(points.items); return ok;
}
static int csymbols(const xvalue* values,c_points* points,int depth,size_t* total)
{
 if(!values || xrtValueType(values)!=XVALUE_ARRAY || depth>64) return 0;
 for(size_t i=0;i<xrtValueCount(values);++i) {
  const xvalue* item=xrtValueArrayGet(values,i); const char* name=cstring(cget(item,"name")); int kind=0;
  if(++*total>8192 || !name || !*name || !cint(cget(item,"kind"),&kind) || kind<1 || kind>26 || !caddRange(points,cget(item,"range")) || !caddRange(points,cget(item,"selectionRange"))) return 0;
  if(cget(item,"children") && !csymbols(cget(item,"children"),points,depth+1,total)) return 0;
 } return 1;
}
static int csymbolResult(const c_input* doc,const xvalue* result)
{
 c_points points={calloc(8192u*4u,sizeof(xide_lsp_position)),0,8192u*4u}; if(!points.items) return 0; size_t total=0;
 int ok=csymbols(result,&points,0,&total) && xideLspPositionsToOffsets(doc->text,points.items,points.count);
 for(size_t i=0;ok && i<points.count;i+=4) ok=points.items[i].offset<=points.items[i+2].offset && points.items[i+3].offset<=points.items[i+1].offset;
 free(points.items); return ok;
}
static int clocation(xide_c_session* s,const xvalue* item)
{
 const char* uri=cstring(cget(item,"uri")); const xvalue* range=cget(item,"range"); int link=0;
 if(!uri) { uri=cstring(cget(item,"targetUri")); range=cget(item,"targetRange"); link=1; }
 char* path=uri ? xideLspUriToPath(uri,NULL) : NULL; if(!path) return 0; int index=cdocument(s,path); free(path);
 xide_lsp_position pair[4]; c_points points={pair,0,4}; int ok=caddRange(&points,range) && (!link || caddRange(&points,cget(item,"targetSelectionRange")));
 if(ok && index>=0) ok=xideLspPositionsToOffsets(s->documents[index].text,points.items,points.count);
 if(ok && link && index>=0) ok=pair[0].offset<=pair[2].offset && pair[3].offset<=pair[1].offset;
 return ok;
}
static int ccompletion(const c_input* doc,const xvalue* result)
{
 const xvalue* list=xrtValueType(result)==XVALUE_ARRAY ? result : cget(result,"items");
 if(!list || xrtValueType(list)!=XVALUE_ARRAY || xrtValueCount(list)>4096) return 0;
 c_points points={calloc(4096u*2u,sizeof(xide_lsp_position)),0,4096u*2u}; if(!points.items) return 0; int ok=1;
 for(size_t i=0;ok && i<xrtValueCount(list);++i) {
  const xvalue* item=xrtValueArrayGet(list,i); const char* label=cstring(cget(item,"label")); int format=0;
  ok=label && *label && (!cget(item,"insertTextFormat") || (cint(cget(item,"insertTextFormat"),&format) && format==1));
  const xvalue* edit=cget(item,"textEdit"); if(ok && edit) ok=cstring(cget(edit,"newText")) && caddRange(&points,cget(edit,"range"));
  if(ok && cget(item,"additionalTextEdits")) ok=xrtValueType(cget(item,"additionalTextEdits"))==XVALUE_ARRAY && xrtValueCount(cget(item,"additionalTextEdits"))==0;
 }
 if(ok) { ok=xideLspPositionsToOffsets(doc->text,points.items,points.count); } free(points.items); return ok;
}
static int ccontents(const xvalue* contents)
{
 if(cstring(contents)) return 1;
 if(xrtValueType(contents)==XVALUE_OBJECT) { const char* kind=cstring(cget(contents,"kind")); return cstring(cget(contents,"value")) && ((kind && (!strcmp(kind,"plaintext") || !strcmp(kind,"markdown"))) || cstring(cget(contents,"language"))); }
 if(xrtValueType(contents)!=XVALUE_ARRAY || xrtValueCount(contents)>128) return 0;
 for(size_t i=0;i<xrtValueCount(contents);++i) { const xvalue* item=xrtValueArrayGet(contents,i); if(!cstring(item) && !(cstring(cget(item,"language")) && cstring(cget(item,"value")))) return 0; } return 1;
}
static int cresult(xide_c_session* s,const c_request* request,const xvalue* message)
{
 if(cget(message,"error")) { int64 code=0; return xrtValueGetInt(cget(cget(message,"error"),"code"),&code) && cstring(cget(cget(message,"error"),"message")); }
 const xvalue* result=cget(message,"result"); if(!result) return 0; if(xrtValueType(result)==XVALUE_NULL) return 1;
 const c_input* doc=&s->documents[request->index];
 if(request->query==XIDE_C_COMPLETION) return ccompletion(doc,result);
 if(request->query==XIDE_C_SYMBOLS) return csymbolResult(doc,result);
 if(request->query==XIDE_C_HOVER) { int start=0,end=0; xvalue* contents=cget(result,"contents"); return ccontents(contents) && (!cget(result,"range") || crange(doc->text,cget(result,"range"),&start,&end)); }
 if(xrtValueType(result)!=XVALUE_ARRAY) return clocation(s,result);
 if(xrtValueCount(result)>128) return 0;
 for(size_t i=0;i<xrtValueCount(result);++i) { if(!clocation(s,xrtValueArrayGet(result,i))) return 0; } return 1;
}
static int crejectRequest(xide_c_session* s,const xvalue* message)
{
 xvalue *reply=xrtValueObject(),*error=xrtValueObject();
 int ok=reply && error && cput(reply,"jsonrpc",xrtValueString(csv("2.0"))) && xrtValueObjectSet(reply,csv("id"),cget(message,"id")) && cput(error,"code",xrtValueInt(-32601)) && cput(error,"message",xrtValueString(csv("Client method is not supported"))) && xrtValueObjectSet(reply,csv("error"),error) && xideLanguageClientSend(s->client,reply,NULL);
 xrtValueRelease(reply); xrtValueRelease(error); return ok;
}
int xideCSessionNext(xide_c_session* s,xide_c_event* event)
{
 if(!s || !event || event->message || xideCSessionState(s,NULL,0)>=XIDE_C_FAILED) return 0;
 xvalue* message=NULL; unsigned consumed=0; uint64_t started=GetTickCount64();
 while(s->client && consumed<32 && GetTickCount64()-started<8 && (message=xideLanguageClientNext(s->client))) {
  ++consumed;
  if(!cenvelope(message)) { xrtValueRelease(message); cfailed(s,"Invalid C JSON-RPC envelope."); return 0; }
  const char* method=cstring(cget(message,"method")); int id=0;
  if(method && cget(message,"id")) {
   if(!crejectRequest(s,message)) { xrtValueRelease(message); cfailed(s,"Cannot reject unsupported C client request."); return 0; }
  } else if(method && !strcmp(method,"xlang/inputTrace")) {
   if(s->state!=XIDE_C_STARTING && !s->tracing) { xrtValueRelease(message); cfailed(s,"C server sent an unnegotiated input trace."); return 0; }
   s->traceSeen=1; *event=(xide_c_event){.kind=XIDE_C_INPUT_TRACE,.generation=s->generation,.message=message}; return 1;
  } else if(method && !strcmp(method,"textDocument/publishDiagnostics") && s->state==XIDE_C_READY) {
   const xvalue* p=cget(message,"params"); const char* uri=cstring(cget(p,"uri")); char* path=uri ? xideLspUriToPath(uri,NULL) : NULL; int index=path ? cdocument(s,path) : -1; int version=0; free(path);
   if(index>=0 && cint(cget(p,"version"),&version) && version==2) {
    if(!cdiagnostics(&s->documents[index],cget(p,"diagnostics"))) { xrtValueRelease(message); cfailed(s,"Invalid C diagnostic ranges or contents."); return 0; }
    *event=(xide_c_event){.kind=XIDE_C_DIAGNOSTICS,.generation=s->generation,.documentId=s->documents[index].id,.version=s->documents[index].version,.message=message}; return 1;
   }
  } else if(cint(cget(message,"id"),&id) && id==1 && s->state==XIDE_C_STARTING) {
   if(!cready(s,message)) { xrtValueRelease(message); cfailed(s,"C server lacks UTF-16/full-document synchronization or initialization failed."); return 0; }
   *event=(xide_c_event){.kind=XIDE_C_INITIALIZED,.generation=s->generation,.requestId=1,.message=message}; return 1;
  } else if(id>1 && s->state==XIDE_C_READY) {
   for(int i=0;i<(int)C_REQUESTS;++i) if(s->requests[i].id==id) {
    c_request request=s->requests[i]; s->requests[i].id=0;
    if(!cresult(s,&request,message)) { xrtValueRelease(message); cfailed(s,"Invalid C response shape or ranges."); return 0; }
    c_input* d=&s->documents[request.index]; *event=(xide_c_event){.kind=XIDE_C_RESULT,.generation=s->generation,.query=request.query,.documentId=d->id,.version=d->version,.requestId=id,.message=message}; return 1;
   }
  }
  xrtValueRelease(message);
 }
 if(s->wake && xideLanguageClientHasPending(s->client)) s->wake(s->wakeUser);
 return 0;
}
void xideCEventClear(xide_c_event* event) { if(event) { xrtValueRelease(event->message); xrtValueRelease(event->sourceDefinition); memset(event,0,sizeof(*event)); } }
void xideCSessionStop(xide_c_session* s)
{ if(s) { xideLanguageClientStop(s->client); s->client=NULL; cdraftClear(s); s->state=XIDE_C_STOPPED; memset(s->requests,0,sizeof(s->requests)); } }
void xideCSessionDestroy(xide_c_session* s)
{
 if(!s) { return; } xideCSessionStop(s); xrtValueRelease(s->commands);
 for(size_t i=0;i<s->count;++i) { free(s->documents[i].path); free(s->documents[i].uri); free(s->documents[i].text); }
 free(s);
}
