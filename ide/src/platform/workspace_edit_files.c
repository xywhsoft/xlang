#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include "platform/workspace_edit_files.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define FILE_BYTES (8u*1024u*1024u)
typedef struct pinned_directory {wchar_t* path;HANDLE handle;BY_HANDLE_FILE_INFORMATION identity;} pinned_directory;
typedef struct pinned_file {wchar_t* path;HANDLE handle;size_t document;xide_file_snapshot snapshot;} pinned_file;
struct xide_workspace_edit_files {xide_workspace_edit* plan;xarray directories,files;};
static int fail(xide_model_error* e,int code,const char* s){if(e){e->code=code;snprintf(e->message,sizeof(e->message),"%s",s);}return 0;}
static int stopped(int (*fn)(void*),void* user,xide_model_error* e){if(!fn || !fn(user))return 0;fail(e,XIDE_MODEL_CONFLICT,"Workspace edit file validation cancelled.");return 1;}
static int sameFile(const BY_HANDLE_FILE_INFORMATION* a,const BY_HANDLE_FILE_INFORMATION* b)
{return a->dwVolumeSerialNumber==b->dwVolumeSerialNumber && a->nFileIndexHigh==b->nFileIndexHigh && a->nFileIndexLow==b->nFileIndexLow;}
static int directoryOrder(const void* a,const void* b){return xidePathNativeKeyCompare(((const pinned_directory*)a)->path,((const pinned_directory*)b)->path);}
static int validPath(const wchar_t* p)
{
 size_t n=wcslen(p);if(n<8 || wcsncmp(p,L"\\\\?\\",4) || !((p[4]>=L'A' && p[4]<=L'Z') || (p[4]>=L'a' && p[4]<=L'z')) || p[5]!=L':' || p[6]!=L'\\')return 0;
 wchar_t root[]={p[4],L':',L'\\',0};if(GetDriveTypeW(root)!=DRIVE_FIXED)return 0;
 size_t start=7;for(size_t i=7;i<=n;++i){if(!p[i] || p[i]==L'\\'){if(i==start || p[i-1]==L'.' || p[i-1]==L' ')return 0;start=i+1;}else if(p[i]<32 || wcschr(L"<>:\"/|?*",p[i]))return 0;}return 1;
}
static int ordinary(HANDLE h,int directory,BY_HANDLE_FILE_INFORMATION* info)
{return h!=INVALID_HANDLE_VALUE && GetFileType(h)==FILE_TYPE_DISK && GetFileInformationByHandle(h,info) && !(info->dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) && !!(info->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==directory && (directory || (!(info->dwFileAttributes&FILE_ATTRIBUTE_READONLY) && info->nNumberOfLinks==1));}
void xideWorkspaceEditFilesDestroy(xide_workspace_edit_files* p)
{
 if(!p)return;
 for(size_t i=0;i<p->files.Count;++i){pinned_file* f=xrtArrayGet(&p->files,i);if(f->handle!=INVALID_HANDLE_VALUE)CloseHandle(f->handle);free(f->path);}
 for(size_t i=p->directories.Count;i;--i){pinned_directory* d=xrtArrayGet(&p->directories,i-1);if(d->handle!=INVALID_HANDLE_VALUE)CloseHandle(d->handle);xrtFree(d->path);}
 xrtArrayUnit(&p->files);xrtArrayUnit(&p->directories);xideWorkspaceEditRelease(p->plan);xrtFree(p);
}
size_t xideWorkspaceEditFilesCount(const xide_workspace_edit_files* p){return p?p->files.Count:0;}
int xideWorkspaceEditFilesAt(const xide_workspace_edit_files* p,size_t at,size_t* document,xide_file_snapshot* out)
{if(document)*document=SIZE_MAX;if(out)memset(out,0,sizeof(*out));const pinned_file* f=p?xrtArrayConstGet(&p->files,at):NULL;if(!f || !document || !out)return 0;*document=f->document;*out=f->snapshot;return 1;}
static int readBody(HANDLE file,const xide_workspace_edit_document* document,int (*cancelled)(void*),void* user,xide_model_error* e)
{
 size_t size=document->proof.rawSize;unsigned char* raw=xrtCalloc(size+1,1);if(!raw)return fail(e,XIDE_MODEL_MEMORY,"Cannot allocate closed source validation buffer.");
 LARGE_INTEGER zero={0};int okay=SetFilePointerEx(file,zero,NULL,FILE_BEGIN)!=0;size_t at=0;
 while(okay && at<size){if(stopped(cancelled,user,e)){okay=0;break;}DWORD got=0,want=size-at>65536?65536:(DWORD)(size-at);okay=ReadFile(file,raw+at,want,&got,NULL) && got==want;at+=got;}
 xide_text_file decoded={0};if(okay)okay=xideTextFileDecode(raw,size,&decoded,e) && strlen(decoded.text)==xlsTextSize(document->before) && !memcmp(decoded.text,xlsTextData(document->before),xlsTextSize(document->before));
 xideTextFileClear(&decoded);xrtFree(raw);return okay?1:fail(e,e && e->code?e->code:XIDE_MODEL_CONFLICT,"Closed source does not decode to the captured workspace edit text.");
}
xide_workspace_edit_files* xideWorkspaceEditFilesPin(const xide_workspace_edit* plan,int (*cancelled)(void*),void* user,xide_model_error* e)
{
 if(e){memset(e,0,sizeof(*e));}xide_workspace_edit_info info;if(!xideWorkspaceEditInfo(plan,&info)){fail(e,XIDE_MODEL_INVALID,"Missing prepared workspace edit.");return NULL;}
 xide_workspace_edit_files* p=xrtCalloc(1,sizeof(*p));if(!p){fail(e,XIDE_MODEL_MEMORY,"Cannot allocate workspace edit file gate.");return NULL;}
 p->plan=xideWorkspaceEditRetain((xide_workspace_edit*)plan);size_t total=0,pathBytes=0;int okay=xrtArrayInit(&p->directories,sizeof(pinned_directory)) && xrtArrayInit(&p->files,sizeof(pinned_file));
 for(size_t i=0;okay && i<info.documents;++i){
  if(stopped(cancelled,user,e)){okay=0;break;}xide_workspace_edit_document doc;okay=xideWorkspaceEditDocumentAt(plan,i,&doc);if(!okay)break;if(doc.hasVersion)continue;
  if(!doc.proof.present || doc.proof.overlay || doc.proof.rawSize>FILE_BYTES || doc.proof.rawSize>64u*1024u*1024u-total){okay=fail(e,XIDE_MODEL_INVALID,"Closed source requires a bounded validated disk proof.");break;}total+=doc.proof.rawSize;
  pinned_file file={.path=xidePathNativeWide(doc.path),.handle=INVALID_HANDLE_VALUE,.document=i};
  if(!file.path || !validPath(file.path)){free(file.path);okay=fail(e,XIDE_MODEL_INVALID,"Workspace edit requires a local fixed-drive path without streams or ambiguous components.");break;}
  if(p->files.Count>=512 || !xrtArrayPush(&p->files,&file)){free(file.path);okay=fail(e,XIDE_MODEL_MEMORY,"Cannot retain closed source path.");break;}
  size_t n=wcslen(file.path);
  for(size_t j=6;okay && j<n;++j)if(file.path[j]==L'\\'){
   if(stopped(cancelled,user,e)){okay=0;break;}size_t length=j==6?7:j,bytes=(length+1)*sizeof(wchar_t);
   if(p->directories.Count>=32768 || bytes>8u*1024u*1024u-pathBytes){okay=fail(e,XIDE_MODEL_INVALID,"Workspace edit ancestor storage limit exceeded.");break;}
   pinned_directory d={.path=xrtCalloc(length+1,sizeof(wchar_t)),.handle=INVALID_HANDLE_VALUE};if(d.path)memcpy(d.path,file.path,length*sizeof(wchar_t));
   if(!d.path || !xrtArrayPush(&p->directories,&d)){xrtFree(d.path);okay=fail(e,XIDE_MODEL_MEMORY,"Cannot retain workspace edit ancestor.");break;}pathBytes+=bytes;
  }
 }
 if(okay && p->directories.Count){
  qsort(p->directories.Data,p->directories.Count,sizeof(pinned_directory),directoryOrder);size_t unique=0;
  for(size_t i=0;i<p->directories.Count;++i){pinned_directory* d=xrtArrayGet(&p->directories,i);if(unique && !directoryOrder(xrtArrayGet(&p->directories,unique-1),d)){xrtFree(d->path);continue;}if(unique!=i)*(pinned_directory*)xrtArrayGet(&p->directories,unique)=*d;++unique;}
  p->directories.Count=unique;if(unique>4096)okay=fail(e,XIDE_MODEL_INVALID,"Workspace edit ancestor handle limit exceeded.");
 }
 for(size_t i=0;okay && i<p->directories.Count;++i){
  if(stopped(cancelled,user,e)){okay=0;break;}pinned_directory* d=xrtArrayGet(&p->directories,i);d->handle=CreateFileW(d->path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
  if(!ordinary(d->handle,1,&d->identity))okay=fail(e,XIDE_MODEL_CONFLICT,"Cannot pin an ordinary workspace edit ancestor.");
 }
 for(size_t i=0;okay && i<p->files.Count;++i){
  if(stopped(cancelled,user,e)){okay=0;break;}pinned_file* f=xrtArrayGet(&p->files,i);f->handle=CreateFileW(f->path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);BY_HANDLE_FILE_INFORMATION identity={0};
  if(!ordinary(f->handle,0,&identity)){okay=fail(e,XIDE_MODEL_CONFLICT,"Cannot pin a writable single-link workspace edit source.");break;}
  xide_workspace_edit_document doc;xideWorkspaceEditDocumentAt(plan,f->document,&doc);
  if((((uint64_t)identity.nFileSizeHigh<<32)|identity.nFileSizeLow)!=doc.proof.rawSize){okay=fail(e,XIDE_MODEL_CONFLICT,"Closed source raw size no longer matches.");break;}
  okay=xideFileSnapshotInspectHandle(f->handle,&f->snapshot,e) && f->snapshot.length==doc.proof.rawSize && !memcmp(f->snapshot.sha256,doc.proof.rawSha256,32) && readBody(f->handle,&doc,cancelled,user,e);
  if(!okay){fail(e,e && e->code?e->code:XIDE_MODEL_CONFLICT,"Closed source raw digest or decoded snapshot no longer matches.");break;}
  for(size_t j=0;j<i;++j){const pinned_file* old=xrtArrayConstGet(&p->files,j);if(f->snapshot.volume==old->snapshot.volume && f->snapshot.index==old->snapshot.index){okay=fail(e,XIDE_MODEL_CONFLICT,"Workspace edit paths identify the same physical source.");break;}}
 }
 if(okay)okay=xideWorkspaceEditFilesValidate(p,cancelled,user,e);
 if(!okay){if(e && !e->code)fail(e,XIDE_MODEL_MEMORY,"Cannot prepare workspace edit file gate.");xideWorkspaceEditFilesDestroy(p);p=NULL;}return p;
}
int xideWorkspaceEditFilesValidate(xide_workspace_edit_files* p,int (*cancelled)(void*),void* user,xide_model_error* e)
{
 if(e){memset(e,0,sizeof(*e));}if(!p)return fail(e,XIDE_MODEL_INVALID,"Missing workspace edit file gate.");
 for(size_t i=0;i<p->directories.Count;++i){
  if(stopped(cancelled,user,e)){return 0;}pinned_directory* d=xrtArrayGet(&p->directories,i);BY_HANDLE_FILE_INFORMATION held={0},named={0};HANDLE h=CreateFileW(d->path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
  int okay=ordinary(d->handle,1,&held) && sameFile(&held,&d->identity) && ordinary(h,1,&named) && sameFile(&held,&named);if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);if(!okay)return fail(e,XIDE_MODEL_CONFLICT,"Workspace edit ancestor identity changed.");
 }
 for(size_t i=0;i<p->files.Count;++i){
  if(stopped(cancelled,user,e)){return 0;}pinned_file* f=xrtArrayGet(&p->files,i);BY_HANDLE_FILE_INFORMATION held={0},named={0};HANDLE h=CreateFileW(f->path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
  int okay=ordinary(f->handle,0,&held) && ordinary(h,0,&named) && sameFile(&held,&named) && (((uint64_t)held.nFileSizeHigh<<32)|held.nFileSizeLow)==f->snapshot.length;if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);if(!okay)return fail(e,XIDE_MODEL_CONFLICT,"Workspace edit source identity or write attributes changed.");
  xide_file_snapshot current={0};if(!xideFileSnapshotInspectHandle(f->handle,&current,e) || !xideFileSnapshotEqual(&f->snapshot,&current))return fail(e,XIDE_MODEL_CONFLICT,"Workspace edit source bytes or metadata changed.");
 }
 return !stopped(cancelled,user,e);
}
