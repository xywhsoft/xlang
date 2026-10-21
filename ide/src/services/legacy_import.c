#define _WIN32_WINNT 0x0601
#include "services/legacy_import.h"
#include "platform/path.h"
#include "platform/file_rename.h"
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MANIFEST_LIMIT (16u*1024u*1024u)
#define SOURCE_LIMIT (128u*1024u*1024u)
#define ITEM_LIMIT 10000u
typedef struct source_snapshot {
 char* path; unsigned char* bytes; size_t length; BY_HANDLE_FILE_INFORMATION identity; int missing;
} source_snapshot;
typedef struct legacy_project {
 char *id,*source,*base,*manifest,*fileName,*name,*entry,*kind,*outputFile,*outputDir;
 xide_strings candidates;
 xvalue *original,*reference,*files,*cSources,*libraries,*hints,*json;
 char* text; int supported;
} legacy_project;
struct xide_legacy_import {
 char *source,*parent,*directory,*workspacePath,*name,*startup;
 source_snapshot* snapshots; size_t snapshotCount,snapshotCapacity,sourceBytes;
 legacy_project* projects; size_t projectCount,projectCapacity;
 xvalue *original,*files,*fileHints,*issues,*workspace,*report,*hints;
 char *workspaceText,*reportText,*hintsText;
 BY_HANDLE_FILE_INFORMATION parentIdentity;
 int inputKind,committed;
};
static int fail(xide_model_error* e,int code,const char* s)
{ if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static void success(xide_model_error* e) { if(e) memset(e,0,sizeof(*e)); }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* k) { return v ? xrtValueObjectGet(v,sv(k)) : NULL; }
static int object(const xvalue* v) { return v && xrtValueIs(v,XVALUE_OBJECT); }
static int array(const xvalue* v) { return v && xrtValueIs(v,XVALUE_ARRAY); }
static const char* text(const xvalue* v)
{ xstrview s={0}; return v && xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) ? s.Data : NULL; }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s))); }
static int set(xvalue* v,const char* k,xvalue* owned) { return owned && xrtValueObjectSetNew(v,sv(k),owned); }
static int clone(xvalue* v,const char* k,const xvalue* source)
{ return source ? set(v,k,xrtValueDeepClone(source)) : 1; }
static int reserve(void** items,size_t* capacity,size_t count,size_t bytes)
{
 if(count<=*capacity) return 1;
 size_t next=*capacity ? *capacity*2 : 8; if(next<count) next=count;
 if(next>ITEM_LIMIT || next>SIZE_MAX/bytes) next=count;
 if(next>ITEM_LIMIT || next>SIZE_MAX/bytes) return 0;
 void* p=realloc(*items,next*bytes); if(!p) return 0; *items=p; *capacity=next; return 1;
}
static wchar_t* native(const char* s)
{
 int n=s ? MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0) : 0;
 wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL; if(!w) return NULL;
 if(n>32000 || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n)) { free(w); return NULL; }
 for(int i=0;i<n;++i) if(w[i]==L'/') w[i]=L'\\';
 if(!wcsncmp(w,L"\\\\?\\",4)) return w;
 int unc=w[0]==L'\\' && w[1]==L'\\'; const wchar_t* prefix=unc ? L"\\\\?\\UNC\\" : L"\\\\?\\";
 size_t a=wcslen(prefix),b=unc ? 2 : 0; wchar_t* out=malloc((a+(size_t)n-b)*sizeof(*out));
 if(out) { memcpy(out,prefix,a*sizeof(*out)); memcpy(out+a,w+b,((size_t)n-b)*sizeof(*out)); }
 free(w); return out;
}
static int unused(const char* path,xide_model_error* e)
{
 wchar_t* w=native(path); if(!w) return fail(e,XIDE_MODEL_INVALID,"导入目标路径无效。");
 DWORD a=GetFileAttributesW(w),code=GetLastError(); free(w);
 if(a!=INVALID_FILE_ATTRIBUTES) return fail(e,XIDE_MODEL_CONFLICT,"导入目标已存在，请选择一个新目录。");
 return code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND ? 1 : fail(e,XIDE_MODEL_IO,"无法检查导入目标。");
}
static int sameIdentity(const BY_HANDLE_FILE_INFORMATION* a,const BY_HANDLE_FILE_INFORMATION* b)
{ return a->dwVolumeSerialNumber==b->dwVolumeSerialNumber && a->nFileIndexHigh==b->nFileIndexHigh && a->nFileIndexLow==b->nFileIndexLow; }
static int parentSnapshot(const char* path,BY_HANDLE_FILE_INFORMATION* out,xide_model_error* e)
{
 wchar_t* w=native(path); HANDLE h=w ? CreateFileW(w,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; free(w);
 int okay=h!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(h,out) && (out->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && !(out->dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT);
 if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
 return okay ? 1 : fail(e,XIDE_MODEL_IO,"导入父目录须为可访问的普通目录。");
}
static int sameInfo(const BY_HANDLE_FILE_INFORMATION* a,const BY_HANDLE_FILE_INFORMATION* b)
{
 return a->dwVolumeSerialNumber==b->dwVolumeSerialNumber && a->nFileIndexHigh==b->nFileIndexHigh && a->nFileIndexLow==b->nFileIndexLow &&
 a->nFileSizeHigh==b->nFileSizeHigh && a->nFileSizeLow==b->nFileSizeLow && a->dwFileAttributes==b->dwFileAttributes &&
 a->ftLastWriteTime.dwHighDateTime==b->ftLastWriteTime.dwHighDateTime && a->ftLastWriteTime.dwLowDateTime==b->ftLastWriteTime.dwLowDateTime;
}
static int readHandle(HANDLE h,source_snapshot* out,xide_model_error* e)
{
 BY_HANDLE_FILE_INFORMATION after={0}; LARGE_INTEGER zero={0};
 if(!GetFileInformationByHandle(h,&out->identity) || !SetFilePointerEx(h,zero,NULL,FILE_BEGIN)) return fail(e,XIDE_MODEL_IO,"无法读取旧清单基线。");
 if(out->identity.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) return fail(e,XIDE_MODEL_INVALID,"旧清单须为普通文件。");
 uint64_t length=((uint64_t)out->identity.nFileSizeHigh<<32)|out->identity.nFileSizeLow;
 if(length>MANIFEST_LIMIT) return fail(e,XIDE_MODEL_INVALID,"旧清单超过 16 MiB 限制。");
 out->bytes=malloc((size_t)length+1); if(!out->bytes) return fail(e,XIDE_MODEL_MEMORY,"无法分配旧清单快照。");
 out->length=(size_t)length; size_t at=0;
 while(at<out->length) { DWORD done=0,want=out->length-at>65536 ? 65536 : (DWORD)(out->length-at); if(!ReadFile(h,out->bytes+at,want,&done,NULL) || done!=want) return fail(e,XIDE_MODEL_IO,"旧清单读取失败。"); at+=done; }
 out->bytes[out->length]=0;
 if(!GetFileInformationByHandle(h,&after) || !sameInfo(&out->identity,&after)) return fail(e,XIDE_MODEL_CONFLICT,"读取期间旧清单发生变化，请重新准备导入。");
 return 1;
}
static void clearSnapshot(source_snapshot* s) { free(s->path); free(s->bytes); memset(s,0,sizeof(*s)); }
static int readSource(xide_legacy_import* p,const char* path,int allowMissing,xvalue** root,xide_model_error* e)
{
 *root=NULL;
 for(size_t i=0;i<p->snapshotCount;++i) if(xidePathEqual(p->snapshots[i].path,path)) {
  source_snapshot* s=&p->snapshots[i]; if(s->missing) return allowMissing ? 1 : fail(e,XIDE_MODEL_IO,"旧清单不存在。");
  size_t skip=s->length>=3 && !memcmp(s->bytes,"\xef\xbb\xbf",3) ? 3 : 0;
  *root=xrtJsonParse((xstrview){(char*)s->bytes+skip,s->length-skip}); return 1;
 }
 source_snapshot s={0}; s.path=copy(path); wchar_t* w=native(path);
 HANDLE h=w ? CreateFileW(w,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE;
 DWORD code=GetLastError(); free(w); int okay=s.path!=NULL;
 if(h==INVALID_HANDLE_VALUE) {
  if(allowMissing && (code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND)) s.missing=1;
  else okay=fail(e,XIDE_MODEL_IO,"无法读取旧清单；访问受限的引用须先修复再导入。");
 } else { okay=okay && readHandle(h,&s,e); CloseHandle(h); }
 if(okay && (p->sourceBytes>SOURCE_LIMIT-s.length || !reserve((void**)&p->snapshots,&p->snapshotCapacity,p->snapshotCount+1,sizeof(s)))) okay=fail(e,XIDE_MODEL_MEMORY,"清单总量超过 128 MiB 或 10000 项限制，或内存不足。");
 if(okay) {
  p->snapshots[p->snapshotCount++]=s; p->sourceBytes+=s.length;
  if(!s.missing) { size_t skip=s.length>=3 && !memcmp(s.bytes,"\xef\xbb\xbf",3) ? 3 : 0; *root=xrtJsonParse((xstrview){(char*)s.bytes+skip,s.length-skip}); }
 } else clearSnapshot(&s);
 return okay;
}
static int issue(xvalue* list,const char* code,const char* source,const char* id,const char* message)
{
 xvalue* v=xrtValueObject(); int okay=v && put(v,"severity","warning") && put(v,"code",code) && put(v,"source",source ? source : "") && put(v,"projectId",id ? id : "") && put(v,"message",message);
 if(okay) return xrtValueArrayAppendNew(list,v);
 xrtValueRelease(v); return 0;
}
static int legacyShape(const xvalue* root,int kind,xide_model_error* e)
{
 int64 version=0; uint64 unsignedVersion=0;
 int v1=xrtValueGetInt(get(root,"Version"),&version) && version==1;
 if(!v1) v1=xrtValueGetUInt(get(root,"Version"),&unsignedVersion) && unsignedVersion==1;
 if(!object(root) || get(root,"format") || !v1) return fail(e,XIDE_MODEL_INVALID,"仅支持无 format 字段的 Version=1 旧清单，不能混入新格式。");
 if(kind==XIDE_IMPORT_WORKSPACE) {
  if(!array(get(root,"Projects")) || (get(root,"Files") && !array(get(root,"Files"))) || (get(root,"Name") && (!text(get(root,"Name")) || !*text(get(root,"Name"))))) return fail(e,XIDE_MODEL_INVALID,"旧工作空间 Name/Projects/Files 类型无效。");
 } else if(!object(get(root,"Project")) || !object(get(root,"Build")) || !array(get(root,"File")) || !array(get(root,"Library")) || (get(root,"Resource") && !array(get(root,"Resource")))) return fail(e,XIDE_MODEL_INVALID,"旧项目 Project/Build/File/Library/Resource 类型无效。");
 if(get(root,"Status") && !object(get(root,"Status"))) return fail(e,XIDE_MODEL_INVALID,"旧展开状态须为对象。");
 return 1;
}
static int validOpen(const xvalue* v,xide_model_error* e)
{ bool b=false; return !get(v,"Open") || xrtValueGetBool(get(v,"Open"),&b) ? 1 : fail(e,XIDE_MODEL_INVALID,"旧引用的 Open 须为布尔值。"); }
static int opened(const xvalue* v) { bool b=false; xrtValueGetBool(get(v,"Open"),&b); return b; }
static const char* fileKind(const char* path,int resource)
{
 if(resource) return "resource";
 if(xidePathHasExtension(path,".xl")) return "xlang";
 if(xidePathHasExtension(path,".c")) return "c";
 if(xidePathHasExtension(path,".h")) return "header";
 return "text";
}
static xvalue* matchingFile(xvalue* files,const char* base,const char* path)
{
 for(size_t i=0;i<xrtValueCount(files);++i) {
  xvalue* v=xrtValueArrayGet(files,i); char* absolute=xidePathResolve(base,text(get(v,"path")));
  int same=absolute && xidePathEqual(absolute,path); free(absolute); if(same) return v;
 }
 return NULL;
}
static int addOldFile(xide_legacy_import* p,xvalue* files,xvalue* hints,xvalue* cSources,const char* base,const xvalue* old,int resource,const char* projectId,xide_model_error* e)
{
 const char* path=text(get(old,"Path"));
 if(!object(old) || !path || !*path || !validOpen(old,e)) return fail(e,XIDE_MODEL_INVALID,"旧文件引用须包含非空 Path 和有效 Open。");
 char* absolute=xidePathResolve(base,path); char* relative=absolute ? xidePathRelative(p->directory,absolute) : NULL;
 if(!absolute || !relative) { free(absolute); free(relative); return fail(e,XIDE_MODEL_MEMORY,"无法准备旧文件路径。"); }
 xvalue* existing=matchingFile(files,p->directory,absolute); int okay=1;
 if(existing) {
  okay=xrtValueArrayAppendNew(get(existing,"legacyEntries"),xrtValueDeepClone(old)) && issue(p->issues,"duplicate-file",absolute,projectId,"同一物理文件的重复引用已合并，所有旧引用完整保留，按首次种类分类。");
  if(opened(old)) for(size_t i=0;okay && i<xrtValueCount(hints);++i) { xvalue* h=xrtValueArrayGet(hints,i); if(!strcmp(text(get(h,"id")),text(get(existing,"id")))) okay=set(h,"open",xrtValueBool(true)); }
 } else {
  char* id=xideCreateId("file"); xvalue *v=xrtValueObject(),*h=xrtValueObject(),*originals=xrtValueArray(); const char* kind=fileKind(absolute,resource);
  okay=id && v && h && originals && xrtValueCount(files)<ITEM_LIMIT && put(v,"id",id) && put(v,"path",relative) && put(v,"kind",kind) && xrtValueArrayAppendNew(originals,xrtValueDeepClone(old)) && xrtValueObjectSet(v,sv("legacyEntries"),originals) && put(h,"id",id) && put(h,"path",relative) && set(h,"open",xrtValueBool(opened(old)!=0));
  if(okay && cSources && !strcmp(kind,"c")) okay=xrtValueArrayAppendNew(cSources,xrtValueString(sv(relative)));
  if(okay) okay=xrtValueArrayAppend(files,v) && xrtValueArrayAppend(hints,h);
  if(okay && (!xidePathExists(absolute) || xidePathIsDirectory(absolute))) okay=issue(p->issues,"missing-file",absolute,projectId,"文件缺失或不是普通文件，保留引用供后续修复。");
  if(okay && resource) okay=issue(p->issues,"resource-review",absolute,projectId,"旧资源保留为普通资源，数据 JSON/XSON 不作为编译资源配置，需手工映射 Windows 资源。");
  free(id); xrtValueRelease(v); xrtValueRelease(h); xrtValueRelease(originals);
 }
 free(absolute); free(relative); return okay ? 1 : fail(e,e && e->code ? e->code : XIDE_MODEL_MEMORY,"无法准备文件引用或迁移报告。");
}
static int importFiles(xide_legacy_import* p,xvalue* files,xvalue* hints,xvalue* cSources,const char* base,const xvalue* list,int resource,const char* id,xide_model_error* e)
{
 for(size_t i=0;list && i<xrtValueCount(list);++i) if(!addOldFile(p,files,hints,cSources,base,xrtValueArrayGet(list,i),resource,id,e)) return 0;
 return 1;
}
static void clearProject(legacy_project* q)
{
 free(q->id); free(q->source); free(q->base); free(q->manifest); free(q->fileName); free(q->name); free(q->entry); free(q->kind); free(q->outputFile); free(q->outputDir);
 for(size_t i=0;i<q->candidates.count;++i) free(q->candidates.items[i]);
 free(q->candidates.items);
 xrtValueRelease(q->original); xrtValueRelease(q->reference); xrtValueRelease(q->files); xrtValueRelease(q->cSources); xrtValueRelease(q->libraries); xrtValueRelease(q->hints); xrtValueRelease(q->json); xrtFree(q->text); memset(q,0,sizeof(*q));
}
static int projectData(xide_legacy_import* p,legacy_project* q,xide_model_error* e)
{
 const xvalue *meta=get(q->original,"Project"),*build=get(q->original,"Build");
 const char *description=text(get(meta,"Description")),*appPath=text(get(meta,"AppPath")),*output=text(get(build,"File"));
 if((get(meta,"Description") && !description) || (get(meta,"AppPath") && !appPath) || (get(build,"File") && !output) || (get(q->original,"BootCode") && !text(get(q->original,"BootCode")))) return fail(e,XIDE_MODEL_INVALID,"旧项目描述、输出位置/文件或 BootCode 类型无效。");
 if((description && !strcmp(description,"$Project_Description")) || (appPath && !strcmp(appPath,"$Project_AppPath")) || (output && !strcmp(output,"$Build_File"))) return fail(e,XIDE_MODEL_INVALID,"这是尚未替换占位符的旧项目模板，请先实例化为真实项目。");
 q->name=copy(description && *description ? description : xidePathFilename(q->source));
 q->outputFile=copy(output ? output : ""); q->outputDir=xidePathResolve(q->base,appPath && *appPath ? appPath : ".");
 q->kind=copy(!output || !*output || xidePathHasExtension(output,".exe") ? "exe" : xidePathHasExtension(output,".dll") ? "dll" : "");
 q->files=xrtValueArray(); q->cSources=xrtValueArray(); q->libraries=xrtValueArray(); q->hints=xrtValueObject();
 xvalue* fileHints=xrtValueArray();
 int okay=q->name && q->outputFile && q->outputDir && q->kind && q->files && q->cSources && q->libraries && q->hints && fileHints && put(q->hints,"id",q->id) && set(q->hints,"open",xrtValueBool(opened(q->reference)!=0)) && clone(q->hints,"status",get(q->original,"Status")) && xrtValueObjectSet(q->hints,sv("files"),fileHints);
 if(okay) okay=importFiles(p,q->files,fileHints,q->cSources,q->base,get(q->original,"File"),0,q->id,e) && importFiles(p,q->files,fileHints,NULL,q->base,get(q->original,"Resource"),1,q->id,e);
 xrtValueRelease(fileHints);
 for(size_t i=0;okay && i<xrtValueCount(q->files);++i) {
  xvalue* v=xrtValueArrayGet(q->files,i);
  if(!strcmp(text(get(v,"kind")),"xlang")) {
   char* candidate=xidePathResolve(p->directory,text(get(v,"path")));
   okay=candidate && reserve((void**)&q->candidates.items,&q->candidates.capacity,q->candidates.count+1,sizeof(char*));
   if(okay) q->candidates.items[q->candidates.count++]=candidate; else free(candidate);
  }
 }
 q->entry=copy(q->candidates.count==1 ? q->candidates.items[0] : ""); okay=okay && q->entry;
 const xvalue* libs=get(q->original,"Library");
 for(size_t i=0;okay && i<xrtValueCount(libs);++i) {
  const char* name=text(xrtValueArrayGet(libs,i)); if(!name || !*name) return fail(e,XIDE_MODEL_INVALID,"旧 Library 项须为非空字符串，复杂库声明需要手工处理。");
  xvalue* lib=xrtValueObject(); char* id=xideCreateId("library");
  okay=lib && id && put(lib,"id",id) && put(lib,"name",name) && put(lib,"kind",xidePathHasExtension(name,".dll") ? "legacy-dll" : "package") && set(lib,"legacy",xrtValueString(sv(name)));
  if(okay) okay=xrtValueArrayAppendNew(q->libraries,lib); else xrtValueRelease(lib); free(id);
  if(okay) okay=issue(p->issues,"library-review",q->source,q->id,"旧库名仅保留为引用元数据，新编译器不会据此自动导入或链接，须检查 ABI、模块和链接配置。");
 }
 if(okay && xrtValueCount(build)) okay=issue(p->issues,"build-review",q->source,q->id,"只映射旧输出目录与文件名。图标、版本、进程名、字符集及其他构建字段完整保留，需检查后配置。");
 if(okay) okay=issue(p->issues,"run-review",q->source,q->id,"早期 IDE 从活动源文件目录调用 xl run，新 IDE 构建后运行产物。运行目录按所选入口目录设置，参数/环境与旧工具链行为须检查。");
 if(okay && text(get(q->original,"BootCode")) && *text(get(q->original,"BootCode"))) okay=issue(p->issues,"boot-code-review",q->source,q->id,"BootCode 仅保留在原始数据中，不注入新入口，请检查初始化语义。");
 if(okay && !strcmp(q->kind,"dll")) okay=issue(p->issues,"dll-review",q->source,q->id,"输出类型按 .dll 映射，但旧导出 ABI 和启动行为须手工核对。");
 return okay;
}
static int addProject(xide_legacy_import* p,const char* source,const xvalue* reference,int direct,xide_model_error* e)
{
 for(size_t i=0;i<p->projectCount;++i) if(xidePathEqual(p->projects[i].source,source)) {
  legacy_project* previous=&p->projects[i];
  if(previous->supported && opened(reference) && !set(previous->hints,"open",xrtValueBool(true))) return fail(e,XIDE_MODEL_MEMORY,"无法合并旧项目打开状态。");
  return issue(p->issues,"duplicate-project",source,previous->id,"重复物理项目引用已合并，旧工作空间原始数据保留全部引用。");
 }
 legacy_project q={0}; char name[64]; snprintf(name,sizeof(name),"project-%04zu.xlp",p->projectCount+1);
 q.id=xideCreateId("project"); q.source=copy(source); q.base=xidePathDirectory(source); q.fileName=copy(name); q.manifest=xidePathResolve(p->directory,name); q.reference=reference ? xrtValueDeepClone(reference) : xrtValueObject();
 int okay=q.id && q.source && q.base && q.fileName && q.manifest && q.reference && readSource(p,source,!direct,&q.original,e);
 xide_model_error shape={0}; q.supported=okay && legacyShape(q.original,XIDE_IMPORT_PROJECT,&shape);
 if(okay && !q.supported) {
  if(direct) { if(e) *e=shape; okay=0; }
  else okay=issue(p->issues,"unavailable-project",source,q.id,"缺失、损坏或不支持的项目保留原路径，不生成替代清单，请修复后重新导入。");
 }
 if(okay && q.supported) {
  size_t issueStart=xrtValueCount(p->issues); xide_model_error dataError={0};
  okay=projectData(p,&q,&dataError);
  if(!okay && !direct && dataError.code==XIDE_MODEL_INVALID) {
   legacy_project keep={0}; keep.id=q.id; q.id=NULL; keep.source=q.source; q.source=NULL; keep.base=q.base; q.base=NULL;
   keep.manifest=q.manifest; q.manifest=NULL; keep.fileName=q.fileName; q.fileName=NULL;
   keep.original=q.original; q.original=NULL; keep.reference=q.reference; q.reference=NULL;
   clearProject(&q); q=keep;
   if(xrtValueCount(p->issues)>issueStart) xrtValueArrayRemove(p->issues,issueStart,xrtValueCount(p->issues)-issueStart);
   okay=issue(p->issues,"unavailable-project",source,q.id,"旧项目字段无效，保留原路径和原始数据，不生成部分转换的项目。");
  } else if(!okay) { if(e) *e=dataError; }
 }
 if(okay) okay=reserve((void**)&p->projects,&p->projectCapacity,p->projectCount+1,sizeof(q));
 if(okay) p->projects[p->projectCount++]=q; else { clearProject(&q); if(e && !e->code) fail(e,XIDE_MODEL_MEMORY,"无法准备旧项目。"); }
 return okay;
}
static char* defaultOutput(const legacy_project* q)
{
 if(*q->outputFile) return xidePathResolve(q->outputDir,q->outputFile);
 if(!*q->entry) return copy("");
 const char* name=xidePathFilename(q->entry); size_t n=strlen(name); char* stem=malloc(n+5); if(!stem) return NULL;
 memcpy(stem,name,n-3); strcpy(stem+n-3,!strcmp(q->kind,"dll") ? ".dll" : ".exe"); char* out=xidePathResolve(q->outputDir,stem); free(stem); return out;
}
static xvalue* projectJson(const xide_legacy_import* p,const legacy_project* q)
{
 xvalue *root=xrtValueObject(),*ide=xrtValueObject(),*legacy=xrtValueObject(),*configs=xrtValueArray(),*config=xrtValueObject(),*run=xrtValueObject();
 char *entry=*q->entry ? xidePathRelative(p->directory,q->entry) : copy(""),*output=defaultOutput(q),*relativeOutput=output && *output ? xidePathRelative(p->directory,output) : copy(""),*working=*q->entry ? xidePathDirectory(q->entry) : copy(q->base),*directory=working ? xidePathRelative(p->directory,working) : NULL;
 int okay=root && ide && legacy && configs && config && run && entry && relativeOutput && directory && put(root,"format","xlang-project-1") && (!*q->kind || put(root,"kind",q->kind)) && (!*entry || put(root,"entry",entry)) && clone(root,"cSources",q->cSources) && set(root,"resources",xrtValueArray()) && put(ide,"format","xlang-ide-project-1") && put(ide,"name",q->name) && put(ide,"description",q->name) && put(ide,"activeConfiguration","imported") && clone(ide,"files",q->files) && clone(ide,"libraries",q->libraries) && put(legacy,"source",q->source) && clone(legacy,"original",q->original) && xrtValueObjectSet(ide,sv("legacy"),legacy) && put(config,"id","imported") && put(config,"name","旧项目导入配置") && put(config,"output",relativeOutput) && set(config,"includePaths",xrtValueArray()) && set(config,"libraryPaths",xrtValueArray()) && set(config,"linkLibraries",xrtValueArray()) && put(run,"workingDirectory",directory) && set(run,"arguments",xrtValueArray()) && set(run,"environment",xrtValueObject()) && xrtValueObjectSet(config,sv("run"),run) && xrtValueArrayAppend(configs,config) && xrtValueObjectSet(ide,sv("configurations"),configs) && xrtValueObjectSet(root,sv("ide"),ide);
 free(entry); free(output); free(relativeOutput); free(working); free(directory); xrtValueRelease(ide); xrtValueRelease(legacy); xrtValueRelease(configs); xrtValueRelease(config); xrtValueRelease(run);
 if(!okay) { xrtValueRelease(root); return NULL; } return root;
}
static int ready(const xide_legacy_import* p)
{ if(!p) return 0; for(size_t i=0;i<p->projectCount;++i) if(p->projects[i].supported && (!*p->projects[i].entry || !*p->projects[i].kind)) return 0; return 1; }
/* Allocate every new preview before replacing any plan state. */
static int refresh(xide_legacy_import* p,xide_model_error* e)
{
 xvalue *ws=xrtValueObject(),*projects=xrtValueArray(),*legacy=xrtValueObject(),*report=xrtValueObject(),*issues=xrtValueDeepClone(p->issues),*summaries=xrtValueArray(),*hints=xrtValueObject(),*projectHints=xrtValueArray();
 xvalue** json=calloc(p->projectCount ? p->projectCount : 1,sizeof(*json)); char** texts=calloc(p->projectCount ? p->projectCount : 1,sizeof(*texts));
 char *wsText=NULL,*reportText=NULL,*hintsText=NULL;
 int okay=ws && projects && legacy && report && issues && summaries && hints && projectHints && json && texts && put(ws,"format","xlang-workspace-1") && put(ws,"name",p->name) && put(ws,"startupProject",p->startup) && clone(ws,"files",p->files) && put(legacy,"source",p->source) && clone(legacy,"original",p->original) && xrtValueObjectSet(ws,sv("legacy"),legacy);
 for(size_t i=0;okay && i<p->projectCount;++i) {
  legacy_project* q=&p->projects[i];
  char* relative=q->supported ? copy(q->fileName) : xidePathRelative(p->directory,q->source);
  xvalue *ref=xrtValueObject(),*summary=xrtValueObject();
  okay=relative && ref && summary && put(ref,"id",q->id) && put(ref,"path",relative) && clone(ref,"legacy",q->reference) && xrtValueArrayAppend(projects,ref) && put(summary,"id",q->id) && put(summary,"source",q->source) && put(summary,"manifest",q->supported ? q->manifest : q->source) && set(summary,"supported",xrtValueBool(q->supported!=0)) && clone(summary,"original",q->original) && clone(summary,"reference",q->reference);
  if(okay && q->supported) {
   json[i]=projectJson(p,q); okay=json[i]!=NULL;
   if(okay && *q->entry && *q->kind) { xide_project model={0}; okay=xideProjectDecode(&model,q->manifest,json[i],e); xideProjectClear(&model); }
   if(okay) { texts[i]=xrtJsonStringify(json[i],true,NULL); okay=texts[i] && put(summary,"entry",q->entry) && put(summary,"kind",q->kind); }
   if(okay && !*q->entry) okay=issue(issues,"entry-required",q->source,q->id,"旧 IDE 按活动文件构建，请选择 .xl 入口后提交，Open 顺序不代表入口。");
   if(okay && !*q->kind) okay=issue(issues,"kind-required",q->source,q->id,"旧输出后缀无法确定应用类型，请明确选择 exe 或 dll 后提交。");
  }
  if(okay) {
   xvalue* hint=q->supported ? xrtValueDeepClone(q->hints) : xrtValueObject();
   okay=hint && put(hint,"id",q->id) && put(hint,"manifest",q->supported ? q->manifest : q->source) && set(hint,"supported",xrtValueBool(q->supported!=0));
   if(okay && !q->supported) okay=set(hint,"open",xrtValueBool(opened(q->reference)!=0)) && clone(hint,"status",get(q->original,"Status")) && set(hint,"files",xrtValueArray());
   if(okay) okay=xrtValueArrayAppendNew(projectHints,hint); else xrtValueRelease(hint);
  }
  if(okay) okay=xrtValueArrayAppend(summaries,summary);
  free(relative); xrtValueRelease(ref); xrtValueRelease(summary);
 }
 if(okay) okay=xrtValueObjectSet(ws,sv("projects"),projects) && put(report,"format","xlang-ide-legacy-import-report-1") && put(report,"source",p->source) && put(report,"destination",p->directory) && set(report,"ready",xrtValueBool(ready(p)!=0)) && put(report,"startupProject",p->startup) && put(report,"assetPolicy","reference-original-assets") && clone(report,"original",p->original) && xrtValueObjectSet(report,sv("issues"),issues) && xrtValueObjectSet(report,sv("projects"),summaries) && put(hints,"format","xlang-ide-legacy-session-hints-1") && put(hints,"workspace",p->workspacePath) && clone(hints,"status",get(p->original,"Status")) && clone(hints,"newFileIndex",get(p->original,"NewFileIndex")) && clone(hints,"files",p->fileHints) && xrtValueObjectSet(hints,sv("projects"),projectHints);
 if(okay) { xide_workspace model={0}; okay=xideWorkspaceDecode(&model,p->workspacePath,ws,e); xideWorkspaceClear(&model); }
 if(okay) { wsText=xrtJsonStringify(ws,true,NULL); reportText=xrtJsonStringify(report,true,NULL); hintsText=xrtJsonStringify(hints,true,NULL); okay=wsText && reportText && hintsText; }
 if(okay) {
  xrtValueRelease(p->workspace); p->workspace=ws; ws=NULL; xrtValueRelease(p->report); p->report=report; report=NULL; xrtValueRelease(p->hints); p->hints=hints; hints=NULL;
  xrtFree(p->workspaceText); p->workspaceText=wsText; wsText=NULL; xrtFree(p->reportText); p->reportText=reportText; reportText=NULL; xrtFree(p->hintsText); p->hintsText=hintsText; hintsText=NULL;
  for(size_t i=0;i<p->projectCount;++i) { xrtValueRelease(p->projects[i].json); p->projects[i].json=json[i]; json[i]=NULL; xrtFree(p->projects[i].text); p->projects[i].text=texts[i]; texts[i]=NULL; }
 }
 for(size_t i=0;i<p->projectCount;++i) { if(json) xrtValueRelease(json[i]); if(texts) xrtFree(texts[i]); }
 free(json); free(texts); xrtFree(wsText); xrtFree(reportText); xrtFree(hintsText); xrtValueRelease(ws); xrtValueRelease(projects); xrtValueRelease(legacy); xrtValueRelease(report); xrtValueRelease(issues); xrtValueRelease(summaries); xrtValueRelease(hints); xrtValueRelease(projectHints);
 return okay ? 1 : fail(e,e && e->code ? e->code : XIDE_MODEL_MEMORY,"无法生成有效的新清单与迁移预览。");
}
xide_legacy_import* xideLegacyImportPrepare(const char* source,const char* destination,int kind,xide_model_error* e)
{
 success(e); if(!source || !destination || (kind!=XIDE_IMPORT_WORKSPACE && kind!=XIDE_IMPORT_PROJECT) || !xidePathHasExtension(source,kind==XIDE_IMPORT_WORKSPACE ? ".xlw" : ".xlp")) { fail(e,XIDE_MODEL_INVALID,"选择对应的旧 .xlw 或 .xlp 文件和新的导入目录。"); return NULL; }
 xide_legacy_import* p=calloc(1,sizeof(*p)); if(!p) { fail(e,XIDE_MODEL_MEMORY,"无法分配导入计划。"); return NULL; }
 p->inputKind=kind; p->source=xidePathResolve(NULL,source); p->directory=xidePathResolveLexical(NULL,destination); p->parent=p->directory ? xidePathDirectory(p->directory) : NULL; p->workspacePath=p->directory ? xidePathResolve(p->directory,"workspace.xlw") : NULL;
 /* Canonicalize the existing parent while preserving the requested leaf. */
 if(p->directory && p->parent) {
  char* canonicalParent=xidePathResolve(NULL,p->parent);
  char* canonicalTarget=canonicalParent ? xidePathResolveLexical(canonicalParent,xidePathFilename(p->directory)) : NULL;
  if(canonicalParent && canonicalTarget) {
   free(p->parent); p->parent=canonicalParent; free(p->directory); p->directory=canonicalTarget;
   free(p->workspacePath); p->workspacePath=xidePathResolve(p->directory,"workspace.xlw");
  } else { free(canonicalParent); free(canonicalTarget); }
 }
 p->startup=copy(""); p->files=xrtValueArray(); p->fileHints=xrtValueArray(); p->issues=xrtValueArray();
 int okay=p->source && p->directory && p->parent && p->workspacePath && p->startup && p->files && p->fileHints && p->issues && xidePathIsDirectory(p->parent) && xideFileRenameTargetValid(p->directory,e) && unused(p->directory,e) && parentSnapshot(p->parent,&p->parentIdentity,e);
 if(okay) okay=readSource(p,p->source,0,&p->original,e) && legacyShape(p->original,kind,e);
 const char* name=okay && kind==XIDE_IMPORT_WORKSPACE ? text(get(p->original,"Name")) : NULL;
 if(okay) { p->name=copy(name ? name : xidePathFilename(p->source)); okay=p->name!=NULL; }
 char* base=okay ? xidePathDirectory(p->source) : NULL;
 if(okay && kind==XIDE_IMPORT_PROJECT) okay=addProject(p,p->source,NULL,1,e);
 for(size_t i=0;okay && kind==XIDE_IMPORT_WORKSPACE && i<xrtValueCount(get(p->original,"Projects"));++i) {
  xvalue* ref=xrtValueArrayGet(get(p->original,"Projects"),i); const char* path=text(get(ref,"Path")); char* absolute=path && *path ? xidePathResolve(base,path) : NULL;
  okay=object(ref) && absolute && validOpen(ref,e) && addProject(p,absolute,ref,0,e); free(absolute);
  if(!okay && e && !e->code) fail(e,XIDE_MODEL_INVALID,"旧 Projects 项须包含有效 Path/Open。");
 }
 if(okay && kind==XIDE_IMPORT_WORKSPACE) okay=importFiles(p,p->files,p->fileHints,NULL,base,get(p->original,"Files"),0,NULL,e);
 if(okay) okay=issue(p->issues,"review-original-semantics",p->source,NULL,"旧清单、未知字段和状态完整保留，导入只建立新引用和配置，不转换源代码语义。会话提示另存，由显式导入流程决定是否应用。") && refresh(p,e);
 free(base); if(!okay) { if(e && !e->code) fail(e,XIDE_MODEL_INVALID,"导入路径、旧清单或引用无效。"); xideLegacyImportDestroy(p); return NULL; } return p;
}
void xideLegacyImportDestroy(xide_legacy_import* p)
{
 if(!p) return;
 for(size_t i=0;i<p->snapshotCount;++i) clearSnapshot(&p->snapshots[i]);
 for(size_t i=0;i<p->projectCount;++i) clearProject(&p->projects[i]);
 free(p->snapshots); free(p->projects); free(p->source); free(p->parent); free(p->directory); free(p->workspacePath); free(p->name); free(p->startup);
 xrtValueRelease(p->original); xrtValueRelease(p->files); xrtValueRelease(p->fileHints); xrtValueRelease(p->issues); xrtValueRelease(p->workspace); xrtValueRelease(p->report); xrtValueRelease(p->hints); xrtFree(p->workspaceText); xrtFree(p->reportText); xrtFree(p->hintsText); free(p);
}
const char* xideLegacyImportDirectory(const xide_legacy_import* p) { return p ? p->directory : ""; }
const char* xideLegacyImportWorkspacePath(const xide_legacy_import* p) { return p ? p->workspacePath : ""; }
size_t xideLegacyImportProjectCount(const xide_legacy_import* p) { return p ? p->projectCount : 0; }
const char* xideLegacyImportProjectId(const xide_legacy_import* p,size_t i) { return p && i<p->projectCount ? p->projects[i].id : ""; }
const char* xideLegacyImportProjectSource(const xide_legacy_import* p,size_t i) { return p && i<p->projectCount ? p->projects[i].source : ""; }
int xideLegacyImportProjectSupported(const xide_legacy_import* p,size_t i) { return p && i<p->projectCount && p->projects[i].supported; }
size_t xideLegacyImportEntryCount(const xide_legacy_import* p,size_t i) { return p && i<p->projectCount ? p->projects[i].candidates.count : 0; }
const char* xideLegacyImportEntryAt(const xide_legacy_import* p,size_t i,size_t candidate)
{ return p && i<p->projectCount && candidate<p->projects[i].candidates.count ? p->projects[i].candidates.items[candidate] : NULL; }
static int editable(xide_legacy_import* p,size_t i,xide_model_error* e)
{ success(e); return p && !p->committed && i<p->projectCount && p->projects[i].supported ? 1 : fail(e,XIDE_MODEL_INVALID,"导入计划或项目不可编辑。"); }
int xideLegacyImportSetEntry(xide_legacy_import* p,size_t i,const char* path,xide_model_error* e)
{
 if(!editable(p,i,e) || !path || !xidePathHasExtension(path,".xl")) return fail(e,XIDE_MODEL_INVALID,"选择有效的 .xl 构建入口。");
 legacy_project* q=&p->projects[i]; char* next=xidePathResolve(q->base,path);
 if(!next) return fail(e,XIDE_MODEL_MEMORY,"无法准备入口路径。");
 xvalue* v=matchingFile(q->files,p->directory,next);
 if(v && strcmp(text(get(v,"kind")),"xlang")) { free(next); return fail(e,XIDE_MODEL_INVALID,"已分类为其他种类的文件不能作为导入入口，请先修复旧清单。"); }
 if(v) {
  char* before=q->entry; q->entry=next;
  if(!refresh(p,e)) { q->entry=before; free(next); return 0; } free(before); return 1;
 }
 /* Explicit external/zero-candidate choice: allocate a private file/hint/issue
    branch and candidate array before publishing any preview or plan state. */
 xide_legacy_import trial=*p; trial.issues=xrtValueDeepClone(p->issues);
 xvalue *files=xrtValueDeepClone(q->files),*hints=xrtValueDeepClone(q->hints),*old=xrtValueObject();
 size_t count=q->candidates.count; char** candidates=count<ITEM_LIMIT ? malloc((count+1)*sizeof(*candidates)) : NULL;
 char* candidate=copy(next);
 int okay=trial.issues && files && hints && old && candidates && candidate && put(old,"Path",next) && set(old,"Open",xrtValueBool(false)) && put(old,"source","explicit-entry-choice");
 if(okay) okay=addOldFile(&trial,files,get(hints,"files"),NULL,q->base,old,0,q->id,e) && issue(trial.issues,"entry-added",next,q->id,"显式选择了旧清单之外的入口，加入新项目文件列表，旧清单不变。");
 xrtValueRelease(old);
 if(okay) {
  if(count) { memcpy(candidates,q->candidates.items,count*sizeof(*candidates)); } candidates[count]=candidate;
  char* before=q->entry; xvalue *oldFiles=q->files,*oldHints=q->hints,*oldIssues=p->issues; xide_strings oldCandidates=q->candidates;
  q->entry=next; q->files=files; q->hints=hints; p->issues=trial.issues; q->candidates=(xide_strings){candidates,count+1,count+1};
  okay=refresh(p,e);
  if(okay) { free(before); xrtValueRelease(oldFiles); xrtValueRelease(oldHints); xrtValueRelease(oldIssues); free(oldCandidates.items); return 1; }
  q->entry=before; q->files=oldFiles; q->hints=oldHints; p->issues=oldIssues; q->candidates=oldCandidates;
 }
 free(next); free(candidate); free(candidates); xrtValueRelease(files); xrtValueRelease(hints); xrtValueRelease(trial.issues);
 return fail(e,e && e->code ? e->code : XIDE_MODEL_MEMORY,"无法准备显式入口引用。");
}
int xideLegacyImportSetKind(xide_legacy_import* p,size_t i,const char* kind,xide_model_error* e)
{
 if(!editable(p,i,e) || !kind || (strcmp(kind,"exe") && strcmp(kind,"dll"))) return fail(e,XIDE_MODEL_INVALID,"导入类型须为 exe 或 dll。");
 legacy_project* q=&p->projects[i]; char* next=copy(kind); if(!next) return fail(e,XIDE_MODEL_MEMORY,"无法准备输出类型。");
 char* before=q->kind; q->kind=next; if(!refresh(p,e)) { q->kind=before; free(next); return 0; } free(before); return 1;
}
int xideLegacyImportSetStartup(xide_legacy_import* p,const char* id,xide_model_error* e)
{
 success(e); if(!p || p->committed || !id) return fail(e,XIDE_MODEL_INVALID,"无效的启动项目选择。");
 int found=!*id; for(size_t i=0;i<p->projectCount;++i) if(p->projects[i].supported && !strcmp(id,p->projects[i].id)) found=1;
 if(!found) return fail(e,XIDE_MODEL_INVALID,"启动项目须为可导入的项目。");
 char* next=copy(id); if(!next) return fail(e,XIDE_MODEL_MEMORY,"无法准备启动项目。");
 char* before=p->startup; p->startup=next; if(!refresh(p,e)) { p->startup=before; free(next); return 0; } free(before); return 1;
}
int xideLegacyImportReady(const xide_legacy_import* p) { return ready(p); }
const char* xideLegacyImportWorkspaceJson(const xide_legacy_import* p) { return p ? p->workspaceText : NULL; }
const char* xideLegacyImportProjectJson(const xide_legacy_import* p,size_t i) { return p && i<p->projectCount ? p->projects[i].text : NULL; }
const char* xideLegacyImportReportJson(const xide_legacy_import* p) { return p ? p->reportText : NULL; }
const char* xideLegacyImportSessionHintsJson(const xide_legacy_import* p) { return p ? p->hintsText : NULL; }
static int lockSources(xide_legacy_import* p,HANDLE* handles,xide_model_error* e)
{
 for(size_t i=0;i<p->snapshotCount;++i) {
  source_snapshot* expected=&p->snapshots[i];
  if(expected->missing) { wchar_t* w=native(expected->path); DWORD a=w ? GetFileAttributesW(w) : 0,code=GetLastError(); free(w); if(a!=INVALID_FILE_ATTRIBUTES || (code!=ERROR_FILE_NOT_FOUND && code!=ERROR_PATH_NOT_FOUND)) return fail(e,XIDE_MODEL_CONFLICT,"原缺失项目的状态已变化，请重新准备导入。"); continue; }
  wchar_t* w=native(expected->path); handles[i]=w ? CreateFileW(w,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; free(w);
  if(handles[i]==INVALID_HANDLE_VALUE) return fail(e,XIDE_MODEL_IO,"无法锁定旧清单，请关闭外部写入程序再重试。");
  source_snapshot now={0}; int okay=readHandle(handles[i],&now,e);
  if(okay) okay=sameInfo(&expected->identity,&now.identity) && expected->length==now.length && !memcmp(expected->bytes,now.bytes,now.length);
  clearSnapshot(&now); if(!okay) return fail(e,e && e->code ? e->code : XIDE_MODEL_CONFLICT,"旧清单字节或物理身份已变化，请重新准备并检查报告。");
 }
 return 1;
}
static int writeNew(const char* path,const char* bytes,HANDLE* out)
{
 wchar_t* w=native(path); *out=w ? CreateFileW(w,GENERIC_WRITE|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH|FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; free(w);
 if(*out==INVALID_HANDLE_VALUE) return 0;
 size_t n=strlen(bytes),at=0; while(at<n) { DWORD done=0,want=n-at>65536 ? 65536 : (DWORD)(n-at); if(!WriteFile(*out,bytes+at,want,&done,NULL) || !done) return 0; at+=done; } return FlushFileBuffers(*out)!=0;
}
static int removeClosed(const char* path,const BY_HANDLE_FILE_INFORMATION* original)
{
 wchar_t* w=native(path); HANDLE h=w ? CreateFileW(w,DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; DWORD code=GetLastError(); free(w);
 if(h==INVALID_HANDLE_VALUE) return code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND;
 BY_HANDLE_FILE_INFORMATION now={0}; FILE_DISPOSITION_INFO remove={TRUE};
 int okay=GetFileInformationByHandle(h,&now) && now.dwVolumeSerialNumber==original->dwVolumeSerialNumber && now.nFileIndexHigh==original->nFileIndexHigh && now.nFileIndexLow==original->nFileIndexLow && now.nNumberOfLinks==1 && !(now.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) && SetFileInformationByHandle(h,FileDispositionInfo,&remove,sizeof(remove)); CloseHandle(h); return okay;
}
typedef struct generated { const char *name,*text; HANDLE handle; BY_HANDLE_FILE_INFORMATION identity; int closed; } generated;
int xideLegacyImportCommit(xide_legacy_import* p,int reviewed,xide_model_error* e)
{
 success(e); if(!p || p->committed || !ready(p) || !reviewed) return fail(e,XIDE_MODEL_INVALID,"需选择全部入口/输出类型并确认迁移报告，已提交计划不可再次执行。");
 if(!unused(p->directory,e)) return 0;
 size_t count=3; for(size_t i=0;i<p->projectCount;++i) if(p->projects[i].supported) ++count;
 generated* files=calloc(count,sizeof(*files)); HANDLE* sources=malloc(p->snapshotCount*sizeof(*sources));
 wchar_t *parent=native(p->parent),*target=native(p->directory); size_t targetLength=target ? wcslen(target) : 0;
 size_t renameBytes=offsetof(FILE_RENAME_INFO,FileName)+(targetLength+1)*sizeof(wchar_t); FILE_RENAME_INFO* rename=target ? calloc(1,renameBytes) : NULL;
 if(!files || !sources || !parent || !rename) { free(files); free(sources); free(parent); free(target); free(rename); return fail(e,XIDE_MODEL_MEMORY,"无法准备导入发布事务。"); }
 for(size_t i=0;i<p->snapshotCount;++i) sources[i]=INVALID_HANDLE_VALUE;
 files[0].name="workspace.xlw"; files[0].text=p->workspaceText; files[1].name="report.json"; files[1].text=p->reportText; files[2].name="session-hints.json"; files[2].text=p->hintsText;
 for(size_t i=0,j=3;i<p->projectCount;++i) if(p->projects[i].supported) { files[j].name=p->projects[i].fileName; files[j++].text=p->projects[i].text; }
 for(size_t i=0;i<count;++i) files[i].handle=INVALID_HANDLE_VALUE;
 rename->ReplaceIfExists=FALSE; rename->FileNameLength=(DWORD)(targetLength*sizeof(wchar_t)); memcpy(rename->FileName,target,(targetLength+1)*sizeof(wchar_t));
 HANDLE parentHandle=INVALID_HANDLE_VALUE,stageHandle=INVALID_HANDLE_VALUE; char* stage=NULL; int stageCreated=0;
 const char* phase="复核旧清单"; int okay=lockSources(p,sources,e);
 if(okay) { phase="锁定导入父目录"; parentHandle=CreateFileW(parent,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL); BY_HANDLE_FILE_INFORMATION info={0}; okay=parentHandle!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(parentHandle,&info) && !(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) && (info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY);
  if(okay && !sameIdentity(&info,&p->parentIdentity)) okay=fail(e,XIDE_MODEL_CONFLICT,"导入父目录的物理身份已变化，请重新检查目标。");
 }
 if(okay) {
  phase="创建导入暂存目录"; char* id=xideCreateId(".xide-import"); stage=id ? xidePathResolve(p->parent,id) : NULL; free(id); wchar_t* w=stage ? native(stage) : NULL;
  okay=w && CreateDirectoryW(w,NULL); stageCreated=okay;
  if(okay) stageHandle=CreateFileW(w,DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
  free(w); okay=okay && stageHandle!=INVALID_HANDLE_VALUE;
 }
 for(size_t i=0;okay && i<count;++i) { phase="写入导入文件"; char* path=xidePathResolve(stage,files[i].name); okay=path && writeNew(path,files[i].text,&files[i].handle); free(path); }
 for(size_t i=0;okay && i<count;++i) { phase="完成导入文件"; okay=GetFileInformationByHandle(files[i].handle,&files[i].identity)!=0; if(okay) { CloseHandle(files[i].handle); files[i].handle=INVALID_HANDLE_VALUE; files[i].closed=1; } }
 if(okay) { phase="发布导入目录"; okay=SetFileInformationByHandle(stageHandle,FileRenameInfo,rename,(DWORD)renameBytes)!=0; }
 DWORD failure=okay ? 0 : GetLastError();
 if(okay) p->committed=1;
 else {
  FILE_DISPOSITION_INFO remove={TRUE}; int cleaned=!stageCreated || stageHandle!=INVALID_HANDLE_VALUE;
  for(size_t i=0;i<count;++i) {
   if(files[i].handle!=INVALID_HANDLE_VALUE) { if(!SetFileInformationByHandle(files[i].handle,FileDispositionInfo,&remove,sizeof(remove))) cleaned=0; CloseHandle(files[i].handle); files[i].handle=INVALID_HANDLE_VALUE; }
   else if(files[i].closed) { char* path=xidePathResolve(stage,files[i].name); if(!path || !removeClosed(path,&files[i].identity)) cleaned=0; free(path); }
  }
  if(stageHandle!=INVALID_HANDLE_VALUE && !SetFileInformationByHandle(stageHandle,FileDispositionInfo,&remove,sizeof(remove))) cleaned=0;
  if(e && !e->code) { e->code=XIDE_MODEL_IO; snprintf(e->message,sizeof(e->message),"%s失败（Windows %lu）。原文件未修改。%s%s",phase,failure,cleaned ? "暂存已清理。" : "请检查保留的暂存目录：",cleaned ? "" : stage ? stage : ""); }
 }
 if(stageHandle!=INVALID_HANDLE_VALUE) CloseHandle(stageHandle);
 if(parentHandle!=INVALID_HANDLE_VALUE) CloseHandle(parentHandle);
 for(size_t i=0;i<p->snapshotCount;++i) if(sources[i]!=INVALID_HANDLE_VALUE) CloseHandle(sources[i]);
 free(files); free(sources); free(parent); free(target); free(rename); free(stage); return okay;
}
