#define _WIN32_WINNT 0x0601
#include "platform/state_repair.h"
#include "platform/path.h"
#include <bcrypt.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int fail(xide_model_error* e,int code,const char* s) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static int put(xvalue* v,const char* key,xvalue* value) { return value && xrtValueObjectSetNew(v,sv(key),value); }
static wchar_t* wide(const char* s) { int n=s ? MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0) : 0; wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL; if(w) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n); return w; }
static int cancelled(volatile LONG* p) { return p && InterlockedCompareExchange(p,0,0); }
static int ordinary(HANDLE h,int directory,BY_HANDLE_FILE_INFORMATION* out)
{
    BY_HANDLE_FILE_INFORMATION info={0}; int okay=h!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(h,&info) &&
        !(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) && !!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==directory && (directory || info.nNumberOfLinks==1);
    if(okay && out) *out=info;
    return okay;
}
/* Pin every existing ancestor before creating its child. Reparse ancestors and
   directory renames are rejected, including custom preferences outside scope. */
static int pinTree(xide_state_repair* r,const char* path)
{
    char* parent=xidePathDirectory(path); int okay=parent!=NULL;
    if(okay && !xidePathLexicalEqual(parent,path)) okay=pinTree(r,parent);
    free(parent); if(!okay || r->parentCount>=256) return 0;
    wchar_t* w=wide(path); if(!w) return 0;
    if(!CreateDirectoryW(w,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS && GetLastError()!=ERROR_ACCESS_DENIED) { free(w); return 0; }
    HANDLE h=CreateFileW(w,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL); free(w);
    if(!ordinary(h,1,NULL)) { if(h!=INVALID_HANDLE_VALUE) CloseHandle(h); return 0; }
    r->parents[r->parentCount++]=h; return 1;
}
static int pinChild(xide_state_repair* r,const char* path,int fresh)
{
    wchar_t* w=wide(path); if(!w) return 0;
    int made=CreateDirectoryW(w,NULL),status=GetLastError();
    if(!made && (fresh || status!=ERROR_ALREADY_EXISTS)) { free(w); return 0; }
    HANDLE h=CreateFileW(w,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL); free(w);
    if(r->parentCount>=256 || !ordinary(h,1,NULL)) { if(h!=INVALID_HANDLE_VALUE) CloseHandle(h); return 0; }
    r->parents[r->parentCount++]=h; return 1;
}
static HANDLE createNew(const char* path)
{ wchar_t* w=wide(path); HANDLE h=w ? CreateFileW(w,GENERIC_WRITE|DELETE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; free(w); return h; }
static int writeBytes(HANDLE h,const unsigned char* bytes,size_t count)
{
    while(count) { DWORD n=0,want=count>65536 ? 65536 : (DWORD)count; if(!WriteFile(h,bytes,want,&n,NULL) || n!=want) return 0; bytes+=n; count-=n; } return 1;
}
static int renameHandle(HANDLE h,const char* destination)
{
    wchar_t* w=wide(destination); if(!w) return 0;
    size_t n=wcslen(w),size=offsetof(FILE_RENAME_INFO,FileName)+(n+1)*sizeof(*w); FILE_RENAME_INFO* info=calloc(1,size);
    int okay=info!=NULL; if(okay) { info->ReplaceIfExists=FALSE; info->FileNameLength=(DWORD)(n*sizeof(*w)); memcpy(info->FileName,w,(n+1)*sizeof(*w)); okay=SetFileInformationByHandle(h,FileRenameInfo,info,(DWORD)size); }
    free(info); free(w); return okay;
}
static int copyRaw(HANDLE source,const char* target,volatile LONG* cancel,uint64_t* bytes,char digest[65])
{
    BCRYPT_ALG_HANDLE algorithm=NULL; BCRYPT_HASH_HANDLE hash=NULL; unsigned char *object=NULL,*buffer=NULL,raw[32]; DWORD length=0,returned=0;
    HANDLE dest=createNew(target); int okay=dest!=INVALID_HANDLE_VALUE && BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0)==0;
    if(okay) okay=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,(PUCHAR)&length,sizeof(length),&returned,0)==0;
    if(okay) { object=malloc(length); buffer=malloc(65536); okay=object && buffer && BCryptCreateHash(algorithm,&hash,object,length,NULL,0,0)==0; }
    *bytes=0;
    while(okay) {
        DWORD n=0; if(cancelled(cancel) || !ReadFile(source,buffer,65536,&n,NULL)) { okay=0; break; } if(!n) break;
        okay=writeBytes(dest,buffer,n) && BCryptHashData(hash,buffer,n,0)==0; *bytes+=n;
    }
    if(okay) okay=FlushFileBuffers(dest) && BCryptFinishHash(hash,raw,sizeof(raw),0)==0;
    if(okay) for(int i=0;i<32;++i) snprintf(digest+i*2,3,"%02x",raw[i]);
    if(hash) BCryptDestroyHash(hash);
    if(algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
    if(dest!=INVALID_HANDLE_VALUE) CloseHandle(dest);
    free(buffer); free(object); return okay;
}
static int writeJson(const char* path,xvalue* root)
{
    size_t count=0; char* text=xrtJsonStringify(root,true,&count); HANDLE h=text && count<=8u*1024u*1024u ? createNew(path) : INVALID_HANDLE_VALUE;
    int okay=h!=INVALID_HANDLE_VALUE && writeBytes(h,(unsigned char*)text,count) && FlushFileBuffers(h);
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    xrtFree(text); return okay;
}
static int rollback(xide_state_repair* r)
{ if(!r->retired || r->published) return 1; if(r->original && renameHandle(r->original,r->path)) { r->retired=0; return 1; } return 0; }
void xideStateRepairClear(xide_state_repair* r)
{
    if(!r) return;
    if(!rollback(r)) fputs("state repair: rollback blocked; complete raw backup and retired original are preserved.\n",stderr);
    if(r->original) CloseHandle(r->original);
    if(r->lock) CloseHandle(r->lock);
    for(size_t i=r->parentCount;i>0;--i) CloseHandle(r->parents[i-1]);
    free(r->path); free(r->directory); memset(r,0,sizeof(*r));
}
int xideStateRepairPrepare(const char* path,volatile LONG* cancel,xide_state_repair* out,xide_model_error* e)
{
    if(e) memset(e,0,sizeof(*e));
    if(!out || !path) return fail(e,XIDE_MODEL_INVALID,"Invalid global-state repair path.");
    xide_state_repair r={0}; char *parent=NULL,*lockPath=NULL,*archiveRoot=NULL,*id=NULL,*rawPath=NULL,*manifestPath=NULL; xvalue* manifest=NULL; int okay=0;
    r.path=xidePathResolveLexical(NULL,path); parent=r.path ? xidePathDirectory(r.path) : NULL;
    if(!parent || cancelled(cancel) || !pinTree(&r,parent)) { fail(e,XIDE_MODEL_INVALID,"State parent is unavailable or traverses a reparse directory; original preserved."); goto done; }
    size_t n=strlen(r.path); lockPath=malloc(n+6); if(lockPath) { memcpy(lockPath,r.path,n); memcpy(lockPath+n,".lock",6); }
    wchar_t* w=wide(lockPath); HANDLE h=w ? CreateFileW(w,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; free(w);
    if(h==INVALID_HANDLE_VALUE) { fail(e,XIDE_MODEL_CONFLICT,"Another IDE is writing this state file; original preserved."); goto done; }
    r.lock=h; if(!ordinary(h,0,NULL)) { fail(e,XIDE_MODEL_INVALID,"State writer lock is unsafe; original preserved."); goto done; }
    w=wide(r.path); h=w ? CreateFileW(w,GENERIC_READ|DELETE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; DWORD status=GetLastError(); free(w);
    if(h!=INVALID_HANDLE_VALUE) { r.original=h; BY_HANDLE_FILE_INFORMATION info={0};
        if(!ordinary(h,0,&info) || (info.dwFileAttributes&FILE_ATTRIBUTE_READONLY)) { fail(e,XIDE_MODEL_INVALID,"State file is read-only, linked or unsafe; original preserved."); goto done; } r.existed=1;
    } else if(status!=ERROR_FILE_NOT_FOUND && status!=ERROR_PATH_NOT_FOUND) { fail(e,XIDE_MODEL_IO,"Cannot lock original state file for backup; original preserved."); goto done; }
    archiveRoot=xidePathResolve(parent,"state-backups"); id=xideCreateId("backup"); r.directory=archiveRoot && id ? xidePathResolve(archiveRoot,id) : NULL;
    if(!archiveRoot || !r.directory || !pinChild(&r,archiveRoot,0) || !pinChild(&r,r.directory,1)) { fail(e,XIDE_MODEL_IO,"Cannot create an independent ordinary state backup."); goto done; }
    char digest[65]=""; rawPath=xidePathResolve(r.directory,"original.bin"); manifestPath=xidePathResolve(r.directory,"manifest.json");
    if(!rawPath || !manifestPath || (r.existed && !copyRaw(r.original,rawPath,cancel,&r.bytes,digest)) || cancelled(cancel)) { fail(e,XIDE_MODEL_IO,"Raw backup cancelled or incomplete; original preserved."); goto done; }
    manifest=xrtValueObject(); FILETIME now; GetSystemTimeAsFileTime(&now); uint64_t ticks=((uint64_t)now.dwHighDateTime<<32)|now.dwLowDateTime;
    if(!manifest || !put(manifest,"format",xrtValueString(sv("xlang-ide-state-backup-1"))) || !put(manifest,"state",xrtValueString(sv("complete"))) ||
       !put(manifest,"originalPath",xrtValueString(sv(r.path))) || !put(manifest,"existed",xrtValueBool(r.existed)) || !put(manifest,"bytes",xrtValueUInt(r.bytes)) ||
       !put(manifest,"sha256",xrtValueString(sv(digest))) || !put(manifest,"createdFileTime",xrtValueUInt(ticks)) || !writeJson(manifestPath,manifest)) { fail(e,XIDE_MODEL_IO,"Cannot complete backup manifest; original preserved."); goto done; }
    r.complete=1; okay=1;
done:
    free(parent); free(lockPath); free(archiveRoot); free(id); free(rawPath); free(manifestPath); xrtValueRelease(manifest);
    if(okay) { *out=r; memset(&r,0,sizeof(r)); }
    else { out->directory=r.directory; r.directory=NULL; }
    xideStateRepairClear(&r); return okay;
}
int xideStateRepairPublish(xide_state_repair* r,xvalue* root,xide_model_error* e)
{
    if(e) memset(e,0,sizeof(*e));
    if(!r || !r->complete || !r->lock || r->published || !root || !xrtValueIs(root,XVALUE_OBJECT)) return fail(e,XIDE_MODEL_INVALID,"Publish requires a complete backup and its writer lock.");
    char *replacement=xidePathResolve(r->directory,"replacement.json"),*retired=xidePathResolve(r->directory,"retired-original.bin"); size_t n=0;
    char* text=xrtJsonStringify(root,true,&n); HANDLE h=replacement && text && n<=8u*1024u*1024u ? createNew(replacement) : INVALID_HANDLE_VALUE;
    int okay=h!=INVALID_HANDLE_VALUE && writeBytes(h,(unsigned char*)text,n) && FlushFileBuffers(h); xrtFree(text);
    if(okay && r->existed) { okay=retired && renameHandle(r->original,retired); if(okay) r->retired=1; }
    if(okay) { okay=renameHandle(h,r->path); if(okay) r->published=1; }
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    free(replacement); free(retired);
    if(!okay) return fail(e,XIDE_MODEL_IO,rollback(r) ? "Cannot publish repaired state; original restored and raw backup retained." : "Publish and rollback blocked by a competing file; raw backup and retired original retained.");
    return 1;
}
