#include "platform/recovery_store.h"
#include "platform/state_file.h"
#include "platform/recovery_backup.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#define MAX_FILES 20000u
#define GRACE_TICKS (UINT64_C(7)*24*60*60*10000000)
typedef struct names { char** items; size_t count,capacity; } names;
typedef struct candidate { wchar_t name[128]; } candidate;
static int fail(xide_model_error* e,int code,const char* text)
{ if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",text); } return 0; }
static wchar_t* wide(const char* s)
{
    return xidePathNativeWide(s);
}
static char* join(const char* directory,const char* leaf)
{
    size_t n=strlen(directory),m=strlen(leaf); char* p=malloc(n+m+2);
    if(p) { memcpy(p,directory,n); p[n]='/'; memcpy(p+n+1,leaf,m+1); } return p;
}
static int leafValid(const char* s)
{
    if(!s || !*s || strlen(s)>120 || !strcmp(s,".") || !strcmp(s,"..")) return 0;
    for(;*s;++s) if(!isalnum((unsigned char)*s) && *s!='-' && *s!='_' && *s!='.') return 0;
    return 1;
}
static int ordinary(const wchar_t* path,int directory,int allowMissing)
{
    DWORD attrs=GetFileAttributesW(path);
    if(attrs==INVALID_FILE_ATTRIBUTES) { DWORD e=GetLastError(); return allowMissing && (e==ERROR_FILE_NOT_FOUND || e==ERROR_PATH_NOT_FOUND); }
    return !(attrs&FILE_ATTRIBUTE_REPARSE_POINT) && !!(attrs&FILE_ATTRIBUTE_DIRECTORY)==directory;
}
static HANDLE pinDirectory(const wchar_t* path)
{
    HANDLE h=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    BY_HANDLE_FILE_INFORMATION info={0};
    if(h!=INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(h,&info) || !(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))) { CloseHandle(h); h=INVALID_HANDLE_VALUE; }
    return h;
}
void xideRecoveryUnlock(xide_recovery_guard* g)
{
    if(!g) return;
    if(g->lockHandle) CloseHandle(g->lockHandle);
    if(g->recoveryHandle) CloseHandle(g->recoveryHandle);
    if(g->directoryHandle) CloseHandle(g->directoryHandle);
    free(g->directory); memset(g,0,sizeof(*g));
}
int xideRecoveryLock(xide_recovery_guard* g,const char* directory,xide_model_error* e)
{
    if(e) memset(e,0,sizeof(*e));
    if(!g || !directory || g->lockHandle) return fail(e,XIDE_MODEL_INVALID,"Invalid recovery lock arguments.");
    xide_recovery_guard next={0}; wchar_t *root=NULL,*body=NULL,*mutex=NULL; char *p=NULL; int okay=0;
    /* Resolve only the caller's scope, never a child that may be a reparse point. */
    next.directory=xidePathResolve(NULL,directory);
    char* native=next.directory ? xidePathNativeUtf8(next.directory) : NULL;
    int created=native && xrtDirCreateAll(native); free(native);
    if(!created) { fail(e,XIDE_MODEL_IO,"Cannot create session directory."); goto done; }
    root=wide(next.directory); HANDLE h=root ? pinDirectory(root) : INVALID_HANDLE_VALUE;
    if(h==INVALID_HANDLE_VALUE) { fail(e,XIDE_MODEL_IO,"Cannot pin the session directory."); goto done; } next.directoryHandle=h;
    p=join(next.directory,"checkpoint.lock"); mutex=wide(p); free(p); p=NULL;
    if(!mutex || !ordinary(mutex,0,1)) { fail(e,XIDE_MODEL_INVALID,"Checkpoint lock is not an ordinary file."); goto done; }
    h=CreateFileW(mutex,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    BY_HANDLE_FILE_INFORMATION lockInfo={0};
    if(h==INVALID_HANDLE_VALUE) { fail(e,XIDE_MODEL_CONFLICT,"Another IDE is writing or maintaining this checkpoint."); goto done; }
    next.lockHandle=h;
    if(!GetFileInformationByHandle(h,&lockInfo) || (lockInfo.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || lockInfo.nNumberOfLinks!=1) { fail(e,XIDE_MODEL_INVALID,"Unsafe checkpoint lock file; state is preserved."); goto done; }
    p=join(next.directory,"recovery"); body=wide(p); free(p); p=NULL;
    if(!body || (!CreateDirectoryW(body,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS) || !ordinary(body,1,0)) { fail(e,XIDE_MODEL_IO,"Recovery storage is not an ordinary directory."); goto done; }
    h=pinDirectory(body); if(h==INVALID_HANDLE_VALUE) { fail(e,XIDE_MODEL_IO,"Cannot pin recovery storage."); goto done; } next.recoveryHandle=h;
    *g=next; memset(&next,0,sizeof(next)); okay=1;
done:
    free(p); free(root); free(body); free(mutex); xideRecoveryUnlock(&next); return okay;
}
char* xideRecoveryPath(const char* directory,const char* relative,xide_model_error* e)
{
    const char* leaf=relative; int recovery=relative && !strncmp(relative,"recovery/",9);
    if(recovery) leaf+=9;
    if(!directory || !leafValid(leaf)) { fail(e,XIDE_MODEL_INVALID,"Unsafe recovery reference; files are preserved."); return NULL; }
    char* base=recovery ? join(directory,"recovery") : NULL;
    wchar_t* parent=wide(recovery ? base : directory);
    char* path=parent && ordinary(parent,1,0) ? join(recovery ? base : directory,leaf) : NULL;
    wchar_t* child=wide(path);
    if(!child || !ordinary(child,0,1)) { free(path); path=NULL; fail(e,XIDE_MODEL_INVALID,"Recovery/state child is not an ordinary file."); }
    free(child); free(parent); free(base); return path;
}
static int add(names* n,const char* s)
{
    if(n->count>=MAX_FILES*4u) return 0;
    if(n->count==n->capacity) { size_t cap=n->capacity ? n->capacity*2 : 64; char** p=realloc(n->items,cap*sizeof(*p)); if(!p) return 0; n->items=p; n->capacity=cap; }
    size_t length=strlen(s)+1; char* p=malloc(length); if(!p) return 0; memcpy(p,s,length); n->items[n->count++]=p; return 1;
}
static void clearNames(names* n) { for(size_t i=0;i<n->count;++i) free(n->items[i]); free(n->items); }
static int compare(const void* a,const void* b) { return _stricmp(*(const char* const*)a,*(const char* const*)b); }
static xvalue* get(const xvalue* root,const char* key) { return root ? xrtValueObjectGet(root,(xstrview){key,strlen(key)}) : NULL; }
static const char* text(const xvalue* value) { xstrview s={0}; return xrtValueGetString(value,&s) && !memchr(s.Data,0,s.Size) ? s.Data : NULL; }
static int refs(const xvalue* root,names* n)
{
    const xvalue* docs=get(root,"openDocuments");
    if(docs && (!xrtValueIs(docs,XVALUE_ARRAY) || xrtValueCount(docs)>10000)) return 0;
    for(size_t i=0;i<xrtValueCount(docs);++i) {
        const xvalue* d=xrtValueArrayGet(docs,i); if(!xrtValueIs(d,XVALUE_OBJECT)) return 0;
        const char* fields[]={"recoveryText","recoveryBaseline"};
        if(!!get(d,fields[0])!=!!get(d,fields[1])) return 0;
        for(int k=0;k<2;++k) if(get(d,fields[k])) {
            const char* s=text(get(d,fields[k]));
            if(!s || strncmp(s,"recovery/",9) || !leafValid(s+9) || !add(n,s+9)) return 0;
        }
    }
    return 1;
}
int xideRecoveryRetain(xide_recovery_guard* g,const xvalue* root,uint64_t now,xide_model_error* e)
{
    if(!g || !g->lockHandle || !g->recoveryHandle) return fail(e,XIDE_MODEL_INVALID,"Retention requires the whole-checkpoint lock.");
    names list={0}; int okay=refs(root,&list);
    if(!okay) { clearNames(&list); return fail(e,XIDE_MODEL_INVALID,"Cannot protect invalid recovery references."); }
    qsort(list.items,list.count,sizeof(*list.items),compare);
    FILETIME time={(DWORD)now,(DWORD)(now>>32)};
    for(size_t i=0;okay && i<list.count;++i) {
        if(i && !_stricmp(list.items[i],list.items[i-1])) continue;
        char ref[140]; snprintf(ref,sizeof(ref),"recovery/%s",list.items[i]);
        char* path=xideRecoveryPath(g->directory,ref,e); wchar_t* w=wide(path); free(path);
        HANDLE h=w ? CreateFileW(w,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE;
        DWORD openError=GetLastError(); BY_HANDLE_FILE_INFORMATION info={0};
        if(h==INVALID_HANDLE_VALUE) {
            int missing=w && (openError==ERROR_FILE_NOT_FOUND || openError==ERROR_PATH_NOT_FOUND);
            free(w);
            if(missing) continue;
            okay=0; break;
        }
        okay=GetFileInformationByHandle(h,&info) && !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
        int permanent=okay && ((info.dwFileAttributes&FILE_ATTRIBUTE_READONLY) || info.nNumberOfLinks!=1);
        CloseHandle(h);
        uint64_t written=((uint64_t)info.ftLastWriteTime.dwHighDateTime<<32)|info.ftLastWriteTime.dwLowDateTime;
        if(okay && !permanent && written<now) {
            h=CreateFileW(w,FILE_READ_ATTRIBUTES|FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
            okay=h!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(h,&info) && !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
            if(okay && !(info.dwFileAttributes&FILE_ATTRIBUTE_READONLY) && info.nNumberOfLinks==1) okay=SetFileTime(h,NULL,NULL,&time);
            if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
        }
        free(w);
    }
    clearNames(&list);
    if(!okay && (!e || !e->message[0])) fail(e,XIDE_MODEL_IO,"Cannot retain recovery references; the checkpoint was not replaced.");
    return okay;
}
static int loadRefs(xide_recovery_guard* g,const char* leaf,int required,int (*validate)(const xvalue*),names* n,xide_model_error* e)
{
    char* p=xideRecoveryPath(g->directory,leaf,e); if(!p) return 0;
    xide_state_file file={0}; int okay=xideStateOpen(&file,p,"xlang-ide-session-1",e); free(p);
    if(okay && (required || file.existed)) okay=file.existed && (!validate || validate(file.source)) && refs(file.source,n);
    if(!okay && (!e || !e->message[0])) fail(e,XIDE_MODEL_INVALID,"Recovery references are damaged; cleanup is disabled.");
    xideStateClear(&file); return okay;
}
static int hex(wchar_t c) { return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F'); }
static int uuid(const wchar_t* s)
{ for(int i=0;i<36;++i) if(i==8 || i==13 || i==18 || i==23 ? s[i]!='-' : !hex(s[i])) return 0; return 1; }
static int managed(const wchar_t* s)
{
    size_t n=wcslen(s); int body=n>=5 && !_wcsicmp(s+n-5,L".text"),base=n>=5 && !_wcsicmp(s+n-5,L".base");
    if(!body && !base) return 0;
    if(n==46 && !wcsncmp(s,L"snap-",5)) return uuid(s+5);
    if(n!=62 || wcsncmp(s,L"doc-",4) || !uuid(s+4) || s[40]!='-') return 0;
    for(int i=41;i<57;++i) if(!hex(s[i])) return 0;
    return 1;
}
static uint64_t ticks(FILETIME t) { return ((uint64_t)t.dwHighDateTime<<32)|t.dwLowDateTime; }
static int expired(FILETIME created,FILETIME written,uint64_t now)
{ uint64_t a=ticks(created),b=ticks(written),last=a>b ? a : b; return now>=last && now-last>=GRACE_TICKS; }
int xideRecoveryCollect(xide_recovery_guard* g,uint64_t now,int (*validate)(const xvalue*),xide_recovery_stats* out,xide_model_error* e)
{
    if(e) memset(e,0,sizeof(*e));
    if(out) memset(out,0,sizeof(*out));
    if(!g || !g->lockHandle || !g->recoveryHandle) return fail(e,XIDE_MODEL_INVALID,"Cleanup requires the whole-checkpoint lock.");
    /* A pending rename owns independent pre/post checkpoint bodies. Keep the
       whole recovery store until that transaction is saved and retired. */
    char* journalPath=xideRecoveryPath(g->directory,"rename-journal.json",e);
    if(!journalPath) return 0;
    if(xidePathExists(journalPath)) {
        xide_state_file journal={0}; int retired=xideStateOpen(&journal,journalPath,"xlang-ide-rename-journal-1",e);
        xstrview state={0}; retired=retired && xrtValueGetString(xrtValueObjectGet(journal.source,(xstrview){"state",5}),&state) && state.Size==7 && !memcmp(state.Data,"retired",7);
        xideStateClear(&journal); free(journalPath);
        if(!retired) return fail(e,XIDE_MODEL_CONFLICT,"Pending or damaged rename transaction protects all recovery files.");
    } else free(journalPath);
    names refsList={0}; candidate* pending=NULL; size_t count=0; xide_recovery_stats stats={0}; int okay=0;
    HANDLE search=INVALID_HANDLE_VALUE; char *directory=NULL,*pattern=NULL; wchar_t *widePattern=NULL;
    if(!loadRefs(g,"session.json",1,validate,&refsList,e) || !loadRefs(g,"previous-session.json",0,validate,&refsList,e)) goto done;
    if(!xideRecoveryBackupsCheck(g,e)) goto done;
    qsort(refsList.items,refsList.count,sizeof(*refsList.items),compare); stats.referenced=refsList.count;
    directory=join(g->directory,"recovery"); pattern=directory ? join(directory,"*") : NULL; widePattern=wide(pattern);
    pending=calloc(MAX_FILES,sizeof(*pending)); if(!widePattern || !pending) { fail(e,XIDE_MODEL_MEMORY,"Cannot inspect recovery storage."); goto done; }
    WIN32_FIND_DATAW found; search=FindFirstFileW(widePattern,&found);
    if(search==INVALID_HANDLE_VALUE) { if(GetLastError()==ERROR_FILE_NOT_FOUND) { okay=1; goto done; } fail(e,XIDE_MODEL_IO,"Cannot list recovery files; cleanup is disabled."); goto done; }
    do {
        if(!wcscmp(found.cFileName,L".") || !wcscmp(found.cFileName,L"..")) continue;
        if(++stats.scanned>MAX_FILES) { fail(e,XIDE_MODEL_INVALID,"Recovery storage exceeds the bounded cleanup limit; nothing was deleted."); goto done; }
        if((found.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY)) || !managed(found.cFileName)) { ++stats.skipped; continue; }
        char name[128]; size_t len=wcslen(found.cFileName); for(size_t i=0;i<=len;++i) name[i]=(char)found.cFileName[i]; const char* key=name;
        if((refsList.count && bsearch(&key,refsList.items,refsList.count,sizeof(*refsList.items),compare)) || !expired(found.ftCreationTime,found.ftLastWriteTime,now)) { ++stats.retained; continue; }
        wcscpy(pending[count++].name,found.cFileName);
    } while(FindNextFileW(search,&found));
    if(GetLastError()!=ERROR_NO_MORE_FILES) { fail(e,XIDE_MODEL_IO,"Recovery enumeration failed; nothing was deleted."); goto done; }
    FindClose(search); search=INVALID_HANDLE_VALUE;
    for(size_t i=0;i<count;++i) {
        char name[128]; size_t len=wcslen(pending[i].name); for(size_t k=0;k<=len;++k) name[k]=(char)pending[i].name[k];
        char* path=join(directory,name); wchar_t* w=wide(path); free(path);
        HANDLE h=w ? CreateFileW(w,DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; free(w);
        BY_HANDLE_FILE_INFORMATION info={0}; FILE_DISPOSITION_INFO disposition={TRUE};
        if(h!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(h,&info) && !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY)) && info.nNumberOfLinks==1 && expired(info.ftCreationTime,info.ftLastWriteTime,now) && SetFileInformationByHandle(h,FileDispositionInfo,&disposition,sizeof(disposition))) {
            ++stats.removed; stats.removedBytes+=((uint64_t)info.nFileSizeHigh<<32)|info.nFileSizeLow;
        } else ++stats.skipped;
        if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    okay=1;
done:
    if(search!=INVALID_HANDLE_VALUE) FindClose(search);
    free(directory); free(pattern); free(widePattern); free(pending); clearNames(&refsList);
    if(out) *out=stats;
    return okay;
}
