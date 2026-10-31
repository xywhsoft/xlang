#include "app/features.h"
#if XIDE_ENABLE_C_LSP
#include "services/c_bridge.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#define CB_PROJECTS 128u
#define CB_FILES 1024u
struct xide_c_bridge_batch {
 char *compiler,*cache; xvalue *projects,*headers,*paths,*directories,*maps;
 xide_language_snapshot documents[64]; size_t count,observedBytes,mapBytes;
 xide_language_check* queries[CB_PROJECTS]; size_t queryCount;
 xide_toolchains* toolchains; HANDLE leases[CB_FILES]; size_t leaseCount;
 int ready; int (*cancelled)(void*); void* user;
};
static xstrview cbsv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* cbget(const xvalue* v,const char* key) { return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,cbsv(key)) : NULL; }
static int cbput(xvalue* v,const char* key,xvalue* value) { return value && xrtValueObjectSetNew(v,cbsv(key),value); }
static const char* cbstr(const xvalue* v,size_t maximum)
{ xstrview s={0}; return v && xrtValueGetString(v,&s) && s.Size<=maximum && !memchr(s.Data,0,s.Size) && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static int cbfail(xide_model_error* e,const char* text) { if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s",text); } return 0; }
static char* cbcopy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static wchar_t* cbwide(const char* s)
{ int n=s ? MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0) : 0; wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL; if(w) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n); return w; }
static int cbstop(xide_c_bridge_batch* b) { return b->cancelled && b->cancelled(b->user); }
static int cbabsolute(const char* s) { return s && *s && (((isalpha((unsigned char)s[0])) && s[1]==':' && (s[2]=='/' || s[2]=='\\')) || (s[0]=='/' && s[1]=='/') || (s[0]=='\\' && s[1]=='\\')); }
static int cbpath(const xvalue* v) { const char* s=cbstr(v,32767); return cbabsolute(s); }
static int cbarray(const xvalue* v,size_t maximum) { return v && xrtValueType(v)==XVALUE_ARRAY && xrtValueCount(v)<=maximum; }
static int cbstrings(const xvalue* v,size_t maximum,int cOnly)
{ if(!cbarray(v,maximum)) return 0; for(size_t i=0;i<xrtValueCount(v);++i) { const char* s=cbstr(xrtValueArrayGet(v,i),32767); if(!cbabsolute(s) || (cOnly && !xidePathHasExtension(s,".c"))) return 0; } return 1; }
static int cbarg(xvalue* args,const char* text) { return xrtValueCount(args)<1024 && xrtValueArrayAppendNew(args,xrtValueString(cbsv(text))); }
static int cbobserve(xide_c_bridge_batch* b,const char* path)
{ size_t n=strlen(path)+1; if(n>16u*1024u*1024u-b->observedBytes) return 0; if(!xrtValueArrayAppendNew(b->paths,xrtValueString(cbsv(path)))) return 0; b->observedBytes+=n; return 1; }
static int cbmkdir(xide_c_bridge_batch* b,const char* path)
{
 for(size_t i=0;i<xrtValueCount(b->directories);++i) if(xidePathLexicalEqual(path,cbstr(xrtValueArrayGet(b->directories,i),32767))) return 1;
 wchar_t* w=cbwide(path); int okay=w && CreateDirectoryW(w,NULL);
 if(okay && !xrtValueArrayAppendNew(b->directories,xrtValueString(cbsv(path)))) { RemoveDirectoryW(w); okay=0; }
 free(w); return okay;
}
static int cbcache(xide_c_bridge_batch* b)
{
 wchar_t w[32768]; DWORD n=GetTempPathW(32768,w); if(!n || n>=32768) return 0;
 int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,NULL,0,NULL,NULL); char* base=bytes ? malloc((size_t)bytes) : NULL;
 if(base) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,base,bytes,NULL,NULL);
 char* canonical=base ? xidePathResolve(NULL,base) : NULL;char* id=xideCreateId("xide-c-bridge"); b->cache=canonical && id ? xidePathResolveLexical(canonical,id) : NULL; free(canonical);free(base); free(id); return b->cache && cbmkdir(b,b->cache);
}
static int cbwrite(xide_c_bridge_batch* b,const char* name,const void* bytes,size_t size,char** path)
{
 *path=NULL; if(cbstop(b) || b->leaseCount>=CB_FILES || size>16u*1024u*1024u) return 0;
 char* relative=cbcopy(name); if(!relative) return 0; int okay=1;
 for(char* slash=strchr(relative,'/');okay && slash;slash=strchr(slash+1,'/')) { *slash=0; char* parent=xidePathResolveLexical(b->cache,relative); okay=parent && cbmkdir(b,parent); free(parent); *slash='/'; }
 free(relative); char* absolute=okay ? xidePathResolveLexical(b->cache,name) : NULL; wchar_t* w=cbwide(absolute);
 HANDLE h=w ? CreateFileW(w,GENERIC_WRITE|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL) : INVALID_HANDLE_VALUE; free(w);
 size_t at=0; if(h!=INVALID_HANDLE_VALUE) while(at<size && !cbstop(b)) { DWORD wrote=0,n=(DWORD)(size-at>65536 ? 65536 : size-at); if(!WriteFile(h,(const char*)bytes+at,n,&wrote,NULL) || !wrote) break; at+=wrote; }
 okay=h!=INVALID_HANDLE_VALUE && at==size && !cbstop(b) && FlushFileBuffers(h) && cbobserve(b,absolute);
 if(okay) { b->leases[b->leaseCount++]=h; *path=absolute; } else { if(h!=INVALID_HANDLE_VALUE) CloseHandle(h); free(absolute); } return okay;
}
static int cbname(const char* s)
{
 size_t n=s ? strlen(s) : 0; if(n<3 || n>512 || strcmp(s+n-2,".h") || (strncmp(s,"include/",8) && strncmp(s,"lib/xrt/",8) && strncmp(s,"lib/xruntime/",13))) return 0;
 for(size_t i=0;i<n;++i) { unsigned char c=(unsigned char)s[i]; if(!(islower(c) || isdigit(c) || c=='_' || c=='-' || c=='.' || c=='/')) return 0; }
 const char* at=s;
 while(*at) { const char* end=strchr(at,'/'); size_t length=end ? (size_t)(end-at) : strlen(at); if(!length || (length==1 && at[0]=='.') || (length==2 && at[0]=='.' && at[1]=='.')) return 0; if(!end) break; at=end+1; } return 1;
}
static int cbhash(const void* data,size_t size,const char* expected)
{ unsigned char digest[32]; char hex[65]; if(!expected || strlen(expected)!=64 || !xrtSha256(data,size,digest)) return 0; for(int i=0;i<32;++i) snprintf(hex+2*i,3,"%02x",digest[i]); return !strcmp(hex,expected); }
static xvalue* cbheader(xide_c_bridge_batch* b,const char* name)
{ for(size_t i=0;i<xrtValueCount(b->headers);++i) { xvalue* row=xrtValueArrayGet(b->headers,i); if(!strcmp(name,cbstr(cbget(row,"name"),512))) return row; } return NULL; }
static int cbheaders(xide_c_bridge_batch* b,const xvalue* environment,xide_model_error* e)
{
 const xvalue* headers=cbget(environment,"builtinHeaders"); bool available=false; int64 totalReported=0; size_t total=0; int initial=xrtValueCount(b->headers)==0;
 if(!xrtValueGetBool(cbget(environment,"embeddedHeadersAvailable"),&available) || !available || !cbarray(headers,512) || !xrtValueCount(headers) || !xrtValueGetInt(cbget(environment,"builtinBytes"),&totalReported) || totalReported<0 || totalReported>32u*1024u*1024u || (!initial && xrtValueCount(headers)!=xrtValueCount(b->headers))) return cbfail(e,"Native bridge lacks a bounded exact embedded-header bundle.");
 for(size_t i=0;i<xrtValueCount(headers);++i) {
  if(cbstop(b)) return 0;
  const xvalue* h=xrtValueArrayGet(headers,i); const char *name=cbstr(cbget(h,"name"),512),*encoded=cbstr(cbget(h,"base64"),24u*1024u*1024u),*digest=cbstr(cbget(h,"sha256"),64); int64 size=0;
  if(!cbname(name) || !encoded || !digest || !xrtValueGetInt(cbget(h,"size"),&size) || size<0 || size>16u*1024u*1024u || (size_t)size>32u*1024u*1024u-total) return cbfail(e,"Embedded native header fields exceed the declared contract.");
  for(size_t j=0;j<i;++j) if(!strcmp(name,cbstr(cbget(xrtValueArrayGet(headers,j),"name"),512))) return cbfail(e,"Duplicate embedded native header name.");
  size_t decoded=0; unsigned char* data=xrtBase64DecodeNew(encoded,strlen(encoded),&decoded,NULL); int okay=data && decoded==(size_t)size && cbhash(data,decoded,digest);
  xvalue* old=cbheader(b,name);
  if(okay && initial) {
   char* path=NULL; xvalue* row=xrtValueObject(); okay=!old && row && cbwrite(b,name,data,decoded,&path) && cbput(row,"name",xrtValueString(cbsv(name))) && cbput(row,"size",xrtValueInt(size)) && cbput(row,"sha256",xrtValueString(cbsv(digest))) && cbput(row,"path",xrtValueString(cbsv(path))) && xrtValueArrayAppend(b->headers,row); free(path); xrtValueRelease(row);
  } else if(okay) { int64 previous=0; okay=old && xrtValueGetInt(cbget(old,"size"),&previous) && previous==size && !strcmp(digest,cbstr(cbget(old,"sha256"),64)); }
  xrtFree(data); if(!okay) return cbfail(e,"Embedded header bytes/digest or same-toolchain bundle changed."); total+=(size_t)size;
 }
 return total==(size_t)totalReported ? 1 : cbfail(e,"Embedded header aggregate length mismatch.");
}
static int cbvfsEntry(xvalue* contents,const char* name,const char* external)
{
 const char* slash=strchr(name,'/'); size_t length=slash ? (size_t)(slash-name) : strlen(name); char component[513]; if(length>=sizeof(component)) return 0; memcpy(component,name,length); component[length]=0;
 xvalue* entry=NULL;
 for(size_t i=0;i<xrtValueCount(contents);++i) if(!strcmp(component,cbstr(cbget(xrtValueArrayGet(contents,i),"name"),512))) entry=xrtValueArrayGet(contents,i);
 if(!entry) {
  entry=xrtValueObject(); int okay=entry && cbput(entry,"type",xrtValueString(cbsv(slash ? "directory" : "file"))) && cbput(entry,"name",xrtValueString(cbsv(component))) && cbput(entry,slash ? "contents" : "external-contents",slash ? xrtValueArray() : xrtValueString(cbsv(external))) && xrtValueArrayAppend(contents,entry);
  xrtValueRelease(entry); if(!okay) return 0; entry=xrtValueArrayGet(contents,xrtValueCount(contents)-1);
 } else if(!slash || !cbget(entry,"contents")) return 0;
 return slash ? cbvfsEntry(cbget(entry,"contents"),slash+1,external) : 1;
}
static int cbvfs(xide_c_bridge_batch* b,const char* directory,size_t index,char** path)
{
 xvalue *vfs=xrtValueObject(),*roots=xrtValueArray(),*root=xrtValueObject(),*contents=xrtValueArray(); int okay=vfs && roots && root && contents;
 for(size_t i=0;okay && i<xrtValueCount(b->headers);++i) {
  xvalue* row=xrtValueArrayGet(b->headers,i); const char* name=cbstr(cbget(row,"name"),512); char* candidate=xidePathResolveLexical(directory,name);
  okay=candidate && cbobserve(b,candidate) && cbvfsEntry(contents,name,cbstr(cbget(row,"path"),32767)); free(candidate);
 }
 if(okay) okay=cbput(root,"type",xrtValueString(cbsv("directory"))) && cbput(root,"name",xrtValueString(cbsv(directory))) && xrtValueObjectSet(root,cbsv("contents"),contents) && xrtValueArrayAppend(roots,root) && cbput(vfs,"version",xrtValueInt(0)) && cbput(vfs,"case-sensitive",xrtValueBool(false)) && cbput(vfs,"use-external-names",xrtValueBool(false)) && cbput(vfs,"redirecting-with",xrtValueString(cbsv("fallback"))) && xrtValueObjectSet(vfs,cbsv("roots"),roots);
 size_t bytes=0; char* json=okay ? xrtJsonStringify(vfs,false,&bytes) : NULL; char name[64]; snprintf(name,sizeof(name),"profile-%zu.vfs.json",index); okay=json && bytes<=4u*1024u*1024u && cbwrite(b,name,json,bytes,path);
 xrtFree(json); xrtValueRelease(vfs); xrtValueRelease(roots); xrtValueRelease(root); xrtValueRelease(contents); return okay;
}
static int cboperations(xvalue* args,const xvalue* operations,const char* directory)
{
 if(!cbarray(operations,2048)) return 0;
 size_t bytes=0;
 for(size_t i=0;i<xrtValueCount(operations);++i) {
  const xvalue* o=xrtValueArrayGet(operations,i); const char *kind=cbstr(cbget(o,"kind"),16),*text=cbstr(cbget(o,"text"),32767),*value=cbstr(cbget(o,"value"),4096); bool required=false;
  if(!kind || !text || !value || !xrtValueGetBool(cbget(o,"required"),&required) || strlen(text)+strlen(value)>1024u*1024u-bytes) return 0;
  bytes+=strlen(text)+strlen(value);
  if(!strcmp(kind,"options")) {
   if(!strcmp(text,"-std=c11 -D_CRT_SECURE_NO_WARNINGS")) { if(!cbarg(args,"-std=c11") || !cbarg(args,"-D_CRT_SECURE_NO_WARNINGS")) return 0; }
   else if(!strcmp(text,"-DXL_STATIC_EMBED") || !strcmp(text,"-DXL_RUNTIME_ONLY_DLL")) { if(!cbarg(args,text)) return 0; }
   else return 0;
  } else if(!strcmp(kind,"include")) {
   char* path=xidePathResolveLexical(directory,text); int okay=path && !*value && cbarg(args,"-I") && cbarg(args,path); free(path); if(!okay) return 0;
  } else if(!strcmp(kind,"define")) {
   size_t n=strlen(text)+strlen(value)+2; char* macro=malloc(n); if(!macro) return 0; snprintf(macro,n,"%s=%s",text,value);
   int okay=xideCAnalysisMacroValid(macro,1) && cbarg(args,"-D") && cbarg(args,macro); free(macro); if(!okay) return 0;
  } else return 0;
 }
 return 1;
}
static int cbscalarTarget(xvalue* args,const xvalue* old,const xvalue* environment)
{
 const xvalue* profile=cbget(environment,"scalarTarget");
 if(!profile) return 1; /* Earlier compiler queries keep their documented scope. */
 const char* scope=cbstr(cbget(profile,"scope"),64); int64 schema=0,version=0,pointer=0,longBytes=0,doubleBytes=0,doubleAlign=0;
 if(!scope || strcmp(scope,"tcc-win64-scalar-target") || !xrtValueGetInt(cbget(profile,"schema"),&schema) || schema!=1 ||
    !xrtValueGetInt(cbget(profile,"tinyCVersion"),&version) || version<900 || version>999 ||
    !xrtValueGetInt(cbget(profile,"pointerBytes"),&pointer) || pointer!=8 ||
    !xrtValueGetInt(cbget(profile,"longBytes"),&longBytes) || longBytes!=4 ||
    !xrtValueGetInt(cbget(profile,"longDoubleBytes"),&doubleBytes) || doubleBytes!=8 ||
    !xrtValueGetInt(cbget(profile,"longDoubleAlign"),&doubleAlign) || doubleAlign!=8) return 0;
 size_t targets=0;
 for(size_t i=1;i<xrtValueCount(old);++i) {
  const char* option=cbstr(xrtValueArrayGet(old,i),32767); if(!option) return 0;
  if(!strcmp(option,"-target")) {
   const char* target=++i<xrtValueCount(old) ? cbstr(xrtValueArrayGet(old,i),64) : NULL;
   if(!target || strcmp(target,"x86_64-w64-windows-gnu")) return 0;
   ++targets;
  }
 }
 if(targets!=1) return 0;
 char tiny[32]; snprintf(tiny,sizeof(tiny),"__TINYC__=%lld",(long long)version);
 /* Only this finite identity subset is aligned, not all Clang predefines.
    Project/user macro operations follow these defaults and retain precedence. */
 return cbarg(args,"-mlong-double-64") && cbarg(args,"-U__clang__") && cbarg(args,"-U__GNUC__") &&
        cbarg(args,"-U__GNUC_MINOR__") && cbarg(args,"-U__GNUC_PATCHLEVEL__") && cbarg(args,"-D") && cbarg(args,tiny);
}
static int cbrecordTarget(xvalue* args,const xvalue* environment)
{
 const xvalue* profile=cbget(environment,"recordLayout");
 if(!profile) return 1;
 /* This producer field describes the configured Win64 TCC default.
    User analysis overrides and source-level attributes retain precedence. */
 const char* scope=cbstr(cbget(profile,"scope"),64); int64 schema=0; bool microsoft=false;
 if(!cbget(environment,"scalarTarget") || !scope || strcmp(scope,"tcc-win64-bitfield-default") ||
    !xrtValueGetInt(cbget(profile,"schema"),&schema) || schema!=1 ||
    !xrtValueGetBool(cbget(profile,"msBitfields"),&microsoft)) return 0;
 return cbarg(args,microsoft ? "-mms-bitfields" : "-mno-ms-bitfields");
}
static int cbapply(xvalue* database,const xvalue* project,const xvalue* environment,const char* prelude,const char* overlay)
{
 const xvalue* files=cbget(project,"files"); const char* directory=cbstr(cbget(project,"directory"),32767);
 for(size_t i=0;i<xrtValueCount(files);++i) {
  const char* file=cbstr(xrtValueArrayGet(files,i),32767); xvalue* row=NULL;
  for(size_t j=0;j<xrtValueCount(database);++j) if(xidePathLexicalEqual(file,cbstr(cbget(xrtValueArrayGet(database,j),"file"),32767))) row=xrtValueArrayGet(database,j);
  const xvalue* old=cbget(row,"arguments"); if(!row || !cbarray(old,1024) || !xrtValueCount(old)) return 0;
  const char* driver=cbstr(xrtValueArrayGet(old,0),32767); xvalue* args=xrtValueArray(); int okay=driver && args && cbarg(args,driver) && cbscalarTarget(args,old,environment) && cbrecordTarget(args,environment) && cboperations(args,cbget(environment,"operations"),directory) && cbarg(args,"-ivfsoverlay") && cbarg(args,overlay) && cbarg(args,"-include") && cbarg(args,prelude);
  for(size_t j=1;okay && j<xrtValueCount(old);++j) {
   const char* arg=cbstr(xrtValueArrayGet(old,j),32767); if(!arg) { okay=0; break; }
   if(!strcmp(arg,"-I")) { okay=++j<xrtValueCount(old); continue; }
   if(!strcmp(arg,"-DXL6_BUILD_SHARED=1")) continue;
   okay=cbarg(args,arg);
  }
  if(okay) okay=xrtValueObjectSet(row,cbsv("arguments"),args);
  xrtValueRelease(args); if(!okay) return 0;
 }
 return 1;
}
static int cbmapStore(xide_c_bridge_batch* b,const xvalue* result,const xvalue* project,const char* header,const char* text,size_t prefix)
{
 const xvalue* bindings=cbget(result,"sourceBindings"),*files=cbget(result,"sourceFiles");
 if(!b->maps || !cbarray(bindings,32768) || !xrtValueCount(bindings)) return 1;
 if(!cbarray(files,512)) return 0;
 /* Sources were read, hashed and identity-checked by the same query. Only disk
    targets enter this guard; copied xlang overlays are guarded by UI versions. */
 for(size_t i=0;i<xrtValueCount(files);++i) {
  const xvalue* file=xrtValueArrayGet(files,i);const char *path=cbstr(cbget(file,"path"),32767),*origin=cbstr(cbget(file,"origin"),16);int64 kind=0;
  if(!path || !origin || !xrtValueGetInt(cbget(file,"kind"),&kind)) return 0;
  if((kind&1) && !strcmp(origin,"disk") && !cbobserve(b,path)) return 0;
 }
 xvalue* row=xrtValueObject();int okay=row && cbput(row,"header",xrtValueString(cbsv(header))) && cbput(row,"text",xrtValueString(cbsv(text))) && cbput(row,"prefix",xrtValueInt((int64)prefix)) && xrtValueObjectSet(row,cbsv("bindings"),bindings) && xrtValueObjectSet(row,cbsv("files"),cbget(project,"files"));
 size_t size=0;char* encoded=okay ? xrtJsonStringify(row,false,&size) : NULL;
 if(encoded && size<=32u*1024u*1024u-b->mapBytes && xrtValueArrayAppend(b->maps,row)) b->mapBytes+=size;
 xrtFree(encoded);xrtValueRelease(row);return 1; /* Navigation alone may exhaust its batch budget. */
}
xide_c_bridge_batch* xideCBridgeCopy(const xide_c_bridge_spec* spec,xide_model_error* e)
{
 if(!spec || !spec->compiler || !cbabsolute(spec->compiler) || strlen(spec->compiler)>32767 || !cbarray(spec->projects,CB_PROJECTS) || !xrtValueCount(spec->projects) || spec->count>64 || (spec->count && !spec->documents)) { cbfail(e,"Invalid copied native bridge inputs."); return NULL; }
 size_t serialized=0; char* json=xrtJsonStringify(spec->projects,false,&serialized); int okay=json && serialized<=4u*1024u*1024u; xrtFree(json);
 xide_c_bridge_batch* b=calloc(1,sizeof(*b)); if(!b) return NULL; b->compiler=cbcopy(spec->compiler); b->projects=xrtValueDeepClone(spec->projects); b->headers=xrtValueArray(); b->paths=xrtValueArray(); b->directories=xrtValueArray(); b->maps=xrtValueArray(); okay=okay && b->compiler && b->projects && b->headers && b->paths && b->directories; size_t total=0;
 for(size_t i=0;okay && i<xrtValueCount(b->projects);++i) {
  const xvalue* p=xrtValueArrayGet(b->projects,i); bool shared=false; const char* entry=cbstr(cbget(p,"entry"),32767); const xvalue* files=cbget(p,"files");
  okay=cbpath(cbget(p,"directory")) && cbabsolute(entry) && xidePathHasExtension(entry,".xl") && xrtValueGetBool(cbget(p,"shared"),&shared) && cbstrings(cbget(p,"includes"),128,0) && cbstrings(cbget(p,"cSources"),2048,1) && cbstrings(files,2048,0) && xrtValueCount(files);
  for(size_t k=0;okay && k<xrtValueCount(files);++k) {
   const char* file=cbstr(xrtValueArrayGet(files,k),32767); okay=xidePathHasExtension(file,".c") || xidePathHasExtension(file,".h");
   for(size_t j=0;okay && j<i;++j) { const xvalue* previous=cbget(xrtValueArrayGet(b->projects,j),"files"); for(size_t h=0;okay && h<xrtValueCount(previous);++h) if(xidePathLexicalEqual(file,cbstr(xrtValueArrayGet(previous,h),32767))) okay=0; }
  }
 }
 for(size_t i=0;okay && i<spec->count;++i) {
  const xide_language_snapshot* d=&spec->documents[i]; size_t n=d->text ? strlen(d->text) : 0;
  okay=d->path && d->text && d->version>0 && cbabsolute(d->path) && xidePathHasExtension(d->path,".xl") && n<=8u*1024u*1024u && n<=32u*1024u*1024u-total;
  if(okay) { xide_language_snapshot* to=&b->documents[b->count++]; *to=(xide_language_snapshot){cbcopy(d->path),cbcopy(d->text),d->version}; okay=to->path && to->text; total+=n; }
 }
 if(!okay) { xideCBridgeDestroy(b); cbfail(e,"Native bridge projects/overlays conflict or exceed capacity."); return NULL; } return b;
}
int xideCBridgePrepare(xide_c_bridge_batch* b,xvalue** commands,int (*cancelled)(void*),void* user,xide_model_error* e)
{
 if(!b || b->ready || b->cache || !commands || !*commands) return cbfail(e,"Invalid native bridge preparation.");
 b->cancelled=cancelled; b->user=user; b->toolchains=xideToolchainsCreate(); xvalue* database=xrtValueDeepClone(*commands); int okay=b->toolchains && database && cbcache(b);
 for(size_t i=0;okay && i<xrtValueCount(b->projects);++i) {
  const xvalue* project=xrtValueArrayGet(b->projects,i); const char *directory=cbstr(cbget(project,"directory"),32767),*entry=cbstr(cbget(project,"entry"),32767); const xvalue *includes=cbget(project,"includes"),*sources=cbget(project,"cSources");
  const char *includePaths[128],*cSources[2048]; for(size_t j=0;j<xrtValueCount(includes);++j) includePaths[j]=cbstr(xrtValueArrayGet(includes,j),32767); for(size_t j=0;j<xrtValueCount(sources);++j) cSources[j]=cbstr(xrtValueArrayGet(sources,j),32767);
  bool shared=false; xrtValueGetBool(cbget(project,"shared"),&shared); xide_language_bridge_options options={includePaths,xrtValueCount(includes),cSources,xrtValueCount(sources),shared,cancelled,user};
  xide_language_check* q=xideLanguageBridgeRun(b->toolchains,b->compiler,directory,entry,b->documents,b->count,&options,e); if(!q) { okay=0; break; } b->queries[b->queryCount++]=q;
  xvalue* result=xideLanguageBridgeTake(q); const xvalue* bridge=cbget(result,"cBridge"),*environment=cbget(bridge,"nativeEnvironment"); const char *preamble=cbstr(cbget(bridge,"runtimePreamble"),256),*declarations=cbstr(cbget(bridge,"declarations"),1024u*1024u),*scope=cbstr(cbget(bridge,"scope"),64),*environmentScope=cbstr(cbget(environment,"scope"),64),*kind=cbstr(cbget(environment,"outputKind"),16),*base=cbstr(cbget(environment,"directory"),32767); bool complete=false,graph=true,abi=true,cInputs=true; int64 schema=0;
  const char* guard=cbstr(cbget(bridge,"headerGuard"),64);
  okay=bridge && guard && !strcmp(guard,"XL6_MODULE_H") && xrtValueGetBool(cbget(bridge,"cInputsObserved"),&cInputs) && !cInputs && scope && !strcmp(scope,"lowered-xlang-native-declarations") && xrtValueGetInt(cbget(bridge,"schema"),&schema) && schema==1 && preamble && !strcmp(preamble,"#include <__xl_runtime/xruntime_decl.h>\n") && declarations && *declarations && environmentScope && !strcmp(environmentScope,"compiler-configured-native-c") && xrtValueGetInt(cbget(environment,"schema"),&schema) && schema==1 && xrtValueGetBool(cbget(environment,"complete"),&complete) && complete && xrtValueGetBool(cbget(environment,"cPreprocessorGraphComplete"),&graph) && !graph && xrtValueGetBool(cbget(environment,"abiEquivalenceGuaranteed"),&abi) && !abi && kind && !strcmp(kind,shared ? "shared" : "exe") && base && xidePathLexicalEqual(base,directory) && cbheaders(b,environment,e);
  char *prelude=NULL,*overlay=NULL,*text=NULL;
  if(okay) { size_t n=strlen(preamble)+strlen(declarations)+1; text=malloc(n); if(text) snprintf(text,n,"%s%s",preamble,declarations); char name[64]; snprintf(name,sizeof(name),"profile-%zu.h",i); okay=text && cbwrite(b,name,text,n-1,&prelude) && cbvfs(b,directory,i,&overlay) && cbapply(database,project,environment,prelude,overlay) && cbmapStore(b,result,project,prelude,text,strlen(preamble)); }
  free(text); free(prelude); free(overlay); xrtValueRelease(result);
 }
 size_t size=0; char* json=okay ? xrtJsonStringify(database,false,&size) : NULL; okay=json && size<=4u*1024u*1024u && !cbstop(b); xrtFree(json);
 if(okay) { b->ready=1; okay=xideCBridgeGuard(b,e); }
 if(okay) { xrtValueRelease(*commands); *commands=database; database=NULL; } else if(e && !*e->message) cbfail(e,"Native bridge environment/header/command validation failed.");
 xrtValueRelease(database); return okay;
}
const xvalue* xideCBridgeObservedPaths(xide_c_bridge_batch* b) { return b ? b->paths : NULL; }
int xideCBridgePrivatePath(xide_c_bridge_batch* b,const char* path)
{
 if(!b || !b->cache || !path) return 0;
 char* relative=xidePathRelative(b->cache,path); int privatePath=relative && !cbabsolute(relative) && strcmp(relative,"..") && strncmp(relative,"../",3) && strncmp(relative,"..\\",3); free(relative); return privatePath;
}
static int cbint(const xvalue* v,int* result)
{ int64 n=0;if(!v || !xrtValueGetInt(v,&n) || n<0 || n>INT_MAX) return 0;*result=(int)n;return 1; }
static int cbposition(const xvalue* value,xide_lsp_position* p)
{ return cbint(cbget(value,"line"),&p->line) && cbint(cbget(value,"character"),&p->character); }
xvalue* xideCBridgeDefinition(xide_c_bridge_batch* b,const char* queryPath,const xvalue* reply)
{
 if(!b || !b->ready || !queryPath || cbstop(b) || cbget(reply,"error")) return NULL;
 const xvalue* item=cbget(reply,"result");if(xrtValueType(item)==XVALUE_ARRAY) { if(!xrtValueCount(item)) return NULL;item=xrtValueArrayGet(item,0); }
 const char* uri=cbstr(cbget(item,"uri"),131072);const xvalue* range=cbget(item,"range");int link=0;
 if(!uri) { uri=cbstr(cbget(item,"targetUri"),131072);range=cbget(item,"targetSelectionRange");link=1; }
 char* path=uri ? xideLspUriToPath(uri,NULL) : NULL;xvalue* result=NULL;
 for(size_t i=0;path && !result && i<xrtValueCount(b->maps);++i) {
  const xvalue* row=xrtValueArrayGet(b->maps,i);const char* header=cbstr(cbget(row,"header"),32767);if(!header || !xidePathLexicalEqual(header,path)) continue;
  const xvalue* files=cbget(row,"files");int owner=0;
  for(size_t j=0;j<xrtValueCount(files);++j) if(xidePathLexicalEqual(queryPath,cbstr(xrtValueArrayGet(files,j),32767))) owner=1;
  if(!owner) break;
  const char* text=cbstr(cbget(row,"text"),1024u*1024u+256);int prefix=0;xide_lsp_position points[4]={0};
  if(!text || !cbint(cbget(row,"prefix"),&prefix) || !cbposition(cbget(range,"start"),points) || !cbposition(cbget(range,"end"),points+1) || !xideLspPositionsToOffsets(text,points,2) || points[0].offset<prefix || points[0].offset>=points[1].offset) break;
  if(link) { const xvalue* outer=cbget(item,"targetRange");if(!cbposition(cbget(outer,"start"),points+2) || !cbposition(cbget(outer,"end"),points+3) || !xideLspPositionsToOffsets(text,points+2,2) || points[2].offset>points[0].offset || points[1].offset>points[3].offset) break; }
  const xvalue* bindings=cbget(row,"bindings");size_t lo=0,hi=xrtValueCount(bindings);int start=points[0].offset-prefix,end=points[1].offset-prefix;
  while(lo<hi) { size_t mid=lo+(hi-lo)/2;int at=0;if(!cbint(cbget(cbget(xrtValueArrayGet(bindings,mid),"headerRange"),"startOffset"),&at)) { hi=0;break; } if(at<start) lo=mid+1;else hi=mid; }
  if(lo<xrtValueCount(bindings)) { const xvalue* binding=xrtValueArrayGet(bindings,lo);int a=0,z=0;if(cbint(cbget(cbget(binding,"headerRange"),"startOffset"),&a) && cbint(cbget(cbget(binding,"headerRange"),"endOffset"),&z) && a==start && z==end) result=xrtValueDeepClone(binding); }
  break;
 }
 free(path);return result;
}
int xideCBridgeGuard(xide_c_bridge_batch* b,xide_model_error* e)
{ if(!b || !b->ready || cbstop(b)) return 0; for(size_t i=0;i<b->queryCount;++i) if(!xideLanguageBridgeGuard(b->queries[i],e)) return 0; return 1; }
void xideCBridgeDestroy(xide_c_bridge_batch* b)
{
 if(!b) return;
 for(size_t i=0;i<b->queryCount;++i) xideLanguageCheckDestroy(b->queries[i]);
 xideToolchainsDestroy(b->toolchains);
 for(size_t i=0;i<b->leaseCount;++i) CloseHandle(b->leases[i]);
 for(size_t i=xrtValueCount(b->directories);i>0;--i) { wchar_t* w=cbwide(cbstr(xrtValueArrayGet(b->directories,i-1),32767)); if(w) RemoveDirectoryW(w); free(w); }
 for(size_t i=0;i<b->count;++i) { free((char*)b->documents[i].path); free((char*)b->documents[i].text); }
 free(b->compiler); free(b->cache); xrtValueRelease(b->projects); xrtValueRelease(b->headers); xrtValueRelease(b->paths); xrtValueRelease(b->directories); xrtValueRelease(b->maps); free(b);
}

#endif /* XIDE_ENABLE_C_LSP */
