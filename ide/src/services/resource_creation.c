#define _WIN32_WINNT 0x0601
#include "services/resource_creation.h"
#include "platform/path.h"
#include <windows.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct xide_resource_plan {
 char *id,*manifest,*path,*parent,*body;
 xvalue* baseline; xide_project next;
 xide_file_snapshot manifestSnapshot; xide_directory_identity parentIdentity;
 int committed;
};
static int fail(xide_model_error* e,int code,const char* s)
{ if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static char* copy(const char* s) { char* p=s ? malloc(strlen(s)+1) : NULL; if(p) strcpy(p,s); return p; }
static wchar_t* native(const char* s)
{
 int n=s ? MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0) : 0;
 wchar_t* w=n>0 && n<32000 ? malloc(((size_t)n+8)*sizeof(*w)) : NULL; if(!w) return NULL;
 MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n);
 for(int i=0;i<n;++i) if(w[i]==L'/') w[i]=L'\\';
 if(!wcsncmp(w,L"\\\\?\\",4)) return w;
 size_t length=wcslen(w); int unc=w[0]==L'\\' && w[1]==L'\\';
 if(unc) { memmove(w+8,w+2,(length-1)*sizeof(*w)); memcpy(w,L"\\\\?\\UNC\\",8*sizeof(*w)); }
 else { memmove(w+4,w,(length+1)*sizeof(*w)); memcpy(w,L"\\\\?\\",4*sizeof(*w)); } return w;
}
char* xideResourceCreationPath(const xide_project* p,const char* path,xide_model_error* e)
{
 if(e) memset(e,0,sizeof(*e));
 if(!p || !p->available || !p->manifest || !path || !*path) { fail(e,XIDE_MODEL_INVALID,"请选择项目并填写新资源配置的文件路径。"); return NULL; }
 /* Validate the submitted leaf before Win32 normalization can remove trailing
    spaces/dots. Retain lexical ancestors so a junction cannot be hidden. */
 if(!xideFileRenameTargetValid(path,e)) return NULL;
 char *base=xidePathDirectory(p->manifest),*absolute=base ? xidePathResolveLexical(base,path) : NULL; free(base);
 if(!absolute) { fail(e,XIDE_MODEL_INVALID,"资源配置路径无效。"); return NULL; }
 if((!xidePathHasExtension(absolute,".json") && !xidePathHasExtension(absolute,".xlon")) || !xideFileRenameTargetValid(absolute,e)) {
  if(e && !e->code) { fail(e,XIDE_MODEL_INVALID,"新资源配置须使用 .json 或 .xlon 扩展名。"); } free(absolute); return NULL;
 }
 return absolute;
}
static int unused(const char* path,xide_model_error* e)
{
 wchar_t* w=native(path); if(!w) return fail(e,XIDE_MODEL_INVALID,"资源路径不是有效 UTF-8 或路径过长。");
 DWORD a=GetFileAttributesW(w),code=GetLastError(); free(w);
 if(a!=INVALID_FILE_ATTRIBUTES) return fail(e,XIDE_MODEL_CONFLICT,"目标已存在，请使用新名称。资源创建不会覆盖任何文件或目录。");
 return code==ERROR_FILE_NOT_FOUND ? 1 : fail(e,XIDE_MODEL_IO,"目标父目录不存在或无法检查，请选择已有普通目录。");
}
void xideResourcePlanDestroy(xide_resource_plan* p)
{ if(p) { free(p->id); free(p->manifest); free(p->path); free(p->parent); free(p->body); xrtValueRelease(p->baseline); xideProjectClear(&p->next); free(p); } }
const char* xideResourcePlanPath(const xide_resource_plan* p) { return p ? p->path : ""; }
xide_resource_plan* xideResourcePlanPrepare(const xide_project* project,const char* path,const char* body,const xide_file_snapshot* expected,xide_model_error* e)
{
 if(e) memset(e,0,sizeof(*e));
 if(!project || !project->id || !body || strlen(body)>16u*1024u*1024u) { fail(e,XIDE_MODEL_INVALID,"资源创建请求无效或超过 16 MiB。"); return NULL; }
 xide_resource_plan* p=calloc(1,sizeof(*p)); if(!p) { fail(e,XIDE_MODEL_MEMORY,"无法准备资源创建。"); return NULL; }
 p->path=xideResourceCreationPath(project,path,e); p->id=copy(project->id); p->manifest=copy(project->manifest); p->body=copy(body); p->parent=p->path ? xidePathDirectory(p->path) : NULL;
 int okay=p->path && p->id && p->manifest && p->body && p->parent;
 for(size_t i=0;okay && i<project->files.count;++i) if(xidePathEqual(project->files.items[i].path,p->path)) okay=fail(e,XIDE_MODEL_CONFLICT,"此路径已经是项目文件引用，请在引用表单调整其类型或选择新路径。");
 if(okay) okay=unused(p->path,e) && xideDirectoryIdentityRead(p->parent,&p->parentIdentity,e) && xideFileSnapshotInspect(p->manifest,&p->manifestSnapshot,e);
 if(okay && expected && !xideFileSnapshotEqual(expected,&p->manifestSnapshot)) okay=fail(e,XIDE_MODEL_CONFLICT,"项目清单在磁盘已变化，请重新打开资源创建表单。");
 xide_resource_config config={0}; if(okay) okay=xideResourceParse(&config,body,e) && xideResourceCheckAssets(config.root,p->path,e); xideResourceClear(&config);
 if(okay) { p->baseline=xideProjectExport(project,e); okay=p->baseline && xideProjectDecode(&p->next,project->manifest,p->baseline,e) && xideProjectAddFile(&p->next,p->path,"resource-config",e); }
 if(!okay) { if(e && !e->code) fail(e,XIDE_MODEL_MEMORY,"无法为资源创建准备文件与引用。"); xideResourcePlanDestroy(p); return NULL; } return p;
}
/* Hold every directory component without delete sharing while using full paths.
   This also rejects junctions/symlinks in any ancestor, not just the immediate parent. */
static int pinParents(const char* path,HANDLE** out,size_t* count,xide_model_error* e)
{
 wchar_t* w=native(path); if(!w) return fail(e,XIDE_MODEL_INVALID,"父目录路径无效。");
 size_t n=wcslen(w),start=7;
 if(!wcsncmp(w,L"\\\\?\\UNC\\",8)) { wchar_t* server=wcschr(w+8,L'\\'); wchar_t* share=server ? wcschr(server+1,L'\\') : NULL; start=share ? (size_t)(share-w)+1 : n; }
 HANDLE* handles=malloc((n+1)*sizeof(*handles)); if(!handles) { free(w); return fail(e,XIDE_MODEL_MEMORY,"无法锁定资源目录。"); }
 size_t used=0; int okay=1;
 for(size_t i=start;okay && i<=n;++i) if(i==n || w[i]==L'\\') {
  wchar_t saved=w[i]; w[i]=0;
  HANDLE h=CreateFileW(w,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL); BY_HANDLE_FILE_INFORMATION info={0}; w[i]=saved;
  okay=h!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(h,&info) && (info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && !(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT);
  if(okay) handles[used++]=h; else if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
 }
 free(w); if(!okay) { for(size_t i=0;i<used;++i) CloseHandle(handles[i]); free(handles); return fail(e,XIDE_MODEL_IO,"无法锁定普通父目录；请检查共享、权限及目录重解析点。"); }
 *out=handles; *count=used; return 1;
}
int xideResourcePlanCommit(xide_project* project,xide_resource_plan* p,xide_model_error* e)
{
 if(e) memset(e,0,sizeof(*e));
 if(!project || !project->available || !p || p->committed || !project->id || !project->manifest || strcmp(project->id,p->id) || strcmp(project->manifest,p->manifest)) return fail(e,XIDE_MODEL_INVALID,"资源创建计划已提交或项目身份已变化。");
 xvalue* now=xideProjectExport(project,e); int same=now && xrtValueEqual(now,p->baseline); xrtValueRelease(now);
 if(!same) return fail(e,XIDE_MODEL_CONFLICT,"项目在预览后已变化；请重新准备资源创建。");
 if(!unused(p->path,e)) return 0;
 char* id=xideCreateId(".xide-resource"); char* stage=id ? xidePathResolve(p->parent,id) : NULL; free(id);
 wchar_t *stageName=stage ? native(stage) : NULL,*target=native(p->path),*manifest=native(p->manifest);
 size_t n=target ? wcslen(target) : 0,bytes=offsetof(FILE_RENAME_INFO,FileName)+(n+1)*sizeof(wchar_t);
 FILE_RENAME_INFO* rename=target ? calloc(1,bytes) : NULL;
 if(!stageName || !target || !manifest || !rename) { free(stage); free(stageName); free(target); free(manifest); free(rename); return fail(e,XIDE_MODEL_MEMORY,"无法准备资源文件事务。"); }
 rename->ReplaceIfExists=FALSE; rename->FileNameLength=(DWORD)(n*sizeof(wchar_t)); memcpy(rename->FileName,target,(n+1)*sizeof(wchar_t));
 HANDLE *parents=NULL,manifestHandle=INVALID_HANDLE_VALUE,file=INVALID_HANDLE_VALUE; size_t count=0; int okay=pinParents(p->parent,&parents,&count,e);
 xide_directory_identity parent={0}; xide_file_snapshot disk={0};
 if(okay) okay=xideDirectoryIdentityRead(p->parent,&parent,e) && parent.index==p->parentIdentity.index && parent.volume==p->parentIdentity.volume;
 if(okay) { manifestHandle=CreateFileW(manifest,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL); okay=manifestHandle!=INVALID_HANDLE_VALUE && xideFileSnapshotInspect(p->manifest,&disk,e) && xideFileSnapshotEqual(&disk,&p->manifestSnapshot); }
 if(!okay && e && !e->code) fail(e,XIDE_MODEL_CONFLICT,"项目清单或资源父目录在预览后已变化，当前输入已保留。");
 xide_resource_config config={0}; if(okay) okay=xideResourceParse(&config,p->body,e) && xideResourceCheckAssets(config.root,p->path,e); xideResourceClear(&config);
 const char* phase="创建暂存文件";
 if(okay) { file=CreateFileW(stageName,GENERIC_WRITE|DELETE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH|FILE_FLAG_OPEN_REPARSE_POINT,NULL); okay=file!=INVALID_HANDLE_VALUE; }
 size_t length=strlen(p->body),at=0;
 if(okay) { phase="写入资源配置"; while(okay && at<length) { DWORD done=0,want=length-at>65536 ? 65536 : (DWORD)(length-at); okay=WriteFile(file,p->body+at,want,&done,NULL) && done; at+=done; } }
 if(okay) { phase="刷新资源配置"; okay=FlushFileBuffers(file)!=0; }
 if(okay) { phase="发布新资源配置"; okay=SetFileInformationByHandle(file,FileRenameInfo,rename,(DWORD)bytes)!=0; }
 DWORD os=okay ? 0 : GetLastError();
 if(okay) {
  /* No allocation or fallible model operations after publication. */
  xide_project old={0}; old.files=project->files; old.cSources=project->cSources; old.resources=project->resources;
  project->files=p->next.files; memset(&p->next.files,0,sizeof(p->next.files));
  project->cSources=p->next.cSources; memset(&p->next.cSources,0,sizeof(p->next.cSources));
  project->resources=p->next.resources; memset(&p->next.resources,0,sizeof(p->next.resources)); project->dirty=1; p->committed=1; xideProjectClear(&old);
 } else if(file!=INVALID_HANDLE_VALUE) {
  FILE_DISPOSITION_INFO remove={TRUE}; int cleaned=SetFileInformationByHandle(file,FileDispositionInfo,&remove,sizeof(remove))!=0;
  if(e) { e->code=XIDE_MODEL_IO; snprintf(e->message,sizeof(e->message),cleaned ? "%s失败（Windows %lu），暂存文件已清理，项目未修改。" : "%s失败（Windows %lu），项目未修改；暂存文件无法清理：%s",phase,os,stage); }
 } else if(e && !e->code) { e->code=XIDE_MODEL_IO; snprintf(e->message,sizeof(e->message),"%s失败（Windows %lu），项目未修改。",phase,os); }
 if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);
 if(manifestHandle!=INVALID_HANDLE_VALUE) CloseHandle(manifestHandle);
 for(size_t i=0;i<count;++i) { CloseHandle(parents[i]); } free(parents); free(stage); free(stageName); free(target); free(manifest); free(rename); return okay;
}
