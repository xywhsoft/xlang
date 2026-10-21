#define _WIN32_WINNT 0x0601
#include "platform/file_rename.h"
#include "platform/path.h"
#include <windows.h>
#include <bcrypt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fail(xide_model_error* e,int code,const char* message,DWORD os)
{ if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s（Windows %lu）。原文件、文档及工程引用保持。",message,os); } return 0; }
static wchar_t* native(const char* path)
{
 int n=path ? MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0) : 0; wchar_t* w=n ? malloc(((size_t)n+8)*sizeof(*w)) : NULL; if(!w) return NULL;
 if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,w,n)) { free(w); return NULL; }
 for(int i=0;i<n;++i) if(w[i]==L'/') w[i]=L'\\';
 if(!wcsncmp(w,L"\\\\?\\",4)) return w;
 int unc=w[0]==L'\\' && w[1]==L'\\'; size_t length=wcslen(w);
 if(unc) { memmove(w+8,w+2,(length-1)*sizeof(*w)); memcpy(w,L"\\\\?\\UNC\\",8*sizeof(*w)); }
 else { memmove(w+4,w,(length+1)*sizeof(*w)); memcpy(w,L"\\\\?\\",4*sizeof(*w)); } return w;
}
static int validTarget(const wchar_t* target)
{
 const wchar_t* name=wcsrchr(target,L'\\'); name=name ? name+1 : target; size_t n=wcslen(name); if(!n || n>255 || name[0]==L' ' || name[n-1]==L' ' || name[n-1]==L'.') return 0;
 for(size_t i=0;i<n;++i) if(name[i]<32 || wcschr(L"<>:\"/\\|?*",name[i])) return 0;
 wchar_t base[256]={0}; size_t b=0; while(b<n && name[b]!=L'.') { base[b]=name[b]; ++b; }
 while(b && base[b-1]==L' ') { base[--b]=0; }
 const wchar_t* reserved[]={L"CON",L"PRN",L"AUX",L"NUL",L"CONIN$",L"CONOUT$"};
 for(size_t i=0;i<sizeof(reserved)/sizeof(*reserved);++i) if(CompareStringOrdinal(base,-1,reserved[i],-1,TRUE)==CSTR_EQUAL) return 0;
 if(b==4 && (CompareStringOrdinal(base,3,L"COM",3,TRUE)==CSTR_EQUAL || CompareStringOrdinal(base,3,L"LPT",3,TRUE)==CSTR_EQUAL) && ((base[3]>=L'1' && base[3]<=L'9') || base[3]==L'\x00b9' || base[3]==L'\x00b2' || base[3]==L'\x00b3')) { return 0; }
 return 1;
}
int xideFileRenameTargetValid(const char* target,xide_model_error* e)
{
 wchar_t* name=native(target); if(!name) return fail(e,XIDE_MODEL_INVALID,"目标路径不是有效 UTF-8",0);
 int okay=validTarget(name); free(name); return okay ? 1 : fail(e,XIDE_MODEL_INVALID,"目标文件名包含 Windows 保留名称或无效字符",0);
}
int xideFileRenameSourceValid(const char* source,xide_model_error* e)
{
 wchar_t* w=native(source); if(!w) return fail(e,XIDE_MODEL_INVALID,"源路径不是有效 UTF-8",0);
 DWORD attributes=GetFileAttributesW(w),code=GetLastError(); free(w);
 if(attributes==INVALID_FILE_ATTRIBUTES) return fail(e,XIDE_MODEL_IO,"源文件不存在或不可读取",code);
 if(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) return fail(e,XIDE_MODEL_INVALID,"只能重命名普通文件",0);
 if(attributes&FILE_ATTRIBUTE_READONLY) return fail(e,XIDE_MODEL_IO,"源文件为只读，请先明确解除只读属性",ERROR_ACCESS_DENIED);
 return 1;
}
static void metadata(xide_file_snapshot* s,const BY_HANDLE_FILE_INFORMATION* i)
{
 s->length=((uint64_t)i->nFileSizeHigh<<32)|i->nFileSizeLow; s->index=((uint64_t)i->nFileIndexHigh<<32)|i->nFileIndexLow;
 s->lastWrite=((uint64_t)i->ftLastWriteTime.dwHighDateTime<<32)|i->ftLastWriteTime.dwLowDateTime; s->volume=i->dwVolumeSerialNumber; s->attributes=i->dwFileAttributes;
}
static int sameMetadata(const xide_file_snapshot* a,const xide_file_snapshot* b)
{ return a->length==b->length && a->index==b->index && a->lastWrite==b->lastWrite && a->volume==b->volume && a->attributes==b->attributes; }
int xideFileSnapshotEqual(const xide_file_snapshot* a,const xide_file_snapshot* b)
{ return a && b && sameMetadata(a,b) && !memcmp(a->sha256,b->sha256,sizeof(a->sha256)); }
static int snapshotHandle(HANDLE file,xide_file_snapshot* out,xide_model_error* e,int inspect)
{
 BY_HANDLE_FILE_INFORMATION first={0},last={0}; xide_file_snapshot snapshot={0},after={0};
 if(!GetFileInformationByHandle(file,&first)) return fail(e,XIDE_MODEL_IO,"无法查询源文件基线",GetLastError());
 if((first.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || (!inspect && (first.dwFileAttributes&FILE_ATTRIBUTE_READONLY))) return fail(e,XIDE_MODEL_IO,"源文件不是可改名的普通非只读文件",ERROR_ACCESS_DENIED);
 metadata(&snapshot,&first); if(inspect && snapshot.length>512u*1024u*1024u) return fail(e,XIDE_MODEL_INVALID,"工具链检查文件超过 512 MiB 上限",0); LARGE_INTEGER beginning={0}; if(!SetFilePointerEx(file,beginning,NULL,FILE_BEGIN)) return fail(e,XIDE_MODEL_IO,"无法读取文件基线",GetLastError());
 BCRYPT_ALG_HANDLE algorithm=NULL; BCRYPT_HASH_HANDLE hash=NULL; unsigned char* object=NULL; DWORD length=0,returned=0; NTSTATUS status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0);
 if(status==0) status=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,(PUCHAR)&length,sizeof(length),&returned,0);
 if(status==0) { object=malloc(length); if(!object) { BCryptCloseAlgorithmProvider(algorithm,0); return fail(e,XIDE_MODEL_MEMORY,"无法准备 SHA-256 文件基线",0); } status=BCryptCreateHash(algorithm,&hash,object,length,NULL,0,0); }
 unsigned char block[65536]; uint64_t at=0; DWORD os=0;
 while(status==0 && at<snapshot.length) {
  DWORD read=0,want=snapshot.length-at>sizeof(block) ? (DWORD)sizeof(block) : (DWORD)(snapshot.length-at);
  if(!ReadFile(file,block,want,&read,NULL) || read!=want) { os=GetLastError(); if(!os) os=ERROR_HANDLE_EOF; break; }
  status=BCryptHashData(hash,block,read,0); at+=read;
 }
 if(status==0 && !os) status=BCryptFinishHash(hash,snapshot.sha256,sizeof(snapshot.sha256),0);
 if(hash) { BCryptDestroyHash(hash); } free(object); if(algorithm) { BCryptCloseAlgorithmProvider(algorithm,0); }
 if(status!=0 || os) return fail(e,XIDE_MODEL_IO,"无法完成源文件 SHA-256 基线",os ? os : (DWORD)status);
 if(!GetFileInformationByHandle(file,&last)) return fail(e,XIDE_MODEL_IO,"无法复核文件基线",GetLastError());
 metadata(&after,&last); if(!sameMetadata(&snapshot,&after)) return fail(e,XIDE_MODEL_CONFLICT,"读取期间源文件已变化，请重新打开重命名窗口",ERROR_FILE_INVALID);
 *out=snapshot; return 1;
}
int xideFileSnapshotInspectHandle(void* handle,xide_file_snapshot* out,xide_model_error* e)
{
 if(e) memset(e,0,sizeof(*e));
 if(!handle || handle==INVALID_HANDLE_VALUE || !out) return fail(e,XIDE_MODEL_INVALID,"缺少已固定的文件基线句柄",0);
 return snapshotHandle((HANDLE)handle,out,e,1);
}
int xideFileSnapshotRead(const char* source,xide_file_snapshot* out,xide_model_error* e)
{
 if(e) { memset(e,0,sizeof(*e)); } if(!out) return fail(e,XIDE_MODEL_INVALID,"缺少文件基线目标",0);
 wchar_t* w=native(source); if(!w) return fail(e,XIDE_MODEL_INVALID,"源路径不是有效 UTF-8",0);
 HANDLE file=CreateFileW(w,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL); DWORD code=GetLastError(); free(w);
 if(file==INVALID_HANDLE_VALUE) return fail(e,XIDE_MODEL_IO,"无法读取源文件基线",code);
 int okay=snapshotHandle(file,out,e,0); CloseHandle(file); return okay;
}
int xideFileSnapshotInspect(const char* source,xide_file_snapshot* out,xide_model_error* e)
{
 if(e) { memset(e,0,sizeof(*e)); } if(!out) return fail(e,XIDE_MODEL_INVALID,"缺少只读文件检查目标",0);
 wchar_t* w=native(source); if(!w) return fail(e,XIDE_MODEL_INVALID,"检查路径不是有效 UTF-8",0);
 HANDLE file=CreateFileW(w,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL); DWORD code=GetLastError(); free(w);
 if(file==INVALID_HANDLE_VALUE) return fail(e,XIDE_MODEL_IO,"无法读取只读文件基线",code);
 int okay=snapshotHandle(file,out,e,1); CloseHandle(file); return okay;
}
int xideFileRenameChecked(const char* source,const char* target,const xide_text_file* baseline,const xide_file_snapshot* expected,xide_model_error* e)
{
 if(e) { memset(e,0,sizeof(*e)); }
 if(!source || !target || (!baseline && !expected) || (baseline && baseline->rawLength && !baseline->raw)) return fail(e,XIDE_MODEL_INVALID,"无效的重命名请求",0);
 wchar_t *old=native(source),*next=native(target); if(!old || !next) { free(old); free(next); return fail(e,XIDE_MODEL_MEMORY,"无法准备重命名路径",0); }
 if(!validTarget(next)) { free(old); free(next); return fail(e,XIDE_MODEL_INVALID,"目标文件名包含 Windows 保留名称或无效字符",0); }
 size_t n=wcslen(next),bytes=offsetof(FILE_RENAME_INFO,FileName)+(n+1)*sizeof(wchar_t); FILE_RENAME_INFO* rename=calloc(1,bytes);
 if(!rename) { free(old); free(next); return fail(e,XIDE_MODEL_MEMORY,"无法准备重命名事务",0); }
 rename->ReplaceIfExists=FALSE; rename->FileNameLength=(DWORD)(n*sizeof(wchar_t)); memcpy(rename->FileName,next,(n+1)*sizeof(wchar_t));
 HANDLE file=CreateFileW(old,GENERIC_READ|DELETE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
 int okay=file!=INVALID_HANDLE_VALUE; DWORD code=GetLastError(); const char* phase="无法锁定源文件"; BY_HANDLE_FILE_INFORMATION info={0};
 if(okay) { phase="无法查询源文件属性"; okay=GetFileInformationByHandle(file,&info)!=0; code=GetLastError(); }
 if(okay && (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) { okay=0; phase="源文件不是普通文件"; code=ERROR_INVALID_DATA; }
 if(okay && (info.dwFileAttributes&FILE_ATTRIBUTE_READONLY)) { okay=0; phase="源文件为只读，请先明确解除只读属性"; code=ERROR_ACCESS_DENIED; }
 int conflict=0;
 if(okay && expected) { xide_file_snapshot now={0}; phase="源文件身份或内容已变化，请重新审阅"; okay=snapshotHandle(file,&now,e,0) && xideFileSnapshotEqual(expected,&now); conflict=!okay; code=okay ? 0 : ERROR_FILE_INVALID; }
 if(okay && baseline) { LARGE_INTEGER start={0}; okay=SetFilePointerEx(file,start,NULL,FILE_BEGIN)!=0; code=GetLastError(); }
 if(okay && baseline) {
  phase="源文件在磁盘已变化，请先处理磁盘变化"; LARGE_INTEGER size={0}; okay=GetFileSizeEx(file,&size) && size.QuadPart>=0 && (uint64_t)size.QuadPart==baseline->rawLength;
  unsigned char block[65536]; size_t at=0;
  while(okay && at<baseline->rawLength) { DWORD read=0,want=baseline->rawLength-at>sizeof(block) ? sizeof(block) : (DWORD)(baseline->rawLength-at); okay=ReadFile(file,block,want,&read,NULL) && read==want && !memcmp(block,baseline->raw+at,want); at+=read; }
  conflict=!okay; code=okay ? 0 : ERROR_FILE_INVALID;
 }
 if(okay) {
  DWORD attributes=GetFileAttributesW(next),query=GetLastError();
  int caseOnly=CompareStringOrdinal(old,-1,next,-1,TRUE)==CSTR_EQUAL && CompareStringOrdinal(old,-1,next,-1,FALSE)!=CSTR_EQUAL;
  /* Existing targets are accepted solely for the same file's case spelling. */
  if(attributes!=INVALID_FILE_ATTRIBUTES && !(caseOnly && xidePathEqual(source,target))) { okay=0; conflict=1; code=ERROR_ALREADY_EXISTS; phase="目标已存在，不能覆盖"; }
  else if(attributes==INVALID_FILE_ATTRIBUTES && query!=ERROR_FILE_NOT_FOUND && query!=ERROR_PATH_NOT_FOUND) { okay=0; code=query; phase="无法核对目标路径"; }
 }
 if(okay) { phase="重命名失败，请检查共享、权限及是否跨卷"; okay=SetFileInformationByHandle(file,FileRenameInfo,rename,(DWORD)bytes)!=0; code=GetLastError(); }
 if(file!=INVALID_HANDLE_VALUE) { CloseHandle(file); } free(old); free(next); free(rename);
 return okay ? 1 : fail(e,conflict ? XIDE_MODEL_CONFLICT : XIDE_MODEL_IO,phase,code);
}

int xideFileRename(const char* source,const char* target,const xide_text_file* baseline,xide_model_error* e)
{ return xideFileRenameChecked(source,target,baseline,NULL,e); }

static int directoryInfo(HANDLE file,xide_directory_identity* out,xide_model_error* e)
{
 BY_HANDLE_FILE_INFORMATION info={0}; if(!GetFileInformationByHandle(file,&info)) return fail(e,XIDE_MODEL_IO,"无法读取目录身份",GetLastError());
 if(!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) return fail(e,XIDE_MODEL_INVALID,"请选择普通目录，不能移动重解析点",0);
 if(info.dwFileAttributes&FILE_ATTRIBUTE_READONLY) return fail(e,XIDE_MODEL_IO,"源目录为只读，请先明确处理属性",ERROR_ACCESS_DENIED);
 *out=(xide_directory_identity){((uint64_t)info.nFileIndexHigh<<32)|info.nFileIndexLow,info.dwVolumeSerialNumber}; return 1;
}
int xideDirectoryIdentityRead(const char* source,xide_directory_identity* out,xide_model_error* e)
{
 if(e) { memset(e,0,sizeof(*e)); } if(!out) return fail(e,XIDE_MODEL_INVALID,"缺少目录身份目标",0);
 wchar_t* w=native(source); if(!w) return fail(e,XIDE_MODEL_INVALID,"目录路径不是有效 UTF-8",0);
 HANDLE file=CreateFileW(w,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL); DWORD code=GetLastError(); free(w);
 if(file==INVALID_HANDLE_VALUE) return fail(e,XIDE_MODEL_IO,"无法读取源目录",code);
 int okay=directoryInfo(file,out,e); CloseHandle(file); return okay;
}
int xideDirectoryRenameChecked(const char* source,const char* target,const xide_directory_identity* expected,xide_model_error* e)
{
 if(e) { memset(e,0,sizeof(*e)); } if(!source || !target || !expected) return fail(e,XIDE_MODEL_INVALID,"无效目录迁移请求",0);
 wchar_t *old=native(source),*next=native(target); if(!old || !next) { free(old); free(next); return fail(e,XIDE_MODEL_MEMORY,"无法准备目录路径",0); }
 if(!validTarget(next)) { free(old); free(next); return fail(e,XIDE_MODEL_INVALID,"无效目标目录名称",0); }
 size_t n=wcslen(next),bytes=offsetof(FILE_RENAME_INFO,FileName)+(n+1)*sizeof(wchar_t); FILE_RENAME_INFO* rename=calloc(1,bytes);
 if(!rename) { free(old); free(next); return fail(e,XIDE_MODEL_MEMORY,"无法准备目录迁移",0); }
 rename->ReplaceIfExists=FALSE; rename->FileNameLength=(DWORD)(n*sizeof(wchar_t)); memcpy(rename->FileName,next,(n+1)*sizeof(wchar_t));
 HANDLE file=CreateFileW(old,DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL); DWORD code=GetLastError(); int okay=file!=INVALID_HANDLE_VALUE,conflict=0; const char* phase="无法锁定源目录";
 xide_directory_identity current={0};
 if(okay) { okay=directoryInfo(file,&current,e); if(!okay) goto done; }
 if(okay && (current.index!=expected->index || current.volume!=expected->volume)) { okay=0; conflict=1; phase="源目录已被替换，请重新审阅"; code=ERROR_FILE_INVALID; }
 if(okay) {
  DWORD attributes=GetFileAttributesW(next),query=GetLastError(); int caseOnly=CompareStringOrdinal(old,-1,next,-1,TRUE)==CSTR_EQUAL && CompareStringOrdinal(old,-1,next,-1,FALSE)!=CSTR_EQUAL;
  if(attributes!=INVALID_FILE_ATTRIBUTES && !(caseOnly && xidePathEqual(source,target))) { okay=0; conflict=1; phase="目标目录已存在，不能合并或覆盖"; code=ERROR_ALREADY_EXISTS; }
  else if(attributes==INVALID_FILE_ATTRIBUTES && query!=ERROR_FILE_NOT_FOUND && query!=ERROR_PATH_NOT_FOUND) { okay=0; phase="无法核对目标目录"; code=query; }
 }
 if(okay) { phase="目录迁移失败，请检查共享、权限和卷"; okay=SetFileInformationByHandle(file,FileRenameInfo,rename,(DWORD)bytes)!=0; code=GetLastError(); }
 if(!okay) fail(e,conflict ? XIDE_MODEL_CONFLICT : XIDE_MODEL_IO,phase,code);
done:
 if(file!=INVALID_HANDLE_VALUE) { CloseHandle(file); } free(old); free(next); free(rename); return okay;
}
