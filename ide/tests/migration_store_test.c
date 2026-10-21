#define _WIN32_WINNT 0x0601
#include "services/migration_journal.h"
#include "services/session.h"
#include "services/recovery.h"
#include "platform/file_io.h"
#include "platform/path.h"
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static xide_model_error error;
static int checks,cases;
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"migration_store_test:%d: %s; %s\n",__LINE__,#v,error.message); ExitProcess(81); } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* o,const char* k) { return xrtValueObjectGet(o,sv(k)); }
static const char* jtext(const xvalue* o,const char* k) { xstrview v={0}; return xrtValueGetString(get(o,k),&v) ? v.Data : ""; }
static int put(xvalue* o,const char* k,const char* v) { return xrtValueObjectSetNew(o,sv(k),xrtValueString(sv(v))); }
static char* copyText(const char* s) { size_t n=strlen(s)+1; char* result=malloc(n); if(result) memcpy(result,s,n); return result; }
static int writeText(const char* path,const char* s) { return xideIoWriteAtomic(path,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int writeJson(const char* path,const xvalue* value)
{ size_t n=0; char* data=xrtJsonStringify(value,true,&n); int okay=data && xideIoWriteAtomic(path,(xbytesview){(unsigned char*)data,n}); xrtFree(data); return okay; }
static uint64_t now(void) { FILETIME time; GetSystemTimeAsFileTime(&time); return ((uint64_t)time.dwHighDateTime<<32)|time.dwLowDateTime; }
static int junction(const char* path,const char* target)
{
 wchar_t* p=xidePathNativeWide(path),*t=xidePathNativeWide(target); wchar_t sub[4100]; if(!p || !t) { free(p); free(t); return 0; }
 const wchar_t* plain=!wcsncmp(t,L"\\\\?\\",4) ? t+4 : t; swprintf(sub,4100,L"\\??\\%ls",plain); size_t a=wcslen(sub),b=wcslen(plain);
 struct { DWORD tag; WORD bytes,reserved,subOffset,subLength,printOffset,printLength; wchar_t buffer[8200]; } data={0};
 data.tag=IO_REPARSE_TAG_MOUNT_POINT; data.subLength=(WORD)(a*2); data.printOffset=(WORD)((a+1)*2); data.printLength=(WORD)(b*2); data.bytes=(WORD)(8+(a+b+2)*2);
 memcpy(data.buffer,sub,(a+1)*2); memcpy(data.buffer+a+1,plain,(b+1)*2);
 int okay=CreateDirectoryW(p,NULL); HANDLE handle=okay ? CreateFileW(p,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; DWORD transferred=0;
 okay=handle!=INVALID_HANDLE_VALUE && DeviceIoControl(handle,FSCTL_SET_REPARSE_POINT,&data,data.bytes+8,NULL,0,&transferred,NULL);
 if(handle!=INVALID_HANDLE_VALUE) CloseHandle(handle);
 free(p); free(t); return okay;
}
#define DAY (UINT64_C(24)*60*60*10000000)
typedef struct fixture {
 xide_app app; xide_migration_journal* journal; xvalue *checkpoint,*layout;
 char *root,*global,*scope,*marker,*store,*source,*entry,*c,*manifest,*workspace;
 xide_file_snapshot sourceSnapshot;
} fixture;
static char* markerPath(const char* global,const char* owner)
{
 char* root=xidePathResolveLexical(global,"migrations"),*pattern=xidePathResolveLexical(root,"*"); wchar_t* wide=xidePathNativeWide(pattern); WIN32_FIND_DATAW data={0}; HANDLE find=FindFirstFileW(wide,&data); char* result=NULL;
 if(find!=INVALID_HANDLE_VALUE) { do {
  if(wcslen(data.cFileName)!=16 || wcsspn(data.cFileName,L"0123456789abcdef")!=16 || !(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) continue;
  char leaf[17]={0}; for(size_t i=0;i<16;++i) leaf[i]=(char)data.cFileName[i]; char* child=xidePathResolveLexical(root,leaf); result=xidePathResolveLexical(child,"migration-journal.json"); free(child);
  char* native=result ? xidePathNativeUtf8(result) : NULL; xvalue* value=native ? xrtJsonParseFile(native) : NULL; free(native); int match=value && !strcmp(jtext(value,"ownerAfter"),owner); xrtValueRelease(value);
  if(match) break;
  free(result); result=NULL;
 } while(FindNextFileW(find,&data)); FindClose(find); }
 free(root); free(pattern); free(wide); return result;
}
static xvalue* marker(const fixture* f) { char* native=xidePathNativeUtf8(f->marker); xvalue* root=xrtJsonParseFile(native); free(native); CHECK(root); return root; }
static char* body(const fixture* f,size_t index)
{ xvalue* root=marker(f); const xvalue* rows=get(root,"ownedBodies"); char* path=xidePathResolveLexical(f->store,jtext(xrtValueArrayGet(rows,index),"ref")); xrtValueRelease(root); CHECK(path); return path; }
static size_t owned(const fixture* f) { xvalue* root=marker(f); size_t count=xrtValueCount(get(root,"ownedBodies")); xrtValueRelease(root); return count; }
static void sourcePreserved(const fixture* f)
{ xide_file_snapshot after={0}; CHECK(xideFileSnapshotInspect(f->c,&after,&error) && xideFileSnapshotEqual(&after,&f->sourceSnapshot)); char* path=xidePathResolveLexical(f->scope,"recovery/c.text"); CHECK(xideIoExists(path)); free(path); }
static void fixtureClear(fixture* f)
{
 xideRenameJournalDestroy(f->journal); xrtValueRelease(f->checkpoint); xrtValueRelease(f->layout); xideWorkspaceClear(&f->app.workspaceModel);
 free(f->root); free(f->global); free(f->scope); free(f->marker); free(f->store); free(f->source); free(f->entry); free(f->c); free(f->manifest); free(f->workspace); memset(f,0,sizeof(*f));
}
static void fixtureMakeIn(fixture* f,const char* base,const char* name,const char* shared)
{
 f->root=xidePathResolveLexical(base,name); f->global=shared ? copyText(shared) : xidePathResolveLexical(f->root,"global"); f->scope=xidePathResolveLexical(f->root,"checkpoint"); f->source=xidePathResolveLexical(f->root,"项目😀"); f->entry=xidePathResolveLexical(f->source,"main.xl"); f->c=xidePathResolveLexical(f->source,"native.c"); f->manifest=xidePathResolveLexical(f->source,"app.xlp"); f->workspace=xidePathResolveLexical(f->root,"work.xlw");
 CHECK(f->workspace && xideIoCreateDirectories(f->source) && xideIoCreateDirectories(f->global));
 CHECK(writeText(f->entry,"func main(...) as int\n    return 0\nend\n") && writeText(f->c,"int answer = 42;\n") && xideFileSnapshotInspect(f->c,&f->sourceSnapshot,&error));
 xide_project project={0}; CHECK(xideProjectCreate(&project,f->manifest,"Migration",f->entry,&error) && xideProjectAddFile(&project,f->c,"c",&error) && xideProjectSave(&project,NULL,&error)); xideProjectClear(&project);
 CHECK(xideWorkspaceCreate(&f->app.workspaceModel,"Workspace",&error) && xideWorkspaceAddProject(&f->app.workspaceModel,f->manifest,"project",&error) && xideWorkspaceSave(&f->app.workspaceModel,f->workspace,&error));
 xide_project* p=&f->app.workspaceModel.projects[0]; CHECK(xideProjectSetMetadata(p,p->name,"unsaved project properties",p->kind,&error));
 f->checkpoint=xrtJsonParse(sv("{\"format\":\"xlang-ide-session-1\",\"ownerKind\":\"workspace\",\"ownerPath\":\"work.xlw\",\"layoutGeneration\":\"generation-A\",\"openDocuments\":[{\"id\":\"doc-c\",\"path\":\"native.c\",\"language\":\"c\",\"dirty\":true,\"recoveryText\":\"recovery/c.text\",\"recoveryBaseline\":\"recovery/c.base\"},{\"id\":\"doc-xl\",\"path\":\"main.xl\",\"language\":\"xlang\",\"dirty\":true,\"recoveryText\":\"recovery/xl.text\",\"recoveryBaseline\":\"recovery/xl.base\"}]}"));
 f->layout=xrtJsonParse(sv("{\"format\":\"xlang-ide-layout-1\",\"states\":[]}"));
 CHECK(f->checkpoint && f->layout && put(xrtValueArrayGet(get(f->checkpoint,"openDocuments"),0),"path",f->c) && put(xrtValueArrayGet(get(f->checkpoint,"openDocuments"),1),"path",f->entry) && put(f->checkpoint,"ownerPath",f->workspace));
 xide_recovery_guard guard={0}; CHECK(xideRecoveryLock(&guard,f->scope,&error)); xideRecoveryUnlock(&guard);
 const char *refs[]={"recovery/c.text","recovery/c.base","recovery/xl.text","recovery/xl.base"},*texts[]={"// C draft A\nint answer = 42;\n","int answer = 42;\n","// xlang draft\nfunc main(...) as int\n    return 0\nend\n","func main(...) as int\n    return 0\nend\n"};
 for(size_t i=0;i<4;++i) { char* path=xidePathResolveLexical(f->scope,refs[i]); CHECK(path && writeText(path,texts[i])); free(path); }
 char* target=xidePathResolveLexical(f->root,"Moved项目😀"); xide_project_migration* plan=xideProjectMigrationPrepare(&f->app.workspaceModel,p->id,target,XIDE_MIGRATE_DIRECTORY,&error); CHECK(plan);
 f->journal=xideMigrationJournalPrepare(&f->app,f->global,f->scope,f->root,f->checkpoint,f->layout,plan,&error); CHECK(f->journal && xideProjectMigrationCommit(plan,&error));
 char *mappedC=xideProjectMigrationMapPath(plan,f->c),*mappedXL=xideProjectMigrationMapPath(plan,f->entry); CHECK(mappedC && mappedXL && put(xrtValueArrayGet(get(f->checkpoint,"openDocuments"),0),"path",mappedC) && put(xrtValueArrayGet(get(f->checkpoint,"openDocuments"),1),"path",mappedXL));
 free(f->c); free(f->entry); f->c=mappedC; f->entry=mappedXL; xideProjectMigrationDestroy(plan); free(target);
 CHECK(xideMigrationJournalCheckpoint(f->journal,&f->app,f->scope,f->root,f->checkpoint,f->layout,&error)); f->marker=markerPath(f->global,f->workspace); f->store=f->marker ? xidePathDirectory(f->marker) : NULL; CHECK(f->store && owned(f)==4);
}
static void fixtureMake(fixture* f,const char* base,const char* name) { fixtureMakeIn(f,base,name,NULL); }
static void ageBodies(fixture* f)
{
 uint64_t old=now()-20*DAY; FILETIME time={(DWORD)old,(DWORD)(old>>32)}; char number[32]; snprintf(number,sizeof(number),"%llu",(unsigned long long)old); xvalue* root=marker(f);
 const char* keys[]={"ownedBodies","blobs"};
 for(size_t k=0;k<2;++k) for(size_t i=0;i<xrtValueCount(get(root,keys[k]));++i) {
  xvalue* row=xrtValueArrayGet(get(root,keys[k]),i); char* path=xidePathResolveLexical(f->store,jtext(row,"ref")); wchar_t* wide=xidePathNativeWide(path); HANDLE file=CreateFileW(wide,FILE_WRITE_ATTRIBUTES,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  CHECK(file!=INVALID_HANDLE_VALUE && SetFileTime(file,&time,NULL,&time) && put(row,"lastWrite",number)); CloseHandle(file); free(path); free(wide);
 }
 CHECK(put(root,"retiredAt",number) && writeJson(f->marker,root)); xrtValueRelease(root);
}
static void changeC(fixture* f,const char* content)
{ char* path=xidePathResolveLexical(f->scope,"recovery/c.text"); CHECK(path && writeText(path,content) && xideMigrationJournalCheckpoint(f->journal,&f->app,f->scope,f->root,f->checkpoint,f->layout,&error)); free(path); }
static void retire(fixture* f)
{ CHECK(xideProjectSave(&f->app.workspaceModel.projects[0],NULL,&error) && xideWorkspaceSave(&f->app.workspaceModel,NULL,&error) && xideMigrationJournalCheckpoint(f->journal,&f->app,f->scope,f->root,f->checkpoint,f->layout,&error)); xvalue* root=marker(f); CHECK(!strcmp(jtext(root,"state"),"retired") && *jtext(root,"retiredAt")); xrtValueRelease(root); }
static xide_recovery_stats maintain(fixture* f,uint64_t when,int expected)
{ xide_recovery_stats stats={0}; CHECK(xideMigrationJournalMaintain(f->global,when,NULL,&stats,&error)==expected); sourcePreserved(f); return stats; }
static void removeFile(const fixture* f,const char* path)
{ CHECK(!strncmp(path,f->root,strlen(f->root)) && path[strlen(f->root)]=='/'); wchar_t* wide=xidePathNativeWide(path); CHECK(wide && DeleteFileW(wide)); free(wide); }
static void pass(const char* name) { ++cases; printf("Migration store case %s passed\n",name); }

int main(int argc,char** argv)
{
 if(argc!=2) return 2;
 char* base=xidePathResolveLexical(NULL,argv[1]); CHECK(base && xideIoCreateDirectories(base)); fixture f={0}; xide_recovery_stats stats={0}; uint64_t future=now()+20*DAY;
 fixtureMake(&f,base,"pending"); stats=maintain(&f,future,1); CHECK(!stats.removed && owned(&f)==4); pass("pending"); fixtureClear(&f);
 fixtureMake(&f,base,"grace"); retire(&f); stats=maintain(&f,now(),1); CHECK(!stats.removed && owned(&f)==4); stats=maintain(&f,now()-DAY,1); CHECK(!stats.removed); stats=maintain(&f,now()+6*DAY,1); CHECK(!stats.removed); stats=maintain(&f,future,1); CHECK(stats.removed==4 && owned(&f)==0); pass("retired-seven-day-grace"); fixtureClear(&f);
 fixtureMake(&f,base,"obsolete"); changeC(&f,"// C draft B\nint answer = 43;\n"); CHECK(owned(&f)==5); changeC(&f,"// C draft A\nint answer = 42;\n"); CHECK(owned(&f)==5); retire(&f); stats=maintain(&f,future,1); CHECK(stats.removed==5 && owned(&f)==0); pass("obsolete-body-reuse-and-cleanup"); fixtureClear(&f);
 fixtureMake(&f,base,"readonly"); retire(&f); char* path=body(&f,0); wchar_t* wide=xidePathNativeWide(path); DWORD attributes=GetFileAttributesW(wide); CHECK(SetFileAttributesW(wide,attributes|FILE_ATTRIBUTE_READONLY)); stats=maintain(&f,future,1); CHECK(stats.removed==3 && owned(&f)==1 && xideIoExists(path)); CHECK(SetFileAttributesW(wide,attributes)); stats=maintain(&f,future,1); CHECK(stats.removed==1 && owned(&f)==0); free(wide); free(path); pass("readonly-preservation"); fixtureClear(&f);
 fixtureMake(&f,base,"hardlink"); retire(&f); path=body(&f,0); char* alias=xidePathResolveLexical(f.root,"outside-alias.body"); wide=xidePathNativeWide(path); wchar_t* aliasWide=xidePathNativeWide(alias); CHECK(CreateHardLinkW(aliasWide,wide,NULL)); stats=maintain(&f,future,1); CHECK(stats.removed==3 && owned(&f)==1 && xideIoExists(alias) && xideIoExists(path)); removeFile(&f,alias); stats=maintain(&f,future,1); CHECK(stats.removed==1); free(path); free(alias); free(wide); free(aliasWide); pass("hardlink-preservation"); fixtureClear(&f);
 fixtureMake(&f,base,"busy"); retire(&f); path=body(&f,0); wide=xidePathNativeWide(path); HANDLE lock=CreateFileW(wide,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); CHECK(lock!=INVALID_HANDLE_VALUE); stats=maintain(&f,future,1); CHECK(stats.removed==3 && owned(&f)==1); CloseHandle(lock); stats=maintain(&f,future,1); CHECK(stats.removed==1); free(path); free(wide); pass("sharing-lock-preservation"); fixtureClear(&f);
 fixtureMake(&f,base,"replacement"); retire(&f); path=body(&f,0); alias=xidePathResolveLexical(f.root,"original-owned.body"); CHECK(xideIoMove(path,alias,0) && xideIoCopy(alias,path,0)); stats=maintain(&f,future,1); CHECK(stats.removed==3 && owned(&f)==1 && xideIoExists(alias) && xideIoExists(path)); free(path); free(alias); pass("same-content-replacement-preservation"); fixtureClear(&f);
 fixtureMake(&f,base,"mutation"); retire(&f); path=body(&f,0); wide=xidePathNativeWide(path); lock=CreateFileW(wide,GENERIC_READ|GENERIC_WRITE|FILE_WRITE_ATTRIBUTES,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); CHECK(lock!=INVALID_HANDLE_VALUE); FILETIME time; CHECK(GetFileTime(lock,NULL,NULL,&time)); unsigned char ch=0; DWORD read=0,written=0; CHECK(ReadFile(lock,&ch,1,&read,NULL) && read==1); LARGE_INTEGER start={0}; ch^=1; CHECK(SetFilePointerEx(lock,start,NULL,FILE_BEGIN) && WriteFile(lock,&ch,1,&written,NULL) && written==1 && SetFileTime(lock,NULL,NULL,&time)); CloseHandle(lock); stats=maintain(&f,future,1); CHECK(stats.removed==3 && owned(&f)==1 && xideIoExists(path)); free(path); free(wide); pass("same-metadata-content-mutation-preservation"); fixtureClear(&f);
 fixtureMake(&f,base,"missing"); retire(&f); path=body(&f,0); removeFile(&f,path); stats=maintain(&f,future,1); CHECK(stats.removed==3 && owned(&f)==0); free(path); pass("interrupted-cleanup-missing-row-repair"); fixtureClear(&f);
 fixtureMake(&f,base,"damaged"); retire(&f); CHECK(writeText(f.marker,"{damaged")); stats=maintain(&f,future,1); CHECK(!stats.removed); pass("damaged-marker-preservation"); fixtureClear(&f);
 fixtureMake(&f,base,"inventory"); retire(&f); xvalue* root=marker(&f); xvalue* duplicate=xrtValueDeepClone(xrtValueArrayGet(get(root,"ownedBodies"),0)); CHECK(duplicate && put(duplicate,"sha256","ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") && xrtValueArrayAppendNew(get(root,"ownedBodies"),duplicate) && writeJson(f.marker,root)); xrtValueRelease(root); stats=maintain(&f,future,1); CHECK(!stats.removed && owned(&f)==5); pass("conflicting-ownership-preservation"); fixtureClear(&f);
 fixtureMake(&f,base,"legacy"); changeC(&f,"// C draft B\nint answer = 43;\n"); retire(&f); root=marker(&f); xrtValueObjectRemove(root,sv("ownedBodies")); xrtValueObjectRemove(root,sv("retiredAt")); CHECK(writeJson(f.marker,root)); xrtValueRelease(root); stats=maintain(&f,future,1); CHECK(stats.removed==4 && owned(&f)==0); pass("legacy-known-body-adoption"); fixtureClear(&f);
 fixtureMake(&f,base,"unknown"); retire(&f); path=xidePathResolveLexical(f.store,"recovery/rename-ffffffffffffffffffffffffffffffff.body"); CHECK(writeText(path,"unknown content")); alias=xidePathResolveLexical(f.global,"migrations/ffffffffffffffff"); CHECK(xideIoCreateDirectories(alias)); char* untouched=xidePathResolveLexical(alias,"unknown.bin"); CHECK(writeText(untouched,"unknown store")); stats=maintain(&f,future,1); CHECK(stats.removed==4 && xideIoExists(path) && xideIoExists(untouched)); char* unexpected=xidePathResolveLexical(alias,"checkpoint.lock"); CHECK(!xideIoExists(unexpected)); free(path); free(alias); free(untouched); free(unexpected); pass("unknown-file-and-store-preservation"); fixtureClear(&f);
 fixtureMake(&f,base,"publish-failure"); retire(&f); wide=xidePathNativeWide(f.marker); lock=CreateFileW(wide,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); CHECK(lock!=INVALID_HANDLE_VALUE); stats=maintain(&f,future,0); CHECK(stats.removed==4 && owned(&f)==4); CloseHandle(lock); stats=maintain(&f,future,1); CHECK(!stats.removed && owned(&f)==0); free(wide); pass("post-delete-publication-failure-retry"); fixtureClear(&f);
 fixtureMake(&f,base,"cancel"); retire(&f); volatile LONG stop=1; CHECK(xideMigrationJournalMaintain(f.global,future,&stop,&stats,&error) && !stats.removed && owned(&f)==4); pass("cancel-before-delete"); fixtureClear(&f);
 fixtureMake(&f,base,"reparse"); retire(&f); path=body(&f,0); alias=xidePathResolveLexical(f.root,"reparse-target.body"); CHECK(xideIoMove(path,alias,0) && junction(path,f.root)); stats=maintain(&f,future,1); wide=xidePathNativeWide(path); attributes=GetFileAttributesW(wide); CHECK(stats.removed==3 && owned(&f)==1 && xideIoExists(alias) && attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT)); free(wide); free(path); free(alias); pass("reparse-body-preservation"); fixtureClear(&f);
 fixtureMake(&f,base,"background"); retire(&f); ageBodies(&f); xide_recovery_service* service=xideRecoveryServiceCreate(NULL); CHECK(service && xideRecoveryServiceStart(service,XIDE_RECOVERY_MIGRATIONS,f.global,&error)); xide_recovery_result result={0}; uint64_t deadline=GetTickCount64()+15000; while(!xideRecoveryServiceTake(service,&result) && GetTickCount64()<deadline) Sleep(1); CHECK(result.okay && result.migrationStats.removed==4 && owned(&f)==0); xideRecoveryResultClear(&result,0); xideRecoveryServiceDestroy(service); sourcePreserved(&f); pass("real-background-worker"); fixtureClear(&f);
 fixtureMake(&f,base,"file-budget"); for(int i=0;i<300;++i) { char text[80]; snprintf(text,sizeof(text),"// C generation %d\nint answer = %d;\n",i,i); changeC(&f,text); } CHECK(owned(&f)==304); retire(&f); size_t removed=0; int rounds=0; while(owned(&f) && rounds<8) { stats=maintain(&f,future,1); CHECK(stats.removed<=128); removed+=stats.removed; ++rounds; } CHECK(removed==304 && rounds==3 && owned(&f)==0); pass("bounded-file-cursor-progress"); fixtureClear(&f);
 fixtureMake(&f,base,"byte-budget"); char* large=malloc(16u*1024u*1024u+1); CHECK(large); memset(large,'a',16u*1024u*1024u); large[16u*1024u*1024u]=0; for(int i=0;i<5;++i) { large[0]=(char)('0'+i); changeC(&f,large); } free(large); CHECK(owned(&f)==9); retire(&f); removed=0; rounds=0; while(owned(&f) && rounds<8) { stats=maintain(&f,future,1); CHECK(stats.removedBytes<=64u*1024u*1024u); removed+=stats.removed; ++rounds; } CHECK(removed==9 && rounds>=2 && owned(&f)==0); pass("bounded-byte-cursor-progress"); fixtureClear(&f);
 fixture group[66]; memset(group,0,sizeof(group)); char* shared=xidePathResolveLexical(base,"shared-global"),*stores=xidePathResolveLexical(shared,"migrations"); CHECK(xideIoCreateDirectories(stores));
 for(int i=0;i<130;++i) { char leaf[17]; snprintf(leaf,sizeof(leaf),"%016llx",(unsigned long long)i); path=xidePathResolveLexical(stores,leaf); CHECK(xideIoCreateDirectories(path)); alias=xidePathResolveLexical(path,"unknown.bin"); CHECK(writeText(alias,"preserve unknown")); free(path); free(alias); }
 for(int i=0;i<66;++i) { char name[40]; snprintf(name,sizeof(name),"owner-%d",i); fixtureMakeIn(&group[i],base,name,shared); retire(&group[i]); }
 removed=0; rounds=0; do { CHECK(xideMigrationJournalMaintain(shared,future,NULL,&stats,&error) && stats.scanned<=128 && stats.removedBytes<=64u*1024u*1024u); removed+=stats.removed; ++rounds; } while(removed<264 && rounds<12); CHECK(removed==264 && rounds>=5);
 for(int i=0;i<66;++i) { CHECK(!owned(&group[i])); sourcePreserved(&group[i]); fixtureClear(&group[i]); }
 for(int i=0;i<130;++i) { char leaf[17]; snprintf(leaf,sizeof(leaf),"%016llx",(unsigned long long)i); path=xidePathResolveLexical(stores,leaf); alias=xidePathResolveLexical(path,"unknown.bin"); CHECK(xideIoExists(alias)); char* newLock=xidePathResolveLexical(path,"checkpoint.lock"); CHECK(!xideIoExists(newLock)); free(path); free(alias); free(newLock); } free(shared); free(stores); pass("bounded-store-cursor-with-unknown-prefix");
 printf("Migration store checks: %d passed; %d native cases, real production prepare/checkpoint/retire, owned handle deletion, immutable C/xlang copies and bounded resumable cleanup\n",checks,cases); free(base); return 0;
}
