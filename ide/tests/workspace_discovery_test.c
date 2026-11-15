#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include "services/workspace_discovery.h"
#include "services/rename_candidate.h"
#include "services/analysis_context.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static volatile LONG checks;static DWORD owner;static volatile LONG tracking,pauseArmed,ownerIO,sourceReads,wakes,openFiles,openDirectories;
static volatile LONG64 operations,faultAt;
static HANDLE entered,released;static const char *pausePath,*mutatePath,*mutateBody,*maskPath;
static volatile LONG mutationArmed,maskTimes;static xfileinfo maskedTimes;
static _Thread_local xfile readFile;static _Thread_local const char* readPath;
#define CHECK(v) do{InterlockedIncrement(&checks);if(!(v)){fprintf(stderr,"workspace-discovery:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
static int operation(void){return !InterlockedCompareExchange(&tracking,0,0) || InterlockedIncrement64(&operations)!=InterlockedCompareExchange64(&faultAt,0,0);}
static void* allocate(size_t n){return operation()?xrtMalloc(n):NULL;}
static void* allocateZero(size_t n,size_t size){return operation()?xrtCalloc(n,size):NULL;}
static char* duplicate(const char* p){return operation()?xrtStrDup(p):NULL;}
static bool arrayInit(xarray* a,size_t n){return operation()&&xrtArrayInit(a,n);}
static bool arrayPush(xarray* a,const void* p){return operation()&&xrtArrayPush(a,p);}
static bool mapInit(xmap* a,size_t n){return operation()&&xrtMapInit(a,n);}
static void* mapGet(xmap* a,xbytesview key,bool* added){return operation()?xrtMapGetOrAdd(a,key,added):NULL;}
static xvalue* parse(xstrview text){return operation()?xrtJsonParse(text):NULL;}
static char* encode(const xvalue* value,bool pretty,size_t* n){return operation()?xrtJsonStringify(value,pretty,n):NULL;}
static xfile openFile(const char* path,uint32 flags)
{
 if(GetCurrentThreadId()==owner)InterlockedIncrement(&ownerIO);
 readPath=path;readFile=xideIoOpen(path,flags);if(readFile)InterlockedIncrement(&openFiles);
 if(readFile&&pausePath&&xidePathLexicalEqual(path,pausePath)&&InterlockedCompareExchange(&pauseArmed,0,1)==1){CHECK(SetEvent(entered)&&WaitForSingleObject(released,30000)==WAIT_OBJECT_0);}
 return readFile;
}
static bool closeFile(xfile file){bool okay=xrtClose(file);if(file==readFile){readFile=NULL;readPath=NULL;InterlockedDecrement(&openFiles);}return okay;}
static int statPath(const char* path,int follow,xfileinfo* info){if(GetCurrentThreadId()==owner)InterlockedIncrement(&ownerIO);return xideIoStat(path,follow,info);}
static int receiptPath(const char* path,xfileinfo* info){if(GetCurrentThreadId()==owner)InterlockedIncrement(&ownerIO);return xideIoReceiptStat(path,info);}
static xdir openDirectory(cstr path,uint32 flags){if(GetCurrentThreadId()==owner)InterlockedIncrement(&ownerIO);xdir dir=xrtDirOpen(path,flags);if(dir)InterlockedIncrement(&openDirectories);return dir;}
static xdirnext nextDirectory(xdir dir,xdirentry* entry){if(GetCurrentThreadId()==owner)InterlockedIncrement(&ownerIO);return xrtDirNext(dir,entry);}
static bool closeDirectory(xdir dir){if(GetCurrentThreadId()==owner)InterlockedIncrement(&ownerIO);bool okay=xrtDirClose(dir);InterlockedDecrement(&openDirectories);return okay;}
static bool readFull(xfile f,ptr p,size_t n,size_t* done)
{if(GetCurrentThreadId()==owner)InterlockedIncrement(&ownerIO);if(f!=readFile || !readPath || (!xidePathHasExtension(readPath,".xlp")&&!xidePathHasExtension(readPath,".xlw")))InterlockedIncrement(&sourceReads);return xrtReadFull(f,p,n,done);}
static int statFile(xfile f,xfileinfo* out)
{int okay=xideIoReceiptFileStat(f,out);if(okay&&maskTimes&&readPath&&maskPath&&xidePathLexicalEqual(readPath,maskPath)){out->Modified=maskedTimes.Modified;out->Changed=maskedTimes.Changed;}return okay;}
static xls_context* projectContext(const xide_project* p,const char* c,const char* profile,const char* package,xide_model_error* e)
{xls_context* out=operation()?xideAnalysisProjectContext(p,c,profile,package,e):NULL;if(out&&mutatePath&&p->manifest&&xidePathLexicalEqual(p->manifest,mutatePath)&&InterlockedCompareExchange(&mutationArmed,0,1)==1)CHECK(xideIoWriteAtomic(mutatePath,(xbytesview){(const unsigned char*)mutateBody,strlen(mutateBody)}));return out;}
#define xrtMalloc allocate
#define xrtCalloc allocateZero
#define xrtStrDup duplicate
#define xrtArrayInit arrayInit
#define xrtArrayPush arrayPush
#define xrtMapInit mapInit
#define xrtMapGetOrAdd mapGet
#define xrtJsonParse parse
#define xrtJsonStringify encode
#define xideIoOpen openFile
#define xrtClose closeFile
#define xideIoStat statPath
#define xideIoReceiptStat receiptPath
#define xrtDirOpen openDirectory
#define xrtDirNext nextDirectory
#define xrtDirClose closeDirectory
#define xrtReadFull readFull
#define xideIoReceiptFileStat statFile
#define xideAnalysisProjectContext projectContext
#include "../src/services/workspace_discovery.c"
#undef xrtMalloc
#undef xrtCalloc
#undef xrtStrDup
#undef xrtArrayInit
#undef xrtArrayPush
#undef xrtMapInit
#undef xrtMapGetOrAdd
#undef xrtJsonParse
#undef xrtJsonStringify
#undef xideIoOpen
#undef xrtClose
#undef xideIoStat
#undef xideIoReceiptStat
#undef xrtDirOpen
#undef xrtDirNext
#undef xrtDirClose
#undef xrtReadFull
#undef xideIoReceiptFileStat
#undef xideAnalysisProjectContext
static char* join(const char* base,const char* leaf){char* p=xidePathResolveLexical(base,leaf);CHECK(p);return p;}
static void write(const char* path,const char* body){char* dir=xidePathDirectory(path);CHECK(dir&&xideIoCreateDirectories(dir)&&xideIoWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)}));free(dir);}
static xvalue* json(const char* text){xvalue* v=xrtJsonParse(xrtStrView(text));CHECK(v);return v;}
static xide_workspace_folders* rootList(const char* first,const char* second)
{xvalue* p=xrtValueObject(),*a=xrtValueArray();CHECK(p&&a);const char* paths[2]={first,second};for(size_t i=0;i<(second?2u:first?1u:0u);++i){xvalue* row=xrtValueObject();char* uri=xideLspPathToUri(paths[i],NULL);CHECK(row&&uri&&put(row,"uri",xrtValueString(xrtStrView(uri)))&&put(row,"name",xrtValueString(xrtStrView("test")))&&xrtValueArrayAppend(a,row));xrtValueRelease(row);free(uri);}CHECK(put(p,"workspaceFolders",a));xide_workspace_folders* out=xideWorkspaceFoldersRead(p,NULL);CHECK(out);xrtValueRelease(p);return out;}
static xide_workspace_folders* roots(const char* path){return rootList(path,NULL);}
/* Same owned junction helper as resource_creation_test,without a shell. */
static int junction(const char* path,const char* target)
{
 wchar_t p[4096],t[4096],sub[4100];if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,p,4096)||!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,target,-1,t,4096))return 0;
 for(wchar_t* c=t;*c;++c)if(*c=='/')*c='\\';
 swprintf(sub,4100,L"\\??\\%ls",t);size_t a=wcslen(sub),b=wcslen(t);
 struct {DWORD tag;WORD bytes,reserved,subOffset,subLength,printOffset,printLength;wchar_t buffer[8200];} data={0};data.tag=IO_REPARSE_TAG_MOUNT_POINT;data.subLength=(WORD)(a*2);data.printOffset=(WORD)((a+1)*2);data.printLength=(WORD)(b*2);data.bytes=(WORD)(8+(a+b+2)*2);memcpy(data.buffer,sub,(a+1)*2);memcpy(data.buffer+a+1,t,(b+1)*2);
 if(!CreateDirectoryW(p,NULL))return 0;
 HANDLE h=CreateFileW(p,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);DWORD n=0;int okay=h!=INVALID_HANDLE_VALUE&&DeviceIoControl(h,FSCTL_SET_REPARSE_POINT,&data,data.bytes+8,NULL,0,&n,NULL);if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);if(!okay)RemoveDirectoryW(p);return okay;
}
static void wake(void* unused){(void)unused;InterlockedIncrement(&wakes);}
static void idle(xide_workspace_discovery* store)
{ULONGLONG until=GetTickCount64()+30000;while(!xideWorkspaceDiscoveryIdle(store)){CHECK(GetTickCount64()<until);Sleep(1);}}
static int wait(xide_discovery_query* q,xide_discovery_work* work,char* error)
{ULONGLONG until=GetTickCount64()+30000;int state;while((state=xideWorkspaceDiscoveryResult(q,NULL,work,error,512))==XIDE_DISCOVERY_PENDING){CHECK(GetTickCount64()<until);Sleep(1);}return state;}
static xide_discovery_snapshot* discover(xide_workspace_discovery* store,xide_workspace_folders* folders,const char* package,int expected)
{xide_discovery_query* q=xideWorkspaceDiscoverySubmit(store,folders,package);CHECK(q&&xideWorkspaceDiscoverySetWake(q,wake,NULL));char error[512];xide_discovery_work work;int state=wait(q,&work,error);if(state!=expected){const char* origin=NULL;xideWorkspaceFoldersAt(folders,0,NULL,NULL,&origin);fprintf(stderr,"discovery root=%s state=%d expected=%d dirs=%zu manifests=%zu sources=%zu error=%s\n",origin?origin:"<empty>",state,expected,work.directories,work.manifests,work.sources,error);}CHECK(state==expected&&work.workerThread!=owner);xide_discovery_snapshot* s=xideWorkspaceDiscoveryTake(q);CHECK((s!=NULL)==(expected==XIDE_DISCOVERY_READY));xideWorkspaceDiscoveryQueryDestroy(q);idle(store);return s;}
typedef struct validation {xide_discovery_snapshot* snapshot;int cancel,state;} validation;
static int cancelNow(void* p){return *(int*)p;}
static xtaskoutcome validateOnWorker(xcancel* cancel,void* raw,xtaskvalue* value)
{(void)cancel;(void)value;validation* in=raw;in->state=xideDiscoverySnapshotValidate(in->snapshot,cancelNow,&in->cancel,NULL);return XTASK_SUCCESS;}
static int verify(xide_workspace_discovery* store,xide_discovery_snapshot* snapshot,int cancel)
{validation in={snapshot,cancel,0};xfuture* f=xrtTaskSubmit(store->pool,validateOnWorker,&in,NULL);CHECK(f);ULONGLONG until=GetTickCount64()+30000;while(xrtFutureState(f)==XFUTURE_PENDING){CHECK(GetTickCount64()<until);Sleep(1);}CHECK(xrtFutureState(f)==XFUTURE_RESOLVED);xrtFutureDestroy(f);idle(store);return in.state;}
static int receive(const xvalue* value,void* user){xvalue** out=user;CHECK(!*out);*out=xrtValueDeepClone(value);return *out!=NULL;}
static void nativeRename(xide_discovery_snapshot* snapshot,const char* main,const char* body,const char* directory)
{
 char* compiler=join(directory,"missing-xl.exe"),*uri=xideLspPathToUri(main,NULL);xide_toolchains* cache=xideToolchainsCreate();xls_module_store* modules=xlsModuleStoreCreate(NULL);
 xide_project_references* references=xideProjectReferencesCreate(cache,compiler,modules);xide_rename_candidates* renames=xideRenameCandidatesCreate(references);CHECK(cache&&modules&&references&&renames&&uri);
 xvalue* params=json("{\"position\":{\"line\":2,\"character\":15},\"newName\":\"updated\"}");xvalue* doc=xrtValueObject();CHECK(doc&&put(doc,"uri",xrtValueString(xrtStrView(uri)))&&put(params,"textDocument",doc));xvalue* id=xrtValueString(xrtStrView("discovered-native"));xls_text* text=xlsTextCreate(body,strlen(body),8u*1024u*1024u,NULL);
 xls_analysis_document input={uri,main,text,1,(uint64_t)((int64_t)1-INT_MIN)};CHECK(id&&text);xide_rename_candidate_query* q=xideRenameStandardSubmit(renames,id,1,params,xideDiscoverySnapshotCatalog(snapshot),&input,1);CHECK(q);
 int state,code;char error[512];ULONGLONG until=GetTickCount64()+30000;while((state=xideRenameCandidateResult(q,&code,error,sizeof(error)))==0){CHECK(GetTickCount64()<until);Sleep(1);}if(state!=1)fprintf(stderr,"native rename state=%d code=%d error=%s\n",state,code,error);CHECK(state==1&&!code);
 size_t size=0;char* frame=xideRenameCandidateTakeFrame(q,&size);xvalue* reply=NULL;xide_lsp_stream stream={0};CHECK(frame&&size&&xideLspStreamFeed(&stream,frame,size,receive,&reply,NULL)&&reply);
 const xvalue* result=xrtValueObjectGet(reply,xrtStrView("result")),*changes=xrtValueObjectGet(result,xrtStrView("documentChanges"));CHECK(xrtValueType(changes)==XVALUE_ARRAY&&xrtValueCount(changes)==4);
 for(size_t i=0;i<4;++i){const xvalue* change=xrtValueArrayGet(changes,i),*edits=xrtValueObjectGet(change,xrtStrView("edits"));CHECK(xrtValueCount(edits)==1);xstrview name={0};CHECK(xrtValueGetString(xrtValueObjectGet(xrtValueArrayGet(edits,0),xrtStrView("newText")),&name)&&name.Size==7&&!memcmp(name.Data,"updated",7));}
 free(frame);xrtValueRelease(reply);xideLspStreamClear(&stream);xideRenameCandidateDestroy(q);xrtValueRelease(params);xrtValueRelease(id);xlsTextRelease(text);free(uri);xideRenameCandidatesDestroy(renames);xideProjectReferencesDestroy(references);xlsModuleStoreRelease(modules);xideToolchainsDestroy(cache);free(compiler);
}
static const char* project="{\"format\":\"xlang-project-1\",\"entry\":\"main.xl\",\"ide\":{\"name\":\"p\",\"files\":[{\"id\":\"s\",\"path\":\"shared.xl\"},{\"id\":\"c\",\"path\":\"closed.xl\"}],\"configurations\":[{\"id\":\"debug\",\"workingDirectory\":\"nonexistent-run-directory\",\"arguments\":[\"runtime-only\"],\"environment\":{\"XLANG_DISCOVERY_SHOULD_NOT_SET\":\"never-apply\"}}]}}";
static const char* mainBody="import shared\nfunc main(...) as int\n return shared.amount(1)\nend\n";
static const char* workspaceForward="{\"format\":\"xlang-workspace-1\",\"name\":\"w\",\"projects\":[{\"id\":\"p\",\"path\":\"p.xlp\"},{\"id\":\"o\",\"path\":\"extlibs/declared/o.xlp\"}],\"files\":[{\"id\":\"l\",\"path\":\"loose.xl\"}]}";
static const char* workspaceReverse="{\"format\":\"xlang-workspace-1\",\"name\":\"w\",\"projects\":[{\"id\":\"o\",\"path\":\"extlibs/declared/o.xlp\"},{\"id\":\"p\",\"path\":\"p.xlp\"}],\"files\":[{\"id\":\"l\",\"path\":\"loose.xl\"}]}";
int main(int argc,char** argv)
{
 const char* priorEnvironment=getenv("XLANG_DISCOVERY_SHOULD_NOT_SET");char* savedEnvironment=priorEnvironment?_strdup(priorEnvironment):NULL;CHECK(!priorEnvironment||savedEnvironment);
 owner=GetCurrentThreadId();wchar_t cwdBefore[32768],cwdAfter[32768];CHECK(GetCurrentDirectoryW(32768,cwdBefore));char leaf[80];snprintf(leaf,sizeof(leaf),"discovery-空间😀-%lu-%llu",owner,(unsigned long long)GetTickCount64());char* base=join(argc>1?argv[1]:".",leaf);CHECK(!xideIoExists(base)&&xideIoCreateDirectories(base));
 entered=CreateEventW(NULL,TRUE,FALSE,NULL);released=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(entered&&released);xide_model_error e={0};xide_workspace_discovery* store=xideWorkspaceDiscoveryCreate(NULL,&e);CHECK(store);
 char *main=join(base,"main.xl"),*shared=join(base,"shared.xl"),*closed=join(base,"closed.xl"),*loose=join(base,"loose.xl"),*manifest=join(base,"p.xlp"),*workspace=join(base,"w.xlw");
 write(main,mainBody);write(shared,"export func amount(int value) as int\n return value\nend\n");write(closed,mainBody);write(loose,mainBody);write(manifest,project);
 write(workspace,workspaceForward);char* declared=join(base,"extlibs/declared/o.xlp"),*outside=join(base,"extlibs/declared/main.xl");write(declared,"{\"format\":\"xlang-project-1\",\"entry\":\"main.xl\"}");write(outside,"func main(...) as int\n return 0\nend\n");
 char* ignoredFile=join(base,"extlibs/ignored.xlp");write(ignoredFile,"intentionally invalid JSON");free(ignoredFile);
 xide_workspace_folders* folders=roots(base);xide_discovery_snapshot* s=discover(store,folders,NULL,XIDE_DISCOVERY_READY);CHECK(s&&xideAnalysisCatalogCount(s->catalog)==2&&xideAnalysisCatalogFileCount(s->catalog)==1);
 const xls_context* context=NULL;CHECK(xideAnalysisCatalogFind(s->catalog,closed,&context)==XIDE_ANALYSIS_PROJECT&&context&&xlsContextSpec(context)->lists[XLS_CONTEXT_MEMBERS].count==3);CHECK(xidePathLexicalEqual(xlsContextSpec(context)->entry,main));
 CHECK(verify(store,s,1)==XIDE_DISCOVERY_CANCELLED&&verify(store,s,0)==XIDE_DISCOVERY_READY);nativeRename(s,main,mainBody,base);
 CHECK(xideAnalysisCatalogFind(s->catalog,outside,&context)==XIDE_ANALYSIS_PROJECT&&context&&xidePathLexicalEqual(xlsContextSpec(context)->project,declared));
 unsigned char fingerprint[32];memcpy(fingerprint,s->fingerprint,32);write(main,"import shared\nfunc main(...) as int\n return shared.amount(2)\nend\n");CHECK(verify(store,s,0)==XIDE_DISCOVERY_READY);write(main,mainBody);
 char* irrelevant=join(base,"README.txt");write(irrelevant,"ignored content");CHECK(verify(store,s,0)==XIDE_DISCOVERY_READY);free(irrelevant);
 write(workspace,workspaceReverse);CHECK(verify(store,s,0)==XIDE_DISCOVERY_STALE);xideDiscoverySnapshotRelease(s);s=discover(store,folders,NULL,XIDE_DISCOVERY_READY);CHECK(!memcmp(s->fingerprint,fingerprint,32));
 char* bom=malloc(strlen(workspaceReverse)+4);CHECK(bom);memcpy(bom,"\xef\xbb\xbf",3);strcpy(bom+3,workspaceReverse);write(workspace,bom);free(bom);CHECK(verify(store,s,0)==XIDE_DISCOVERY_STALE);xideDiscoverySnapshotRelease(s);s=discover(store,folders,NULL,XIDE_DISCOVERY_READY);CHECK(!memcmp(s->fingerprint,fingerprint,32));
 char* added=join(base,"added.xl");write(added,"func main(...) as int\n return 0\nend\n");CHECK(verify(store,s,0)==XIDE_DISCOVERY_STALE);char* native=xidePathNativeUtf8(added);CHECK(native&&xrtFileDelete(native));free(native);CHECK(verify(store,s,0)==XIDE_DISCOVERY_STALE);free(added);xideDiscoverySnapshotRelease(s);
 s=discover(store,folders,NULL,XIDE_DISCOVERY_READY);CHECK(!memcmp(s->fingerprint,fingerprint,32)&&xideIoReceiptStat(manifest,&maskedTimes));write(manifest,project);xfileinfo replacement={0};CHECK(xideIoReceiptStat(manifest,&replacement)&&replacement.Identity!=maskedTimes.Identity);maskPath=manifest;maskTimes=1;CHECK(verify(store,s,0)==XIDE_DISCOVERY_STALE);maskTimes=0;xideDiscoverySnapshotRelease(s);
 s=discover(store,folders,NULL,XIDE_DISCOVERY_READY);CHECK(xideIoReceiptStat(manifest,&maskedTimes));char* alternate=malloc(strlen(project)+1);CHECK(alternate);strcpy(alternate,project);char* name=strstr(alternate,"\"name\":\"p\"");CHECK(name);name[8]='q';
 xfile same=xideIoOpen(manifest,XFILE_WRITE);size_t written=0;CHECK(same&&xrtWriteFull(same,alternate,strlen(alternate),&written)&&written==strlen(alternate)&&xrtClose(same));maskPath=manifest;maskTimes=1;CHECK(verify(store,s,0)==XIDE_DISCOVERY_STALE);maskTimes=0;xideDiscoverySnapshotRelease(s);
 s=discover(store,folders,NULL,XIDE_DISCOVERY_READY);CHECK(!memcmp(s->fingerprint,fingerprint,32));xideDiscoverySnapshotRelease(s);write(manifest,project);free(alternate);
 mutatePath=manifest;mutateBody=project;mutationArmed=1;s=discover(store,folders,NULL,XIDE_DISCOVERY_FAILED);CHECK(!s&&!mutationArmed);mutatePath=mutateBody=NULL;
 write(manifest,"{\"format\":\"unsupported-project\"}");CHECK(!discover(store,folders,NULL,XIDE_DISCOVERY_FAILED));write(manifest,project);
 pausePath=manifest;pauseArmed=1;xide_discovery_query* first=xideWorkspaceDiscoverySubmit(store,folders,NULL);CHECK(first&&xideWorkspaceDiscoverySetWake(first,wake,NULL)&&WaitForSingleObject(entered,30000)==WAIT_OBJECT_0);
 CHECK(openFiles==1&&!openDirectories);xfuture* retired=xrtFutureRef(first->future);CHECK(retired);xideWorkspaceDiscoveryCancel(first);CHECK(xideWorkspaceDiscoveryResult(first,NULL,NULL,NULL,0)==XIDE_DISCOVERY_PENDING&&!xideWorkspaceDiscoveryIdle(store));xide_discovery_query* second=xideWorkspaceDiscoverySubmit(store,folders,NULL);CHECK(second&&!xideWorkspaceDiscoverySubmit(store,folders,NULL));xideWorkspaceDiscoveryQueryDestroy(first);CHECK(SetEvent(released));char error[512];CHECK(wait(second,NULL,error)==XIDE_DISCOVERY_READY);s=xideWorkspaceDiscoveryTake(second);CHECK(s);xideWorkspaceDiscoveryQueryDestroy(second);idle(store);CHECK(xrtFutureState(retired)==XFUTURE_CANCELLED&&!openFiles&&!openDirectories);xrtFutureDestroy(retired);xideDiscoverySnapshotRelease(s);pausePath=NULL;
 xide_discovery_limits limits={.entries=1};xide_workspace_discovery* small=xideWorkspaceDiscoveryCreate(&limits,&e);CHECK(small&&!discover(small,folders,NULL,XIDE_DISCOVERY_FAILED));xideWorkspaceDiscoveryDestroy(small);
 limits=(xide_discovery_limits){.manifestBytes=8};small=xideWorkspaceDiscoveryCreate(&limits,&e);CHECK(small&&!discover(small,folders,NULL,XIDE_DISCOVERY_FAILED));xideWorkspaceDiscoveryDestroy(small);
 limits=(xide_discovery_limits){.directories=4097};CHECK(!xideWorkspaceDiscoveryCreate(&limits,&e)&&e.code);
 limits=(xide_discovery_limits){.labels=64};small=xideWorkspaceDiscoveryCreate(&limits,&e);CHECK(small&&!discover(small,folders,NULL,XIDE_DISCOVERY_FAILED));xideWorkspaceDiscoveryDestroy(small);
 s=discover(store,folders,NULL,XIDE_DISCOVERY_READY);char* nested=join(base,"nested"),*nestedFile=join(nested,"new.xl");write(nestedFile,"func main(...) as int\n return 0\nend\n");CHECK(verify(store,s,0)==XIDE_DISCOVERY_STALE);xideDiscoverySnapshotRelease(s);s=discover(store,folders,NULL,XIDE_DISCOVERY_READY);CHECK(s->proof->directories.Count==2&&xideAnalysisCatalogFileCount(s->catalog)==2);nativeRename(s,main,mainBody,base);xideDiscoverySnapshotRelease(s);
 xide_workspace_folders* overlap=rootList(base,nested);s=discover(store,overlap,NULL,XIDE_DISCOVERY_READY);CHECK(s->proof->directories.Count==2&&s->proof->manifests.Count==3);xideDiscoverySnapshotRelease(s);xideWorkspaceFoldersRelease(overlap);
 limits=(xide_discovery_limits){.directories=1};small=xideWorkspaceDiscoveryCreate(&limits,&e);CHECK(small&&!discover(small,folders,NULL,XIDE_DISCOVERY_FAILED));xideWorkspaceDiscoveryDestroy(small);free(nestedFile);free(nested);
 /* Calibrate only this module's fallible API gates;core/model allocator tests
    remain separate. Every admitted failure must produce no partial catalog. */
 tracking=1;operations=0;faultAt=0;s=discover(store,folders,NULL,XIDE_DISCOVERY_READY);LONG64 gates=operations;tracking=0;xideDiscoverySnapshotRelease(s);CHECK(gates>20);
 for(LONG64 at=1;at<=gates;++at){tracking=1;operations=0;faultAt=at;xide_discovery_query* q=xideWorkspaceDiscoverySubmit(store,folders,NULL);if(q){int state=wait(q,NULL,error);CHECK(state==XIDE_DISCOVERY_FAILED);CHECK(!xideWorkspaceDiscoveryTake(q));xideWorkspaceDiscoveryQueryDestroy(q);}idle(store);tracking=0;}
 faultAt=0;s=discover(store,folders,NULL,XIDE_DISCOVERY_READY);char* runtimeChange=_strdup(project);CHECK(runtimeChange);char* runtimeField=strstr(runtimeChange,"runtime-only");CHECK(runtimeField);memcpy(runtimeField,"runtime-new!",12);write(manifest,runtimeChange);free(runtimeChange);CHECK(verify(store,s,0)==XIDE_DISCOVERY_STALE);unsigned char metadataOnly[32];memcpy(metadataOnly,s->fingerprint,32);xideDiscoverySnapshotRelease(s);s=discover(store,folders,NULL,XIDE_DISCOVERY_READY);CHECK(!memcmp(metadataOnly,s->fingerprint,32));memcpy(fingerprint,s->fingerprint,32);xideDiscoverySnapshotRelease(s);s=discover(store,folders,"",XIDE_DISCOVERY_READY);CHECK(memcmp(s->fingerprint,fingerprint,32)&&xideAnalysisCatalogPackagePath(s->catalog)&&!*xideAnalysisCatalogPackagePath(s->catalog));xideDiscoverySnapshotRelease(s);
 xide_workspace_folders* empty=rootList(NULL,NULL);s=discover(store,empty,NULL,XIDE_DISCOVERY_READY);CHECK(!s->proof->directories.Count&&!s->proof->manifests.Count&&!xideAnalysisCatalogCount(s->catalog)&&!xideAnalysisCatalogFileCount(s->catalog));xideDiscoverySnapshotRelease(s);xideWorkspaceFoldersRelease(empty);
 char* bounded=join(base,"bounded"),*one=join(bounded,"one.xl"),*textFile=join(bounded,"README.txt");write(one,"func main(...) as int\n return 0\nend\n");xide_workspace_folders* boundedRoots=roots(bounded);limits=(xide_discovery_limits){.entries=1};small=xideWorkspaceDiscoveryCreate(&limits,&e);s=discover(small,boundedRoots,NULL,XIDE_DISCOVERY_READY);write(textFile,"not a source member");CHECK(verify(small,s,0)==XIDE_DISCOVERY_STALE);xideDiscoverySnapshotRelease(s);xideWorkspaceFoldersRelease(boundedRoots);xideWorkspaceDiscoveryDestroy(small);free(bounded);free(one);free(textFile);
 char* target=join(base,"link-target"),*targetFile=join(target,"one.xl"),*unsafe=join(base,"link-unsafe"),*alias=join(unsafe,"alias");write(targetFile,"func main(...) as int\n return 0\nend\n");CHECK(xideIoCreateDirectories(unsafe)&&junction(alias,target));xide_workspace_folders* links=roots(unsafe);CHECK(!discover(store,links,NULL,XIDE_DISCOVERY_FAILED));xideWorkspaceFoldersRelease(links);links=roots(alias);s=discover(store,links,NULL,XIDE_DISCOVERY_READY);CHECK(s->proof->directories.Count==1&&xideAnalysisCatalogFileCount(s->catalog)==1&&verify(store,s,0)==XIDE_DISCOVERY_READY);xideDiscoverySnapshotRelease(s);xideWorkspaceFoldersRelease(links);links=rootList(target,alias);CHECK(!discover(store,links,NULL,XIDE_DISCOVERY_FAILED));xideWorkspaceFoldersRelease(links);free(target);free(targetFile);free(unsafe);free(alias);
 CHECK(!ownerIO&&!sourceReads&&!openFiles&&!openDirectories&&wakes>0&&GetCurrentDirectoryW(32768,cwdAfter)&&!wcscmp(cwdBefore,cwdAfter));xideWorkspaceFoldersRelease(folders);xideWorkspaceDiscoveryDestroy(store);CHECK(CloseHandle(entered)&&CloseHandle(released));
 const char* afterEnvironment=getenv("XLANG_DISCOVERY_SHOULD_NOT_SET");CHECK((!savedEnvironment&&!afterEnvironment)||(savedEnvironment&&afterEnvironment&&!strcmp(savedEnvironment,afterEnvironment)));free(savedEnvironment);
 free(main);free(shared);free(closed);free(loose);free(manifest);free(workspace);free(declared);free(outside);free(base);
 printf("workspace_discovery_test passed: %ld checks;%lld calibrated local failure gates;closed-file native rename,exact directory/raw-manifest guards,cancel/quiescence and discovery-only no source-body/owner IO.\n",checks,(long long)gates);return 0;
}
