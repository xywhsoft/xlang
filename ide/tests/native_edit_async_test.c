#define main legacyCandidateMain
#include "edit_candidate_test.c"
#undef main
#include "editor/native_edit_async.h"
#include "editor/documents.h"
static DWORD owner;static xide_native_edit_async* current;static uint64_t currentToken;
static HANDLE entered,released;static volatile LONG holdRead,holdWrite,holdRetire,flushFault;
static volatile LONG workerCalls,ownerReads,ownerWaits;static uint64_t workerId;static int normal;
static uint32_t writeVolume;static uint64_t writeIndex;
static void verifyWorker(void)
{if(GetCurrentThreadId()==owner)ExitProcess(71);uint64_t id=xrtThreadCurrentId();if(workerId&&workerId!=id)ExitProcess(72);workerId=id;InterlockedIncrement(&workerCalls);}
static void held(void)
{SetEvent(entered);if(WaitForSingleObject(released,30000)!=WAIT_OBJECT_0)ExitProcess(73);}
extern bool(*__real___imp_xrtReadFull)(xfile,ptr,size_t,size_t*);
static bool readProbe(xfile file,ptr data,size_t size,size_t* done)
{if(normal&&GetCurrentThreadId()==owner)InterlockedIncrement(&ownerReads);bool okay=__real___imp_xrtReadFull(file,data,size,done);if(okay&&done&&*done&&GetCurrentThreadId()!=owner&&InterlockedCompareExchange(&holdRead,0,1)==1)held();return okay;}
bool(*__wrap___imp_xrtReadFull)(xfile,ptr,size_t,size_t*)=readProbe;
extern xwaitresult(*__real___imp_xrtCondWait)(xcond*,xmutex*);
static xwaitresult waitProbe(xcond* cond,xmutex* mutex)
{if(normal&&GetCurrentThreadId()==owner)InterlockedIncrement(&ownerWaits);return __real___imp_xrtCondWait(cond,mutex);}
xwaitresult(*__wrap___imp_xrtCondWait)(xcond*,xmutex*)=waitProbe;
extern BOOL(WINAPI* __real___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
static BOOL WINAPI writeProbe(HANDLE file,LPCVOID data,DWORD size,LPDWORD done,LPOVERLAPPED position)
{BOOL okay=__real___imp_WriteFile(file,data,size,done,position);BY_HANDLE_FILE_INFORMATION info={0};if(okay&&done&&*done&&holdWrite&&GetFileInformationByHandle(file,&info)&&info.dwVolumeSerialNumber==writeVolume&&(((uint64_t)info.nFileIndexHigh<<32)|info.nFileIndexLow)==writeIndex&&InterlockedCompareExchange(&holdWrite,0,1)==1)held();return okay;}
BOOL(WINAPI* __wrap___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED)=writeProbe;
extern BOOL(WINAPI* __real___imp_FlushFileBuffers)(HANDLE);
static BOOL WINAPI flushProbe(HANDLE file)
{wchar_t path[4096];DWORD n=GetFinalPathNameByHandleW(file,path,4096,0);if(flushFault&&n&&n<4096&&wcsstr(path,L"edit-transaction.log")&&InterlockedCompareExchange(&flushFault,0,1)==1){SetLastError(ERROR_WRITE_FAULT);return FALSE;}return __real___imp_FlushFileBuffers(file);}
BOOL(WINAPI* __wrap___imp_FlushFileBuffers)(HANDLE)=flushProbe;
xide_native_edit* __real_xideNativeEditPrepare(xide_edit_candidate*,const char*,const xide_edit_alias_exclusion*,size_t,size_t,int(*)(void*),void*,xide_model_error*);
xide_native_edit* __wrap_xideNativeEditPrepare(xide_edit_candidate* c,const char* dir,const xide_edit_alias_exclusion* paths,size_t count,size_t budget,int(*stop)(void*),void* user,xide_model_error* error)
{verifyWorker();return __real_xideNativeEditPrepare(c,dir,paths,count,budget,stop,user,error);}
int __real_xideNativeEditStage(xide_native_edit*,int(*)(void*),void*,xide_model_error*);
int __wrap_xideNativeEditStage(xide_native_edit* work,int(*stop)(void*),void* user,xide_model_error* error)
{verifyWorker();return __real_xideNativeEditStage(work,stop,user,error);}
int __real_xideNativeEditApply(xide_native_edit*,int(*)(void*,size_t,int),void*,int(*)(void*),void*,xide_model_error*);
int __wrap_xideNativeEditApply(xide_native_edit* work,int(*guard)(void*,size_t,int),void* user,int(*stop)(void*),void* cancelUser,xide_model_error* error)
{verifyWorker();return __real_xideNativeEditApply(work,guard,user,stop,cancelUser,error);}
int __real_xideNativeEditFinish(xide_native_edit*,int(*)(void*,size_t,int),void*,xide_model_error*);
int __wrap_xideNativeEditFinish(xide_native_edit* work,int(*guard)(void*,size_t,int),void* user,xide_model_error* error)
{verifyWorker();return __real_xideNativeEditFinish(work,guard,user,error);}
int __real_xideNativeEditDestroy(xide_native_edit*);
int __wrap_xideNativeEditDestroy(xide_native_edit* work)
{verifyWorker();int status=__real_xideNativeEditDestroy(work);if(status==XUI_OK&&InterlockedCompareExchange(&holdRetire,0,1)==1)held();return status;}
static int uiOkay(void* user){return user?XUI_ERROR_INVALID_STATE:XUI_OK;}
static DWORD WINAPI foreignCalls(void* raw)
{(void)raw;xide_open_edit_outcome out={0};if(xideNativeAsyncAuthorize(current,currentToken,1,1)||xideNativeAsyncPrepare(current,currentToken)||xideNativeAsyncStage(current,currentToken)||xideNativeAsyncApply(current,currentToken)||xideNativeAsyncFinish(current,currentToken)||xideNativeAsyncRetire(current,currentToken,1)||xideNativeAsyncDrop(current,currentToken))return 1;if(xideNativeAsyncCapture(current,currentToken,NULL)!=XUI_ERROR_BUSY||xideNativeAsyncArm(current,currentToken,uiOkay,NULL)!=XUI_ERROR_BUSY||xideNativeAsyncPublish(current,currentToken,uiOkay,NULL,&out)!=XUI_ERROR_BUSY||xideNativeAsyncDetach(current,currentToken)!=XUI_ERROR_BUSY||xideNativeAsyncDestroy(current)!=XUI_ERROR_INVALID_ARGUMENT)return 2;xideNativeAsyncCancel(current,currentToken);return 0;}
static void changed(xide_document* document,void* user)
{(void)document;(void)user;if(current&&currentToken)xideNativeAsyncCancel(current,currentToken);}
typedef struct make {fixture rpc;xide_edit_candidate* candidate;xide_model_error error;} make;
static DWORD WINAPI makeCandidate(void* raw)
{make* j=raw;fixture* f=&j->rpc;xide_edit_request* request=xideEditRequestCreate(++f->token,++f->request,f->sync,f->catalog,&f->files,f->modelsCount,modelAt,f,&j->error);xide_edit_call* call=request?xideEditCallCreate(request,xideLanguageClientIdentity(client),f->uri,2,16,xrtStrView("renamed_😀"),NULL,NULL,NULL):NULL;size_t bytes=0;xvalue* root=call&&xideEditCallSend(call,client)?receive(f->request,&bytes):NULL;xide_edit_reply* reply=root?xideEditCallReply(call,xideLanguageClientIdentity(client),root,bytes,NULL):NULL;if(reply)root=NULL;j->candidate=reply?xideEditCandidatePrepare(call,reply,0,NULL,NULL,&j->error):NULL;xrtValueRelease(root);xideEditReplyRelease(reply);xideEditCallRelease(call);xideEditRequestRelease(request);return j->candidate?0:1;}
static int status(enum xide_native_async_state expected,xide_native_async_status* s)
{ULONGLONG until=GetTickCount64()+30000;do{CHECK(xideNativeAsyncStatus(current,s));if(!s->active&&s->state==expected)return 1;if(!s->active&&(s->state==XIDE_NATIVE_FAILED||s->state==XIDE_NATIVE_FINISHED||s->state==XIDE_NATIVE_READY||s->state==XIDE_NATIVE_RETIRED||s->state==XIDE_NATIVE_IDLE))CHECK(s->state==expected);CHECK(GetTickCount64()<until);Sleep(1);}while(1);}
static int disk(const xide_workspace_edit* plan,int after)
{int saved=normal;normal=0;xide_workspace_edit_info info;CHECK(xideWorkspaceEditInfo(plan,&info));for(size_t i=0;i<info.documents;++i){xide_workspace_edit_document d;CHECK(xideWorkspaceEditDocumentAt(plan,i,&d));if(d.hasVersion)continue;xide_text_file file={0};CHECK(xideTextFileRead(d.path,&file,NULL));const xls_text* expected=after?d.after:d.before;CHECK(strlen(file.text)==xlsTextSize(expected)&&!memcmp(file.text,xlsTextData(expected),xlsTextSize(expected)));xideTextFileClear(&file);}normal=saved;return 1;}
static int one(const char* directory,const char* server,int mode)
{
 make j={0};fixture* f=&j.rpc;f->request=100;f->modelsCount=2;CHECK(xideIoCreateDirectories(directory));const char* names[]={"main.xl","second.xl","shared.xl","unopened.xl","isolated.xl","solo.xl","unused.c","absent-xl.exe"};for(size_t i=0;i<8;++i){f->paths[i]=xidePathResolve(directory,names[i]);CHECK(f->paths[i]);}
 const char *body="import shared\nfunc main(...) as int\n return shared.compute(1)\nend\n",*use="import shared\nfunc main(...) as int\n return shared.compute(2)\nend\n",*shared="\xef\xbb\xbf" "export func compute(int value) as int\r\n return value\r\nend\r\n",*isolated="func main(...) as int\n return 0\nend\n";const char* bodies[]={body,use,shared,use,isolated,use,"int untouched=0;\n"};for(size_t i=0;i<7;++i)CHECK(xideIoWriteAtomic(f->paths[i],(xbytesview){(const unsigned char*)bodies[i],strlen(bodies[i])}));
 xui_context context=NULL;CHECK(xuiCreate(&context)==XUI_OK);xide_document_store store;xideDocumentsInit(&store,context,NULL,changed,NULL);xide_document* main=xideDocumentOpen(&store,f->paths[0],NULL);xide_document* c=xideDocumentOpen(&store,f->paths[6],NULL);CHECK(main&&c);xide_document* docs[]={main,c};for(size_t i=0;i<2;++i){xide_document* d=docs[i];f->models[i]=(xide_edit_request_model){d->id,d->revision,d->open,d->dirty,d->externalState,d->path,d->language};}
 f->uri=xideLspPathToUri(f->paths[0],NULL);f->catalog=configuration(directory,f);f->sync=xlsSyncCreateMonotonic(2);f->loose=(xide_file){.path=f->paths[5]};f->files=(xide_files){&f->loose,1,1};CHECK(f->uri&&f->catalog&&f->sync);client=xideLanguageClientStart(server,f->paths[7],directory,NULL);CHECK(client);xvalue* params=xrtJsonParse(xrtStrView("{\"capabilities\":{},\"initializationOptions\":{\"checkOnSave\":false,\"automaticDiagnostics\":false}}"));CHECK(params&&sendRequest("initialize",params,1));xrtValueRelease(params);size_t bytes=0;xvalue* reply=receive(1,&bytes);CHECK(reply&&get(reply,"result")&&xideLanguageClientSetAnalysisLease(client,1));xrtValueRelease(reply);params=xrtValueObject();CHECK(params&&put(params,"epoch",xrtValueInt(1))&&put(params,"catalog",xrtValueRetain(f->catalog))&&sendRequest("xlang/configureAnalysis",params,2));xrtValueRelease(params);reply=receive(2,&bytes);CHECK(reply&&get(reply,"result"));xrtValueRelease(reply);xls_sync_document synced={f->uri,body,strlen(body),main->id,main->revision};CHECK(xlsSyncUpdate(f->sync,&synced,1,emit,NULL));params=xrtValueObject();CHECK(params&&sendRequest("xlang/inputState",params,3));xrtValueRelease(params);reply=receive(3,&bytes);CHECK(reply&&get(reply,"result"));xrtValueRelease(reply);
 HANDLE maker=CreateThread(NULL,0,makeCandidate,&j,0,NULL);CHECK(maker&&WaitForSingleObject(maker,30000)==WAIT_OBJECT_0);DWORD code=1;CHECK(GetExitCodeThread(maker,&code)&&!code&&CloseHandle(maker)&&j.candidate);
 int64_t lease=xideLanguageClientSuspendAnalysis(client,NULL);CHECK(lease);ULONGLONG until=GetTickCount64()+15000;while(xideLanguageClientAnalysisState(client,lease,NULL)!=XIDE_LS_LEASE_HELD){CHECK(GetTickCount64()<until);Sleep(1);}uint64_t documents=xideDocumentsSuspendSources(&store,NULL);CHECK(documents);
 current=xideNativeAsyncCreate(NULL,NULL);CHECK(current);workerId=0;workerCalls=ownerReads=ownerWaits=0;normal=1;char* recovery=xidePathResolve(directory,"recovery");CHECK(recovery);
 if(mode==0){char* longPath=malloc(65537);CHECK(longPath);memset(longPath,'x',65536);longPath[65536]=0;CHECK(!xideNativeAsyncSubmit(current,j.candidate,longPath,NULL,0,0));longPath[65535]=0;xide_edit_alias_exclusion tooMany[16];for(size_t i=0;i<16;++i)tooMany[i]=(xide_edit_alias_exclusion){longPath,1};CHECK(!xideNativeAsyncSubmit(current,j.candidate,recovery,tooMany,16,0));free(longPath);}
 currentToken=xideNativeAsyncSubmit(current,j.candidate,recovery,NULL,0,0);CHECK(currentToken&&!xideNativeAsyncSubmit(current,j.candidate,recovery,NULL,0,0));const xide_workspace_edit* plan=xideEditCandidatePlan(j.candidate);xide_workspace_edit* retained=xideWorkspaceEditRetain((xide_workspace_edit*)plan);CHECK(retained);xideEditCandidateRelease(j.candidate);j.candidate=NULL;xide_native_async_status s={0};
 if(mode==0){HANDLE other=CreateThread(NULL,0,foreignCalls,NULL,0,NULL);CHECK(other&&WaitForSingleObject(other,10000)==WAIT_OBJECT_0);DWORD result=1;CHECK(GetExitCodeThread(other,&result)&&!result&&CloseHandle(other)&&xideNativeAsyncStatus(current,&s)&&!s.cancelled&&!s.applyAllowed&&!s.restoreAllowed);}
 CHECK(status(XIDE_NATIVE_NEW,&s)&&!s.active&&!workerCalls&&!ownerReads&&!xideNativeAsyncPrepare(current,currentToken)&&!xideNativeAsyncAuthorize(current,currentToken+1,1,1)&&xideNativeAsyncAuthorize(current,currentToken,1,1));
 ResetEvent(entered);ResetEvent(released);if(mode==4)holdRead=1;CHECK(xideNativeAsyncPrepare(current,currentToken));
 if(mode==4){CHECK(WaitForSingleObject(entered,20000)==WAIT_OBJECT_0&&xideNativeAsyncStatus(current,&s)&&s.active&&s.state==XIDE_NATIVE_PREPARING);xideNativeAsyncCancel(current,currentToken);CHECK(!xideNativeAsyncRetire(current,currentToken,0)&&!xideNativeAsyncDrop(current,currentToken)&&xideNativeAsyncCapture(current,currentToken,&store)==XUI_ERROR_BUSY&&SetEvent(released));CHECK(status(XIDE_NATIVE_FAILED,&s));}
 else {
  CHECK(status(XIDE_NATIVE_PREPARED,&s)&&s.workerThread!=owner&&xideNativeAsyncCapture(current,currentToken,&store)==XUI_OK&&xideNativeAsyncStage(current,currentToken)&&status(XIDE_NATIVE_STAGED,&s));
  if(mode==7){store.onChanged=NULL;CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"// edit\n")==XUI_OK&&xideNativeAsyncArm(current,currentToken,uiOkay,NULL)!=XUI_OK);xideNativeAsyncCancel(current,currentToken);}
  else {
   CHECK(xideNativeAsyncArm(current,currentToken,uiOkay,NULL)==XUI_OK);if(mode==1){xide_workspace_edit_info info;CHECK(xideWorkspaceEditInfo(plan,&info));for(size_t i=0;i<info.documents;++i){xide_workspace_edit_document d;CHECK(xideWorkspaceEditDocumentAt(plan,i,&d));if(!d.hasVersion){xfileinfo stat={0};CHECK(xideIoReceiptStat(d.path,&stat));writeVolume=(uint32_t)stat.Device;writeIndex=stat.Identity;break;}}holdWrite=1;}
   if(mode==8)CHECK(SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",L"async-environment-changed"));
   CHECK(xideNativeAsyncApply(current,currentToken));
   if(mode==8){CHECK(status(XIDE_NATIVE_FINISHED,&s)&&s.transaction.rolledBack&&!s.transaction.needsRecovery&&SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",NULL));}
   else if(mode==1){CHECK(WaitForSingleObject(entered,20000)==WAIT_OBJECT_0&&xideNativeAsyncStatus(current,&s)&&s.active&&s.state==XIDE_NATIVE_APPLYING);xideNativeAsyncCancel(current,currentToken);CHECK(!xideNativeAsyncDrop(current,currentToken)&&!xideNativeAsyncRetire(current,currentToken,0)&&SetEvent(released)&&status(XIDE_NATIVE_FINISHED,&s)&&s.transaction.rolledBack&&!s.transaction.needsRecovery);}
   else {
    CHECK(status(XIDE_NATIVE_READY,&s)&&!xideNativeAsyncRetire(current,currentToken,0)&&!xideNativeAsyncDrop(current,currentToken));
    if(mode==3){normal=0;CHECK(xideNativeAsyncDestroy(current)==XUI_OK);current=NULL;CHECK(disk(plan,0));goto destroyed;}
    xide_open_edit_outcome out={0};CHECK((xideNativeAsyncPublish(current,currentToken,uiOkay,mode==2?(void*)1:NULL,&out)==XUI_OK)==(mode!=2));CHECK(out.committed==(mode!=2));xideNativeAsyncCancel(current,currentToken);if(mode==6)flushFault=1;CHECK(xideNativeAsyncFinish(current,currentToken)&&status(XIDE_NATIVE_FINISHED,&s));CHECK(s.transaction.committed==(mode!=2)&&s.transaction.rolledBack==(mode==2));
    if(mode==6){CHECK(s.transaction.needsRecovery&&!flushFault&&!xideNativeAsyncRetire(current,currentToken,0));normal=0;CHECK(xideNativeAsyncDestroy(current)==XUI_OK);current=NULL;CHECK(disk(plan,1));goto destroyed;}
   }
  }
 }
 CHECK(xideNativeAsyncDetach(current,currentToken)==XUI_OK);ResetEvent(entered);ResetEvent(released);if(mode==5)holdRetire=1;CHECK(xideNativeAsyncRetire(current,currentToken,0));
 if(mode==5){CHECK(WaitForSingleObject(entered,20000)==WAIT_OBJECT_0&&xideNativeAsyncStatus(current,&s)&&s.active);xideNativeAsyncCancel(current,currentToken);CHECK(xideNativeAsyncAuthorize(current,currentToken,0,1)&&!xideNativeAsyncDrop(current,currentToken)&&SetEvent(released));}
 CHECK(status(XIDE_NATIVE_RETIRED,&s)&&s.candidate&&disk(plan,mode==0||mode==5));CHECK(xideNativeAsyncDrop(current,currentToken)&&status(XIDE_NATIVE_IDLE,&s)&&s.retiredThread==workerId&&!s.candidate&&!ownerReads&&!ownerWaits);normal=0;CHECK(xideNativeAsyncDestroy(current)==XUI_OK);current=NULL;
destroyed:
 currentToken=0;wchar_t* native=xidePathNativeWide(f->paths[4]);CHECK(native);HANDLE releasedDependency=CreateFileW(native,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);free(native);CHECK(releasedDependency!=INVALID_HANDLE_VALUE&&CloseHandle(releasedDependency));xideWorkspaceEditRelease(retained);
 if(mode!=6){CHECK(xideDocumentsResumeSources(&store,documents,NULL)&&xideLanguageClientResumeAnalysis(client,lease));until=GetTickCount64()+15000;while(!xideLanguageClientAnalysisCanRelease(client,lease)){CHECK(GetTickCount64()<until);Sleep(1);}CHECK(xideLanguageClientReleaseAnalysis(client,lease));}
 xideLanguageClientStop(client);client=NULL;xlsSyncDestroy(f->sync);xrtValueRelease(f->catalog);xideDocumentsClear(&store);xuiDestroy(context);free(f->uri);for(size_t i=0;i<8;++i)free(f->paths[i]);free(recovery);return 1;
}
int main(int argc,char** argv)
{if(argc!=3)return 2;wchar_t oldPath[32768];DWORD oldLength=GetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",oldPath,32768);if(oldLength>=32768||!SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",NULL))return 2;owner=GetCurrentThreadId();entered=CreateEventW(NULL,TRUE,FALSE,NULL);released=CreateEventW(NULL,TRUE,FALSE,NULL);if(!entered||!released)return 2;for(int mode=0;mode<9;++mode){char name[30];snprintf(name,sizeof(name),"async-%d",mode);char* path=xidePathResolve(argv[1],name);if(!path||!one(path,argv[2],mode)){free(path);SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",oldLength?oldPath:NULL);return 1;}free(path);}CloseHandle(entered);CloseHandle(released);if(!SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",oldLength?oldPath:NULL))return 2;printf("PASS native async driver: %u checks; real RPC/mixed edits, persistent XRT worker, no owner source reads or waits, held positive read/write/destruction, cancellation undo, exact UI outcome, metadata retirement, final shutdown and journal uncertainty\n",checks);return 0;}
