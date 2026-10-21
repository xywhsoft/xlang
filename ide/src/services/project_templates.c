#define _WIN32_WINNT 0x0601
#include "services/project_templates.h"
#include "platform/path.h"
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define README_APP "# xlang 工程\n\n使用 IDE 的项目菜单调整文件与构建配置。选择项目后按 F7 构建，设为启动项目后按 F5 构建并运行。\n\nmain.xl 是入口；生成的 build/ 目录不进入版本管理。\n"
static const xide_template_file blankFiles[]={
 {"main.xl","xlang","// 在这里开始编写 xlang 应用。\nfunc main(...) as int\n    return 0\nend\n"},
 {".gitignore","text","build/\n"},{"README.md","text",README_APP}
};
static const xide_template_file consoleFiles[]={
 {"main.xl","xlang","func main(...) as int\n    print(\"Hello, xlang!\")\n    return 0\nend\n"},
 {".gitignore","text","build/\n"},{"README.md","text",README_APP}
};
static const xide_template_file mixedFiles[]={
 {"main.xl","xlang","// c_int 对应 C 的 int，保持边界声明一致。\nextern \"native_answer\" func nativeAnswer() as c_int\n\nfunc main(...) as int\n    if nativeAnswer() != 42 then\n        return 1\n    end\n    print(\"C_XLANG_OK\")\n    return 0\nend\n"},
 {"native.c","c","#include \"native.h\"\n\nint native_answer(void)\n{\n    return 42;\n}\n"},
 {"native.h","header","#ifndef XLANG_NATIVE_H\n#define XLANG_NATIVE_H\n#ifdef __cplusplus\nextern \"C\" {\n#endif\nint native_answer(void);\n#ifdef __cplusplus\n}\n#endif\n#endif\n"},
 {".gitignore","text","build/\n"},{"README.md","text",README_APP "\n本模板由 main.xl 调用 native.c 的 native_answer；native.h 与 xlang extern 声明使用相同的 C int ABI。\n"}
};
static const xide_template_file dllFiles[]={
 {"main.xl","xlang","// 纯导出模块，由宿主加载；无需应用 main。\nexport \"xlang_add\" func add(c_int left, c_int right) as c_int\n    return left + right\nend\n"},
 {".gitignore","text","build/\n"},{"README.md","text","# xlang 动态库\n\n使用 IDE 的 F7 构建 DLL，由 C 或其他宿主加载。导出 xlang_add，C 声明为 int xlang_add(int left, int right)。\n\n动态库没有独立运行入口；构建目录为 build/。\n"}
};
static const xide_project_template definitions[]={
 {"blank","空白 xlang 应用","生成最小 main 入口与开发配置，适合从头编写应用。","exe",blankFiles,sizeof(blankFiles)/sizeof(*blankFiles)},
 {"console","xlang 控制台应用","生成可直接运行的 Hello, xlang! 示例。","exe",consoleFiles,sizeof(consoleFiles)/sizeof(*consoleFiles)},
 {"mixed-c","C / xlang 混合工程","生成调用 C 函数的 xlang 入口、C 源文件和头文件，使用一致的 c_int ABI。","exe",mixedFiles,sizeof(mixedFiles)/sizeof(*mixedFiles)},
 {"dll","xlang 动态库","生成可由宿主调用的 C ABI 导出函数和 DLL 构建配置。","dll",dllFiles,sizeof(dllFiles)/sizeof(*dllFiles)}
};
struct xide_template_plan { char *parent,*directory,*manifestText; xide_project project; const xide_project_template* definition; int committed; };
static int fail(xide_model_error* e,int code,const char* s) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static int put(xvalue* v,const char* key,const char* s) { return xrtValueObjectSetNew(v,sv(key),xrtValueString(sv(s))); }
static wchar_t* wide(const char* s)
{
 int n=s ? MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0) : 0; wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL;
 if(w && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n)) { free(w); w=NULL; } return w;
}
/* Canonical absolute paths use an extended Win32 prefix for Unicode/long paths. */
static wchar_t* native(const char* s)
{
 wchar_t* w=wide(s); if(!w) return NULL; size_t n=wcslen(w); if(n>32000) { free(w); return NULL; }
 for(size_t i=0;i<n;++i) if(w[i]==L'/') w[i]=L'\\';
 if(!wcsncmp(w,L"\\\\?\\",4)) return w;
 int unc=w[0]==L'\\' && w[1]==L'\\'; const wchar_t* prefix=unc ? L"\\\\?\\UNC\\" : L"\\\\?\\";
 size_t a=wcslen(prefix),b=unc ? 2 : 0; wchar_t* result=malloc((a+n-b+1)*sizeof(*result));
 if(result) { memcpy(result,prefix,a*sizeof(*result)); memcpy(result+a,w+b,(n-b+1)*sizeof(*result)); } free(w); return result;
}
static int validName(const char* name,xide_model_error* e)
{
 wchar_t* w=wide(name); if(!w) return fail(e,XIDE_MODEL_INVALID,"项目名称须为有效 UTF-8。适用于新目录名称。");
 size_t n=wcslen(w); int okay=n>0 && n<=120 && w[0]!=L' ' && w[n-1]!=L' ' && w[n-1]!=L'.';
 for(size_t i=0;okay && i<n;++i) if(w[i]<32 || wcschr(L"<>:\"/\\|?*",w[i])) okay=0;
 wchar_t base[121]={0}; size_t b=0; while(b<n && w[b]!=L'.') { base[b]=w[b]; ++b; }
 const wchar_t* reserved[]={L"CON",L"PRN",L"AUX",L"NUL",L"CONIN$",L"CONOUT$"};
 for(size_t i=0;i<sizeof(reserved)/sizeof(*reserved);++i) if(CompareStringOrdinal(base,-1,reserved[i],-1,TRUE)==CSTR_EQUAL) okay=0;
 if(b==4 && (CompareStringOrdinal(base,3,L"COM",3,TRUE)==CSTR_EQUAL || CompareStringOrdinal(base,3,L"LPT",3,TRUE)==CSTR_EQUAL) && ((base[3]>=L'1' && base[3]<=L'9') || base[3]==L'\x00b9' || base[3]==L'\x00b2' || base[3]==L'\x00b3')) okay=0;
 free(w); return okay ? 1 : fail(e,XIDE_MODEL_INVALID,"名称用作新目录：须为 1–120 个 UTF-16 单位，无路径分隔符、保留设备名、首尾空格或末尾句点。");
}
static int unusedDirectory(const char* path,xide_model_error* e)
{
 wchar_t* w=native(path); if(!w) return fail(e,XIDE_MODEL_INVALID,"项目路径无效或过长。");
 DWORD a=GetFileAttributesW(w),code=GetLastError(); free(w);
 if(a!=INVALID_FILE_ATTRIBUTES) return fail(e,XIDE_MODEL_CONFLICT,"项目目录已存在。请选择新名称或其他位置；现有内容不会覆盖。");
 return code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND ? 1 : fail(e,XIDE_MODEL_IO,"无法确认项目目录是否存在，请检查位置与访问权限。");
}
size_t xideProjectTemplateCount(void) { return sizeof(definitions)/sizeof(*definitions); }
const xide_project_template* xideProjectTemplateAt(size_t i) { return i<xideProjectTemplateCount() ? &definitions[i] : NULL; }
const xide_project_template* xideProjectTemplateFind(const char* id) { if(id) for(size_t i=0;i<xideProjectTemplateCount();++i) if(!strcmp(id,definitions[i].id)) return &definitions[i]; return NULL; }
void xideTemplatePlanDestroy(xide_template_plan* p) { if(p) { free(p->parent); free(p->directory); xrtFree(p->manifestText); xideProjectClear(&p->project); free(p); } }
const char* xideTemplatePlanDirectory(const xide_template_plan* p) { return p ? p->directory : ""; }
const char* xideTemplatePlanManifest(const xide_template_plan* p) { return p ? p->manifestText : ""; }
const xide_project* xideTemplatePlanProject(const xide_template_plan* p) { return p ? &p->project : NULL; }
const xide_project_template* xideTemplatePlanDefinition(const xide_template_plan* p) { return p ? p->definition : NULL; }
xide_template_plan* xideTemplatePlanPrepare(const char* parent,const char* name,const char* templateId,xide_model_error* e)
{
 if(e) { memset(e,0,sizeof(*e)); } const xide_project_template* d=xideProjectTemplateFind(templateId);
 if(!d || !parent || !*parent || !validName(name,e)) { if(e && !e->code) fail(e,XIDE_MODEL_INVALID,"选择模板并填写父目录与项目名称。"); return NULL; }
 xide_template_plan* p=calloc(1,sizeof(*p)); if(!p) { fail(e,XIDE_MODEL_MEMORY,"无法准备新项目。"); return NULL; } p->definition=d;
 p->parent=xidePathResolve(NULL,parent); wchar_t* w=p->parent ? native(p->parent) : NULL;
 DWORD a=w ? GetFileAttributesW(w) : INVALID_FILE_ATTRIBUTES; free(w);
 int okay=a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_DIRECTORY); if(!okay) fail(e,XIDE_MODEL_IO,"父目录不存在或无法访问，请先选择已有目录。");
 if(okay) { p->directory=xidePathResolve(p->parent,name); okay=p->directory && unusedDirectory(p->directory,e); }
 char* manifest=okay ? xidePathResolve(p->directory,"project.xlp") : NULL;
 if(okay) okay=manifest && xideProjectCreate(&p->project,manifest,name,"main.xl",e) && xideProjectSetMetadata(&p->project,name,d->description,d->kind,e);
 for(size_t i=1;okay && i<d->fileCount;++i) okay=xideProjectAddFile(&p->project,d->files[i].path,d->files[i].kind,e);
 if(okay) {
  xvalue* configs=xideProjectConfigurationsJson(&p->project); xvalue* first=configs ? xrtValueArrayGet(configs,0) : NULL;
  okay=first && put(first,"output",!strcmp(d->kind,"dll") ? "build/development/app.dll" : "build/development/app.exe") && xideProjectApplyConfigurations(&p->project,configs,"development",e); xrtValueRelease(configs);
 }
 if(okay) { p->project.id=xideCreateId("project"); p->project.referenceSource=xrtValueObject(); okay=p->project.id && p->project.referenceSource; }
 xvalue* json=okay ? xideProjectExport(&p->project,e) : NULL; size_t length=0;
 if(json) { p->manifestText=xrtJsonStringify(json,true,&length); okay=p->manifestText!=NULL; xrtValueRelease(p->project.source); p->project.source=json; json=NULL; p->project.dirty=0; }
 else okay=0;
 free(manifest); xrtValueRelease(json);
 if(!okay) { xideTemplatePlanDestroy(p); if(e && !e->code) fail(e,XIDE_MODEL_MEMORY,"无法准备项目清单。"); return NULL; } return p;
}
static int writeNew(const char* path,const char* text,HANDLE* out)
{
 wchar_t* w=native(path); HANDLE h=w ? CreateFileW(w,GENERIC_WRITE|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH|FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; free(w);
 if(h==INVALID_HANDLE_VALUE) { return 0; }
 *out=h; size_t n=strlen(text),at=0;
 while(at<n) { DWORD done=0,want=n-at>65536 ? 65536 : (DWORD)(n-at); if(!WriteFile(h,text+at,want,&done,NULL) || !done) return 0; at+=done; } return FlushFileBuffers(h)!=0;
}
/* Directory rename requires closed child files. Rollback reopens only the
   generated names and verifies their original physical identity before deletion. */
static int removeClosed(const char* path,const BY_HANDLE_FILE_INFORMATION* original)
{
 wchar_t* w=native(path); HANDLE h=w ? CreateFileW(w,DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE;
 DWORD code=GetLastError(); free(w); if(h==INVALID_HANDLE_VALUE) return code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND;
 BY_HANDLE_FILE_INFORMATION now={0}; FILE_DISPOSITION_INFO remove={TRUE};
 int okay=GetFileInformationByHandle(h,&now) && now.dwVolumeSerialNumber==original->dwVolumeSerialNumber && now.nFileIndexHigh==original->nFileIndexHigh && now.nFileIndexLow==original->nFileIndexLow && now.nNumberOfLinks==1 && !(now.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) && SetFileInformationByHandle(h,FileDispositionInfo,&remove,sizeof(remove)); CloseHandle(h); return okay;
}
int xideTemplatePlanCommit(xide_workspace* workspace,xide_template_plan* p,int startup,char** createdId,xide_model_error* e)
{
 if(e) { memset(e,0,sizeof(*e)); } if(createdId) *createdId=NULL;
 if(!workspace || !workspace->source || !p || p->committed || !p->project.id || p->definition->fileCount>=8 || workspace->projectCount>=SIZE_MAX/sizeof(xide_project)-1) return fail(e,XIDE_MODEL_INVALID,"新建项目计划或目标工作空间无效。");
 if(!unusedDirectory(p->directory,e)) return 0;
 if(xideWorkspaceFindProject(workspace,p->project.id)) return fail(e,XIDE_MODEL_CONFLICT,"项目身份冲突，请重新打开向导。");
 for(size_t i=0;i<workspace->projectCount;++i) if(xidePathEqual(workspace->projects[i].manifest,p->project.manifest)) return fail(e,XIDE_MODEL_CONFLICT,"此项目路径已被工作空间引用。");
 size_t count=workspace->projectCount+1; xide_project* projects=malloc(count*sizeof(*projects));
 char* chosen=startup ? copy(p->project.id) : NULL; char* result=createdId ? copy(p->project.id) : NULL;
 wchar_t* parent=native(p->parent),*target=native(p->directory); size_t targetLength=target ? wcslen(target) : 0;
 size_t renameBytes=offsetof(FILE_RENAME_INFO,FileName)+(targetLength+1)*sizeof(wchar_t); FILE_RENAME_INFO* rename=target ? calloc(1,renameBytes) : NULL;
 if(!projects || (startup && !chosen) || (createdId && !result) || !parent || !rename) { free(projects); free(chosen); free(result); free(parent); free(target); free(rename); return fail(e,XIDE_MODEL_MEMORY,"无法为新项目准备事务。"); }
 rename->ReplaceIfExists=FALSE; rename->FileNameLength=(DWORD)(targetLength*sizeof(wchar_t)); memcpy(rename->FileName,target,(targetLength+1)*sizeof(wchar_t));
 HANDLE parentHandle=CreateFileW(parent,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
 HANDLE stageHandle=INVALID_HANDLE_VALUE,files[8]; BY_HANDLE_FILE_INFORMATION identities[8]={{0}}; int closed[8]={0},stageCreated=0; for(size_t i=0;i<8;++i) files[i]=INVALID_HANDLE_VALUE;
 char *stage=NULL; const char* phase="锁定父目录"; int okay=parentHandle!=INVALID_HANDLE_VALUE;
 if(okay) { phase="创建暂存目录"; char* id=xideCreateId(".xide-new"); stage=id ? xidePathResolve(p->parent,id) : NULL; free(id); wchar_t* w=stage ? native(stage) : NULL; okay=w && CreateDirectoryW(w,NULL); stageCreated=okay; if(okay) stageHandle=CreateFileW(w,DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL); free(w); okay=okay && stageHandle!=INVALID_HANDLE_VALUE; }
 for(size_t i=0;okay && i<p->definition->fileCount;++i) { phase="写入生成文件"; char* path=xidePathResolve(stage,p->definition->files[i].path); okay=path && writeNew(path,p->definition->files[i].content,&files[i]); free(path); }
 if(okay) { phase="写入项目清单"; char* path=xidePathResolve(stage,"project.xlp"); okay=path && writeNew(path,p->manifestText,&files[p->definition->fileCount]); free(path); }
 if(okay) {
  phase="完成生成文件";
  for(size_t i=0;okay && i<=p->definition->fileCount;++i) {
   okay=GetFileInformationByHandle(files[i],&identities[i])!=0;
   if(okay) { CloseHandle(files[i]); files[i]=INVALID_HANDLE_VALUE; closed[i]=1; }
  }
 }
 if(okay) { phase="发布新目录"; okay=SetFileInformationByHandle(stageHandle,FileRenameInfo,rename,(DWORD)renameBytes)!=0; }
 DWORD failure=okay ? 0 : GetLastError();
 if(okay) {
  if(workspace->projectCount) { memcpy(projects,workspace->projects,workspace->projectCount*sizeof(*projects)); } projects[count-1]=p->project; memset(&p->project,0,sizeof(p->project));
  free(workspace->projects); workspace->projects=projects; workspace->projectCount=workspace->projectCapacity=count; projects=NULL; workspace->dirty=1;
  if(startup) { free(workspace->startupProject); workspace->startupProject=chosen; chosen=NULL; }
  if(createdId) { *createdId=result; result=NULL; } p->committed=1;
 } else {
  FILE_DISPOSITION_INFO remove={TRUE}; int cleaned=!stageCreated || stageHandle!=INVALID_HANDLE_VALUE;
  for(size_t i=0;i<8;++i) {
   if(files[i]!=INVALID_HANDLE_VALUE) { if(!SetFileInformationByHandle(files[i],FileDispositionInfo,&remove,sizeof(remove))) cleaned=0; CloseHandle(files[i]); files[i]=INVALID_HANDLE_VALUE; }
   else if(closed[i]) { const char* name=i<p->definition->fileCount ? p->definition->files[i].path : "project.xlp"; char* path=xidePathResolve(stage,name); if(!path || !removeClosed(path,&identities[i])) cleaned=0; free(path); }
  }
  if(stageHandle!=INVALID_HANDLE_VALUE && !SetFileInformationByHandle(stageHandle,FileDispositionInfo,&remove,sizeof(remove))) cleaned=0;
  if(e) { e->code=XIDE_MODEL_IO; if(cleaned) snprintf(e->message,sizeof(e->message),"%s失败（Windows %lu）。目标未覆盖，工作空间未修改。请检查位置和访问权限。",phase,failure); else snprintf(e->message,sizeof(e->message),"%s失败（Windows %lu）；工作空间未修改。无法清理暂存目录：%s",phase,failure,stage ? stage : ""); }
 }
 for(size_t i=0;i<8;++i) if(files[i]!=INVALID_HANDLE_VALUE) CloseHandle(files[i]);
 if(stageHandle!=INVALID_HANDLE_VALUE) { CloseHandle(stageHandle); }
 if(parentHandle!=INVALID_HANDLE_VALUE) { CloseHandle(parentHandle); }
 free(stage); free(projects); free(chosen); free(result); free(parent); free(target); free(rename); return okay;
}
