#include "platform/recovery_backup.h"
#include "platform/state_file.h"
#include "platform/path.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define LIMIT 20000u
static const char* metadataNames[]={"session.json","layout.json","previous-session.json"};
static int fail(xide_model_error* e,int code,const char* s) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* s) { return v ? xrtValueObjectGet(v,sv(s)) : NULL; }
static const char* text(const xvalue* v) { xstrview s={0}; return xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) ? s.Data : NULL; }
static int put(xvalue* v,const char* key,xvalue* value) { return value && xrtValueObjectSetNew(v,sv(key),value); }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static char* join(const char* a,const char* b) { size_t n=strlen(a),m=strlen(b); char* p=malloc(n+m+2); if(p) { memcpy(p,a,n); p[n]='/'; memcpy(p+n+1,b,m+1); } return p; }
static wchar_t* wide(const char* p) { return xidePathNativeWide(p); }
static char* utf8(const wchar_t* p) { int n=WideCharToMultiByte(CP_UTF8,0,p,-1,NULL,0,NULL,NULL); char* s=n ? malloc(n) : NULL; if(s) WideCharToMultiByte(CP_UTF8,0,p,-1,s,n,NULL,NULL); return s; }
static int cancelled(volatile LONG* cancel) { return cancel && InterlockedCompareExchange(cancel,0,0)!=0; }
static int leaf(const char* s)
{
    if(!s || !*s || strlen(s)>160 || !strcmp(s,".") || !strcmp(s,"..")) return 0;
    size_t n=strlen(s); if(s[n-1]=='.' || s[n-1]==' ') return 0;
    for(;*s;++s) if((unsigned char)*s<32 || strchr("/\\:*?\"<>|",*s)) return 0;
    return 1;
}
static HANDLE pin(const char* p,int make)
{
    wchar_t* w=wide(p); if(!w) return INVALID_HANDLE_VALUE;
    if(make && !CreateDirectoryW(w,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS) { free(w); return INVALID_HANDLE_VALUE; }
    HANDLE h=CreateFileW(w,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL); free(w);
    BY_HANDLE_FILE_INFORMATION info={0};
    if(h!=INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(h,&info) || !(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))) { CloseHandle(h); h=INVALID_HANDLE_VALUE; }
    return h;
}
static HANDLE openOrdinary(const char* p,DWORD access)
{
    wchar_t* w=wide(p); HANDLE h=w ? CreateFileW(w,access,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; free(w);
    BY_HANDLE_FILE_INFORMATION info={0};
    if(h!=INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(h,&info) || (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))) { CloseHandle(h); h=INVALID_HANDLE_VALUE; SetLastError(ERROR_INVALID_DATA); }
    return h;
}
static int archiveId(const char* s)
{ if(!s || strlen(s)!=43 || strncmp(s,"backup-",7)) return 0; for(int i=7;i<43;++i) { int k=i-7; if(k==8 || k==13 || k==18 || k==23) { if(s[i]!='-') return 0; } else if(!strchr("0123456789abcdefABCDEF",s[i])) return 0; } return 1; }
/* xideCreateId("backup") has six letters plus '-' and the 36-byte UUID. */
static char* child(const char* root,const char* ref,HANDLE* parents)
{
    const char* name=ref; char *base=NULL,*result=NULL; HANDLE a=INVALID_HANDLE_VALUE,b=INVALID_HANDLE_VALUE,c=INVALID_HANDLE_VALUE;
    if(!strncmp(ref,"recovery/",9)) { base=join(root,"recovery"); name=ref+9; a=base ? pin(base,0) : INVALID_HANDLE_VALUE; }
    else if(!strncmp(ref,"archives/",9)) {
        const char* slash=strchr(ref+9,'/'); if(!slash) return NULL;
        size_t n=(size_t)(slash-(ref+9)); if(n>=64) return NULL; char id[64]; memcpy(id,ref+9,n); id[n]=0;
        if(!archiveId(id) || strncmp(slash,"/recovery/",10)) return NULL;
        char* archives=join(root,"archives"); a=archives ? pin(archives,0) : INVALID_HANDLE_VALUE;
        char* archive=archives ? join(archives,id) : NULL; b=archive ? pin(archive,0) : INVALID_HANDLE_VALUE;
        base=archive ? join(archive,"recovery") : NULL; c=base ? pin(base,0) : INVALID_HANDLE_VALUE;
        name=slash+10; free(archives); free(archive);
        if(b==INVALID_HANDLE_VALUE || c==INVALID_HANDLE_VALUE) goto done;
    } else return NULL;
    if(a!=INVALID_HANDLE_VALUE && base && leaf(name)) result=join(base,name);
done:
    if(result && parents) { parents[0]=a; parents[1]=b; parents[2]=c; a=b=c=INVALID_HANDLE_VALUE; }
    if(a!=INVALID_HANDLE_VALUE) CloseHandle(a);
    if(b!=INVALID_HANDLE_VALUE) CloseHandle(b);
    if(c!=INVALID_HANDLE_VALUE) CloseHandle(c);
    free(base); return result;
}
void xideRecoveryListClear(xide_recovery_list* list)
{ if(!list) return; for(size_t i=0;i<list->count;++i) { free(list->items[i].reference); free(list->items[i].label); } free(list->items); memset(list,0,sizeof(*list)); }
static int appendItem(xide_recovery_list* list,const char* reference,uint64_t bytes,int archived)
{
    if(list->count==LIMIT) return 0;
    if(list->count==list->capacity) { size_t cap=list->capacity ? list->capacity*2 : 64; xide_recovery_item* p=realloc(list->items,cap*sizeof(*p)); if(!p) return 0; list->items=p; list->capacity=cap; }
    xide_recovery_item item={0}; item.reference=copy(reference); item.bytes=bytes; item.archived=archived;
    const char* name=xidePathFilename(reference); size_t n=strlen(name)+80; item.label=malloc(n);
    if(item.label) snprintf(item.label,n,"%s · %llu 字节 · %s",archived ? "备份" : "当前",(unsigned long long)bytes,name);
    if(!item.reference || !item.label) { free(item.reference); free(item.label); return 0; }
    list->items[list->count++]=item; return 1;
}
static int scanBodies(const char* root,const char* prefix,xide_recovery_list* list,volatile LONG* cancel,xide_model_error* e)
{
    char* dir=join(root,prefix); HANDLE pinned=dir ? pin(dir,0) : INVALID_HANDLE_VALUE; char* pattern=dir ? join(dir,"*") : NULL; wchar_t* w=wide(pattern); free(pattern);
    if(pinned==INVALID_HANDLE_VALUE || !w) { free(w); free(dir); return fail(e,XIDE_MODEL_IO,"Cannot inspect recovery directory."); }
    WIN32_FIND_DATAW f; HANDLE search=FindFirstFileW(w,&f); free(w); int okay=1;
    if(search==INVALID_HANDLE_VALUE && GetLastError()!=ERROR_FILE_NOT_FOUND) okay=0;
    size_t count=0;
    if(search!=INVALID_HANDLE_VALUE) do {
        if(!wcscmp(f.cFileName,L".") || !wcscmp(f.cFileName,L"..")) continue;
        if(++count>LIMIT || cancelled(cancel)) { okay=0; break; }
        if(f.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        char* name=utf8(f.cFileName);
        if(name && leaf(name) && xidePathHasExtension(name,".text")) { char* ref=join(prefix,name); okay=ref && appendItem(list,ref,((uint64_t)f.nFileSizeHigh<<32)|f.nFileSizeLow,strncmp(prefix,"recovery",8)!=0); free(ref); }
        free(name); if(!okay) break;
    } while(FindNextFileW(search,&f));
    if(okay && search!=INVALID_HANDLE_VALUE && GetLastError()!=ERROR_NO_MORE_FILES) okay=0;
    if(search!=INVALID_HANDLE_VALUE) FindClose(search);
    CloseHandle(pinned); free(dir);
    return okay ? 1 : fail(e,XIDE_MODEL_IO,"Recovery scan cancelled, failed or exceeded its limit.");
}
static int number(const xvalue* v,uint64_t* out) { uint64 u=0; int64 i=0; if(xrtValueGetUInt(v,&u)) { *out=u; return 1; } if(xrtValueGetInt(v,&i) && i>=0) { *out=(uint64_t)i; return 1; } return 0; }
static int validateArchive(const char* archive,int* complete,xide_model_error* e)
{
    *complete=0; char* path=join(archive,"manifest.json"); xide_state_file state={0}; HANDLE manifest=path ? openOrdinary(path,GENERIC_READ) : INVALID_HANDLE_VALUE;
    int okay=manifest!=INVALID_HANDLE_VALUE && xideStateOpen(&state,path,"xlang-ide-recovery-backup-1",e) && state.existed;
    free(path); if(manifest!=INVALID_HANDLE_VALUE) CloseHandle(manifest);
    const char* phase=okay ? text(get(state.source,"state")) : NULL; const xvalue* files=okay ? get(state.source,"files") : NULL;
    if(!phase || (strcmp(phase,"complete") && strcmp(phase,"copying")) || !xrtValueIs(files,XVALUE_ARRAY) || xrtValueCount(files)>LIMIT+3) okay=0;
    if(okay && !strcmp(phase,"copying")) {
        for(int i=0;i<3;++i) { char name[80]; snprintf(name,sizeof(name),"retired-%s",metadataNames[i]); path=join(archive,name); wchar_t* w=wide(path); free(path);
            DWORD attrs=w ? GetFileAttributesW(w) : INVALID_FILE_ATTRIBUTES; DWORD status=GetLastError();
            if(!w || attrs!=INVALID_FILE_ATTRIBUTES || (status!=ERROR_FILE_NOT_FOUND && status!=ERROR_PATH_NOT_FOUND)) okay=0;
            free(w);
        }
    } else if(okay) {
        xvalue* seen=xrtValueObject(); if(!seen) okay=0;
        for(size_t i=0;okay && i<xrtValueCount(files);++i) {
            const xvalue* record=xrtValueArrayGet(files,i); const char* ref=text(get(record,"path")); uint64_t size=0;
            int body=ref && !strncmp(ref,"recovery/",9); const char* name=ref ? (body ? ref+9 : ref) : NULL;
            if(!name || !leaf(name) || (!body && strcmp(name,metadataNames[0]) && strcmp(name,metadataNames[1]) && strcmp(name,metadataNames[2])) || !number(get(record,"bytes"),&size) || get(seen,ref)) { okay=0; break; }
            okay=put(seen,ref,xrtValueBool(true));
            char* dir=body ? join(archive,"recovery") : NULL; HANDLE parent=body && dir ? pin(dir,0) : INVALID_HANDLE_VALUE;
            path=okay && (!body || parent!=INVALID_HANDLE_VALUE) ? join(body ? dir : archive,name) : NULL;
            HANDLE h=path ? openOrdinary(path,FILE_READ_ATTRIBUTES) : INVALID_HANDLE_VALUE; BY_HANDLE_FILE_INFORMATION info={0};
            okay=okay && h!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(h,&info) && info.nNumberOfLinks==1 && (((uint64_t)info.nFileSizeHigh<<32)|info.nFileSizeLow)==size;
            if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
            if(parent!=INVALID_HANDLE_VALUE) CloseHandle(parent);
            free(path); free(dir);
        }
        xrtValueRelease(seen); if(okay) *complete=1;
    }
    xideStateClear(&state); return okay ? 1 : fail(e,XIDE_MODEL_INVALID,"Recovery backup is unknown, damaged or incomplete after retirement; cleanup is disabled.");
}
static int archives(xide_recovery_guard* guard,xide_recovery_list* list,volatile LONG* cancel,xide_model_error* e)
{
    char* dir=join(guard->directory,"archives"); wchar_t* w=wide(dir); DWORD attrs=w ? GetFileAttributesW(w) : INVALID_FILE_ATTRIBUTES; DWORD error=GetLastError(); free(w);
    if(attrs==INVALID_FILE_ATTRIBUTES && (error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND)) { free(dir); return 1; }
    HANDLE parent=dir ? pin(dir,0) : INVALID_HANDLE_VALUE; if(parent==INVALID_HANDLE_VALUE) { free(dir); return fail(e,XIDE_MODEL_INVALID,"Backup storage is not an ordinary directory."); }
    char* pattern=join(dir,"*"); w=wide(pattern); free(pattern); WIN32_FIND_DATAW f; HANDLE search=w ? FindFirstFileW(w,&f) : INVALID_HANDLE_VALUE; free(w); int okay=1; size_t count=0;
    if(search==INVALID_HANDLE_VALUE && GetLastError()!=ERROR_FILE_NOT_FOUND) okay=0;
    if(search!=INVALID_HANDLE_VALUE) do {
        if(!wcscmp(f.cFileName,L".") || !wcscmp(f.cFileName,L"..")) continue;
        char* id=utf8(f.cFileName); char* backup=id ? join(dir,id) : NULL; HANDLE pinned=backup ? pin(backup,0) : INVALID_HANDLE_VALUE;
        int complete=0;
        okay=++count<=1024 && !cancelled(cancel) && archiveId(id) && pinned!=INVALID_HANDLE_VALUE && validateArchive(backup,&complete,e);
        if(okay && complete && list) { char* a=join("archives",id); char* prefix=a ? join(a,"recovery") : NULL; okay=prefix && scanBodies(guard->directory,prefix,list,cancel,e); free(a); free(prefix); }
        if(pinned!=INVALID_HANDLE_VALUE) CloseHandle(pinned);
        free(id); free(backup); if(!okay) break;
    } while(FindNextFileW(search,&f));
    if(okay && search!=INVALID_HANDLE_VALUE && GetLastError()!=ERROR_NO_MORE_FILES) okay=0;
    if(search!=INVALID_HANDLE_VALUE) FindClose(search);
    CloseHandle(parent); free(dir);
    return okay ? 1 : fail(e,XIDE_MODEL_INVALID,"Unknown or damaged recovery backup; cleanup is disabled.");
}
int xideRecoveryBackupsCheck(xide_recovery_guard* g,xide_model_error* e) { return archives(g,NULL,NULL,e); }
int xideRecoveryListRead(xide_recovery_guard* g,xide_recovery_list* out,volatile LONG* cancel,xide_model_error* e)
{
    xide_recovery_list next={0}; int okay=g && g->lockHandle && scanBodies(g->directory,"recovery",&next,cancel,e);
    /* A damaged archive must not hide otherwise usable current snapshots. */
    if(okay) { xide_model_error archiveError={0}; if(!archives(g,&next,cancel,&archiveError) && e) *e=archiveError; }
    if(!okay) { xideRecoveryListClear(&next); return 0; } xideRecoveryListClear(out); *out=next; return 1;
}
int xideRecoveryRead(xide_recovery_guard* g,const char* ref,xide_text_file* out,xide_model_error* e)
{
    if(!g || !g->lockHandle || !ref) return fail(e,XIDE_MODEL_INVALID,"Preview requires a checkpoint lock.");
    HANDLE parents[3]={INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE};
    char* path=child(g->directory,ref,parents); HANDLE h=path ? openOrdinary(path,GENERIC_READ) : INVALID_HANDLE_VALUE; free(path); LARGE_INTEGER size={0};
    int okay=h!=INVALID_HANDLE_VALUE && GetFileSizeEx(h,&size) && size.QuadPart>=0 && size.QuadPart<=16*1024*1024;
    unsigned char* raw=okay ? malloc((size_t)size.QuadPart+1) : NULL; if(!raw) okay=0; size_t used=0;
    while(okay && used<(size_t)size.QuadPart) { DWORD read=0; okay=ReadFile(h,raw+used,(DWORD)((size_t)size.QuadPart-used),&read,NULL) && read>0; used+=read; }
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    if(okay) { raw[used]=0; okay=xideTextFileDecode(raw,used,out,e); }
    for(int i=0;i<3;++i) if(parents[i]!=INVALID_HANDLE_VALUE) CloseHandle(parents[i]);
    free(raw); return okay ? 1 : fail(e,XIDE_MODEL_IO,"Cannot preview this snapshot; only ordinary UTF-8 files up to 16 MiB are supported.");
}
static int renameHandle(HANDLE h,const char* destination)
{
    wchar_t* w=wide(destination); if(!w) return 0; size_t n=wcslen(w),bytes=offsetof(FILE_RENAME_INFO,FileName)+(n+1)*sizeof(wchar_t); FILE_RENAME_INFO* info=calloc(1,bytes);
    int okay=info!=NULL; if(okay) { info->ReplaceIfExists=FALSE; info->FileNameLength=(DWORD)(n*2); memcpy(info->FileName,w,(n+1)*2); okay=SetFileInformationByHandle(h,FileRenameInfo,info,(DWORD)bytes); }
    free(info); free(w); return okay;
}
static int copyHandle(HANDLE source,const char* path,volatile LONG* cancel,uint64_t* size,uint64_t* digest)
{
    wchar_t* w=wide(path); HANDLE dest=w ? CreateFileW(w,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; free(w);
    if(dest==INVALID_HANDLE_VALUE) return 0;
    unsigned char* buffer=malloc(65536); int okay=buffer!=NULL; *size=0; *digest=UINT64_C(1469598103934665603);
    while(okay) { DWORD n=0; if(cancelled(cancel) || !ReadFile(source,buffer,65536,&n,NULL)) { okay=0; break; } if(!n) break;
        DWORD used=0; while(used<n) { DWORD count=0; if(!WriteFile(dest,buffer+used,n-used,&count,NULL) || !count) { okay=0; break; } used+=count; }
        for(DWORD i=0;i<n;++i) { *digest^=buffer[i]; *digest*=UINT64_C(1099511628211); } *size+=n;
    }
    if(okay) okay=FlushFileBuffers(dest);
    free(buffer); CloseHandle(dest); return okay;
}
static int addCopy(xvalue* files,const char* reference,HANDLE source,const char* root,volatile LONG* cancel,xide_recovery_backup* result)
{
    char* destination=join(root,reference); uint64_t bytes=0,hash=0; int okay=destination && copyHandle(source,destination,cancel,&bytes,&hash); free(destination);
    xvalue* record=okay ? xrtValueObject() : NULL; char digest[24]; snprintf(digest,sizeof(digest),"%016llx",(unsigned long long)hash);
    okay=record && put(record,"path",xrtValueString(sv(reference))) && put(record,"bytes",xrtValueUInt(bytes)) && put(record,"hash",xrtValueString(sv(digest)));
    if(okay) { okay=xrtValueArrayAppendNew(files,record); record=NULL; } xrtValueRelease(record);
    if(okay) { ++result->files; result->bytes+=bytes; } return okay;
}
int xideRecoveryBackupUndo(xide_recovery_backup* b,xide_model_error* e)
{
    int okay=1;
    for(int i=2;i>=0;--i) if(b->retired&(1u<<i)) {
        char name[80]; snprintf(name,sizeof(name),"retired-%s",metadataNames[i]); char* src=join(b->directory,name); char* dest=join(b->guard.directory,metadataNames[i]);
        HANDLE h=src ? openOrdinary(src,GENERIC_READ|DELETE) : INVALID_HANDLE_VALUE;
        if(h!=INVALID_HANDLE_VALUE && dest && renameHandle(h,dest)) b->retired&=~(1u<<i); else okay=0;
        if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
        free(src); free(dest);
    }
    return okay ? 1 : fail(e,XIDE_MODEL_IO,"Cannot roll back every retired state file; complete raw copies remain in the backup.");
}
void xideRecoveryBackupClear(xide_recovery_backup* b) { if(!b) return; xideRecoveryUnlock(&b->guard); free(b->directory); memset(b,0,sizeof(*b)); }
int xideRecoveryBackupCreate(const char* directory,volatile LONG* cancel,xide_recovery_backup* out,xide_model_error* e)
{
    xide_recovery_backup result={0}; HANDLE sources[3]={INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE}; HANDLE archiveRoot=INVALID_HANDLE_VALUE,backupDir=INVALID_HANDLE_VALUE,bodyDir=INVALID_HANDLE_VALUE,search=INVALID_HANDLE_VALUE;
    char *root=NULL,*body=NULL,*pattern=NULL,*manifestPath=NULL; xvalue *manifest=NULL,*files=NULL; xide_state_file state={0}; int okay=0;
    if(!xideRecoveryLock(&result.guard,directory,e)) return 0;
    root=join(result.guard.directory,"archives"); archiveRoot=root ? pin(root,1) : INVALID_HANDLE_VALUE;
    char* id=xideCreateId("backup"); result.directory=root && id ? join(root,id) : NULL; free(id);
    wchar_t* fresh=result.directory ? wide(result.directory) : NULL;
    if(fresh && CreateDirectoryW(fresh,NULL)) backupDir=pin(result.directory,0);
    free(fresh); body=result.directory ? join(result.directory,"recovery") : NULL; bodyDir=body ? pin(body,1) : INVALID_HANDLE_VALUE;
    if(archiveRoot==INVALID_HANDLE_VALUE || backupDir==INVALID_HANDLE_VALUE || bodyDir==INVALID_HANDLE_VALUE) { fail(e,XIDE_MODEL_IO,"Cannot create an ordinary backup directory."); goto done; }
    manifest=xrtValueObject(); files=xrtValueArray(); manifestPath=join(result.directory,"manifest.json");
    if(!manifest || !files || !manifestPath || !xideStateOpen(&state,manifestPath,"xlang-ide-recovery-backup-1",e) || !put(manifest,"format",xrtValueString(sv("xlang-ide-recovery-backup-1"))) || !put(manifest,"state",xrtValueString(sv("copying"))) || !put(manifest,"files",xrtValueArray()) || !xideStateWrite(&state,manifest,e)) goto done;
    for(int i=0;i<3;++i) {
        char* path=xideRecoveryPath(result.guard.directory,metadataNames[i],e); if(!path) goto done;
        sources[i]=openOrdinary(path,GENERIC_READ|DELETE); DWORD error=GetLastError(); free(path);
        if(sources[i]==INVALID_HANDLE_VALUE) { if(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND) continue; fail(e,XIDE_MODEL_IO,"Cannot lock original state files for backup; originals were not changed."); goto done; }
        if(!addCopy(files,metadataNames[i],sources[i],result.directory,cancel,&result)) goto copyFailed;
    }
    char* originalBody=join(result.guard.directory,"recovery"); pattern=originalBody ? join(originalBody,"*") : NULL; free(originalBody); wchar_t* w=wide(pattern); WIN32_FIND_DATAW f;
    search=w ? FindFirstFileW(w,&f) : INVALID_HANDLE_VALUE; free(w); size_t count=0;
    if(search==INVALID_HANDLE_VALUE && GetLastError()!=ERROR_FILE_NOT_FOUND) goto copyFailed;
    if(search!=INVALID_HANDLE_VALUE) do {
        if(!wcscmp(f.cFileName,L".") || !wcscmp(f.cFileName,L"..")) continue;
        if(++count>LIMIT || cancelled(cancel)) goto copyFailed;
        if(f.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        char* name=utf8(f.cFileName); char* ref=name && leaf(name) ? join("recovery",name) : NULL; char* path=ref ? child(result.guard.directory,ref,NULL) : NULL;
        HANDLE h=path ? openOrdinary(path,GENERIC_READ) : INVALID_HANDLE_VALUE; free(name); free(path);
        int copied=h!=INVALID_HANDLE_VALUE && addCopy(files,ref,h,result.directory,cancel,&result); free(ref); if(h!=INVALID_HANDLE_VALUE) CloseHandle(h); if(!copied) goto copyFailed;
    } while(FindNextFileW(search,&f));
    if(search!=INVALID_HANDLE_VALUE && GetLastError()!=ERROR_NO_MORE_FILES) goto copyFailed;
    if(search!=INVALID_HANDLE_VALUE) { FindClose(search); search=INVALID_HANDLE_VALUE; }
    if(cancelled(cancel)) goto copyFailed;
    if(!put(manifest,"state",xrtValueString(sv("complete")))) goto copyFailed;
    xvalue* owned=files; files=NULL;
    if(!put(manifest,"files",owned)) goto copyFailed;
    if(!xideStateWrite(&state,manifest,e)) goto done;
    for(int i=0;i<3;++i) if(sources[i]!=INVALID_HANDLE_VALUE) {
        char name[80]; snprintf(name,sizeof(name),"retired-%s",metadataNames[i]); char* dest=join(result.directory,name); int moved=dest && renameHandle(sources[i],dest); free(dest);
        if(!moved) { fail(e,XIDE_MODEL_IO,"Cannot retire original metadata; a complete backup was saved."); goto done; } result.retired|=1u<<i;
    }
    okay=1; goto done;
copyFailed:
    fail(e,XIDE_MODEL_IO,cancelled(cancel) ? "Backup cancelled; original metadata is preserved." : "Cannot completely copy recovery data; original metadata is preserved.");
done:
    for(int i=0;i<3;++i) if(sources[i]!=INVALID_HANDLE_VALUE) CloseHandle(sources[i]);
    if(!okay && result.retired) { xide_model_error rollback={0}; if(!xideRecoveryBackupUndo(&result,&rollback) && e) *e=rollback; }
    if(search!=INVALID_HANDLE_VALUE) FindClose(search);
    if(bodyDir!=INVALID_HANDLE_VALUE) CloseHandle(bodyDir);
    if(backupDir!=INVALID_HANDLE_VALUE) CloseHandle(backupDir);
    if(archiveRoot!=INVALID_HANDLE_VALUE) CloseHandle(archiveRoot);
    free(root); free(body); free(pattern); free(manifestPath); xideStateClear(&state); xrtValueRelease(manifest); xrtValueRelease(files);
    if(okay) { *out=result; memset(&result,0,sizeof(result)); } else { free(out->directory); out->directory=result.directory; result.directory=NULL; xideRecoveryUnlock(&result.guard); }
    xideRecoveryBackupClear(&result); return okay;
}
