#define main legacyEditScopeMain
#include "edit_request_view_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"native-edit-view:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
typedef struct async_view_fixture {const char* directory;const char* compiler;HDESK desktop;int mode,started;} async_view_fixture;
static DWORD uiOwner,nativeWorker;static HANDLE sourceEntered,sourceRelease;
static volatile LONG pauseRead,pauseWrite,failCommit,failJobs,failWatch;
static volatile LONG prepareCalls,applyCalls,finishCalls,ownerReads,ownerWaits,watchFailures;
static unsigned resumeCalls,candidateLeaseChecks;
static uint32_t sourceVolume;static uint64_t sourceIndex;static int normalApi;
static void sourceHeld(void)
{SetEvent(sourceEntered);if(WaitForSingleObject(sourceRelease,30000)!=WAIT_OBJECT_0)ExitProcess(74);}
extern bool(*__real___imp_xrtReadFull)(xfile,ptr,size_t,size_t*);
static bool readProbe(xfile file,ptr bytes,size_t count,size_t* done)
{if(normalApi&&GetCurrentThreadId()==uiOwner)InterlockedIncrement(&ownerReads);bool okay=__real___imp_xrtReadFull(file,bytes,count,done);if(okay&&done&&*done&&GetCurrentThreadId()==nativeWorker&&InterlockedCompareExchange(&pauseRead,0,1)==1)sourceHeld();return okay;}
bool(*__wrap___imp_xrtReadFull)(xfile,ptr,size_t,size_t*)=readProbe;
extern xwaitresult(*__real___imp_xrtCondWait)(xcond*,xmutex*);
static xwaitresult waitProbe(xcond* cond,xmutex* lock)
{if(normalApi&&GetCurrentThreadId()==uiOwner)InterlockedIncrement(&ownerWaits);return __real___imp_xrtCondWait(cond,lock);}
xwaitresult(*__wrap___imp_xrtCondWait)(xcond*,xmutex*)=waitProbe;
extern BOOL(WINAPI* __real___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
static BOOL WINAPI writeProbe(HANDLE file,LPCVOID bytes,DWORD count,LPDWORD done,LPOVERLAPPED at)
{BOOL okay=__real___imp_WriteFile(file,bytes,count,done,at);BY_HANDLE_FILE_INFORMATION info={0};if(okay&&done&&*done&&pauseWrite&&GetFileInformationByHandle(file,&info)&&info.dwVolumeSerialNumber==sourceVolume&&(((uint64_t)info.nFileIndexHigh<<32)|info.nFileIndexLow)==sourceIndex&&InterlockedCompareExchange(&pauseWrite,0,1)==1)sourceHeld();return okay;}
BOOL(WINAPI* __wrap___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED)=writeProbe;
extern BOOL(WINAPI* __real___imp_FlushFileBuffers)(HANDLE);
static BOOL WINAPI flushProbe(HANDLE file)
{wchar_t path[4096];DWORD n=GetFinalPathNameByHandleW(file,path,4096,0);if(failCommit&&n&&n<4096&&wcsstr(path,L"edit-transaction.log")&&InterlockedCompareExchange(&failCommit,0,1)==1){SetLastError(ERROR_WRITE_FAULT);return FALSE;}return __real___imp_FlushFileBuffers(file);}
BOOL(WINAPI* __wrap___imp_FlushFileBuffers)(HANDLE)=flushProbe;
xide_native_edit* __real_xideNativeEditPrepare(xide_edit_candidate*,const char*,const xide_edit_alias_exclusion*,size_t,size_t,int(*)(void*),void*,xide_model_error*);
xide_native_edit* __wrap_xideNativeEditPrepare(xide_edit_candidate* c,const char* dir,const xide_edit_alias_exclusion* paths,size_t n,size_t budget,int(*stop)(void*),void* user,xide_model_error* error)
{if(GetCurrentThreadId()==uiOwner)ExitProcess(75);nativeWorker=GetCurrentThreadId();InterlockedIncrement(&prepareCalls);return __real_xideNativeEditPrepare(c,dir,paths,n,budget,stop,user,error);}
int __real_xideNativeEditApply(xide_native_edit*,int(*)(void*,size_t,int),void*,int(*)(void*),void*,xide_model_error*);
int __wrap_xideNativeEditApply(xide_native_edit* work,int(*guard)(void*,size_t,int),void* user,int(*stop)(void*),void* cancelUser,xide_model_error* error)
{if(GetCurrentThreadId()==uiOwner)ExitProcess(76);InterlockedIncrement(&applyCalls);return __real_xideNativeEditApply(work,guard,user,stop,cancelUser,error);}
int __real_xideNativeEditFinish(xide_native_edit*,int(*)(void*,size_t,int),void*,xide_model_error*);
static int failNextCommit;
int __wrap_xideNativeEditFinish(xide_native_edit* work,int(*guard)(void*,size_t,int),void* user,xide_model_error* error)
{if(GetCurrentThreadId()==uiOwner)ExitProcess(77);InterlockedIncrement(&finishCalls);if(failNextCommit){xide_edit_transaction_status s={0};if(xideNativeEditStatus(work,&s)&&s.committed){failCommit=1;failNextCommit=0;}}return __real_xideNativeEditFinish(work,guard,user,error);}
uint64_t __real_xideSourceJobsSuspend(xide_source_jobs*,xide_model_error*);
uint64_t __wrap_xideSourceJobsSuspend(xide_source_jobs* jobs,xide_model_error* error)
{if(InterlockedCompareExchange(&failJobs,0,1)==1){if(error){error->code=XIDE_MODEL_MEMORY;snprintf(error->message,sizeof(error->message),"Controlled source-job prefix failure.");}return 0;}return __real_xideSourceJobsSuspend(jobs,error);}
int __real_xideFileSyncResume(xide_app*,uint64_t,xide_model_error*);
int __wrap_xideFileSyncResume(xide_app* app,uint64_t token,xide_model_error* error)
{if(app->languageView&&app->languageView->nativeEdit&&app->languageView->nativeEdit->hold){xide_native_async_status s={0};CHECK(xideNativeAsyncStatus(app->languageView->nativeEdit->worker,&s)&&s.state==XIDE_NATIVE_RETIRED&&!s.active&&s.candidate);++candidateLeaseChecks;}if(InterlockedCompareExchange(&failWatch,0,1)==1){InterlockedIncrement(&watchFailures);if(error){error->code=XIDE_MODEL_MEMORY;snprintf(error->message,sizeof(error->message),"Controlled watch resubscription failure.");}return 0;}return __real_xideFileSyncResume(app,token,error);}
int __real_xideSourceBarrierResume(xide_source_barrier*,uint64_t,xide_model_error*);
int __wrap_xideSourceBarrierResume(xide_source_barrier* barrier,uint64_t token,xide_model_error* error)
{++resumeCalls;return __real_xideSourceBarrierResume(barrier,token,error);}
static void awaitSource(xide_app* app)
{ULONGLONG until=GetTickCount64()+20000;while(WaitForSingleObject(sourceEntered,0)!=WAIT_OBJECT_0){CHECK(frame(app)&&GetTickCount64()<until);Sleep(1);}CHECK(nativeWorker&&nativeWorker!=uiOwner);}
static DWORD WINAPI releaseLater(void* unused)
{(void)unused;Sleep(30);SetEvent(sourceRelease);return 0;}
static const xide_edit_candidate* nextCandidate(xide_app* app,xide_document* document)
{
 xide_edit_request* request=capture(app,document);CHECK(request);xls_sync_binding binding={0};CHECK(xlsSyncBindingAt(xideEditRequestSync(request),0,&binding));char* uri=copy(binding.uri);xls_text* text=xlsSyncRetainText(xideEditRequestSync(request),binding.uri,binding.version);const char* name=text?strstr(xlsTextData(text),"next_amount"):NULL;xls_position position;CHECK(uri&&name&&xlsTextPosition(text,(size_t)(name-xlsTextData(text)),&position));xlsTextRelease(text);end(app,request);CHECK(xideLanguageEditCandidateBegin(app,uri,(int)position.line,(int)position.character,xrtStrView("final_amount"),NULL));free(uri);ULONGLONG until=GetTickCount64()+20000;const xide_edit_candidate* candidate=NULL;while(!(candidate=xideLanguageEditCandidate(app))){CHECK(frame(app)&&app->languageView->editManaged&&GetTickCount64()<until);Sleep(1);}return candidate;
}
static void checkDisk(char** paths,const char* mainBody,const char* closedBody,const char* sharedBody,int after)
{int saved=normalApi;normalApi=0;const size_t rows[]={0,1,2,3,5};for(size_t i=0;i<5;++i){xide_text_file file={0};size_t at=rows[i];CHECK(xideTextFileRead(paths[at],&file,NULL));const char* expected=at==0?mainBody:at==2?sharedBody:closedBody;if(!after||at==0)CHECK(!strcmp(file.text,expected));else CHECK(strstr(file.text,after==2?"final_amount":"next_amount")&&!strstr(file.text,"shared.amount(")&&!strstr(file.text,"func amount("));xideTextFileClear(&file);}normalApi=saved;}
static void standaloneManaged(xide_app* app,const char* source,const char* native,const char* cBody)
{
 xide_model_error e={0};const char* body="func main(...) as int\n int amount = 1\n return amount\nend\n";
 CHECK(!app->session&&!app->workspaceModel.projectCount&&!app->workspaceModel.manifest&&xrtFileWriteAtomic(source,(xbytesview){(const unsigned char*)body,strlen(body)})&&xideEditorOpen(app,native,&e));xide_document* c=app->activeDocument;CHECK(xideEditorOpen(app,source,&e));xide_document* xl=app->activeDocument;
 const xide_edit_candidate* candidate=controllerCandidate(app,xl);xide_workspace_edit_info info;CHECK(candidate&&xideWorkspaceEditInfo(xideEditCandidatePlan(candidate),&info)&&info.documents==1&&info.edits==2);normalApi=1;
 uint64_t token=xideLanguageEditApplyBeginManaged(app,0,&e);xide_native_apply_status status;CHECK(token&&xideLanguageEditApplyStatus(app,&status)&&status.phase==XIDE_APPLY_CONFIRM&&!xidePathExists(status.directory)&&xideLanguageEditApplyConfirm(app,token,&e));ULONGLONG until=GetTickCount64()+30000;
 do{CHECK(frame(app)&&xideLanguageEditApplyStatus(app,&status)&&GetTickCount64()<until);if(!status.active)break;Sleep(1);}while(1);
 CHECK(status.phase==XIDE_APPLY_DONE&&status.transaction.committed&&!status.barrierHeld&&!ownerReads&&!ownerWaits&&strstr(xuiCodeDocumentGetText(xuiCodeEditGetDocument(xl->editor)),"next_amount"));normalApi=0;
 xide_text_file disk={0};CHECK(xideTextFileRead(source,&disk,NULL)&&!strcmp(disk.text,body));xideTextFileClear(&disk);input(app,c,cBody);char* kept=copy(xuiCodeDocumentGetText(xuiCodeEditGetDocument(xl->editor)));CHECK(kept);input(app,xl,kept);free(kept);
}
#include "symbol_rename_ui_test.inc"
static int asyncVerify(void* raw)
{
 async_view_fixture* f=raw;if(f->started)return XGE_OK;f->started=1;uiOwner=GetCurrentThreadId();xide_app app={.hidden=1,.noSession=1,.compilerPath=f->compiler};xide_model_error error={0};
 const char* names[]={"main.xl","second.xl","shared.xl","unopened.xl","isolated.xl","solo.xl","other.c"};char* paths[7];for(size_t i=0;i<7;++i){paths[i]=xidePathResolve(f->directory,names[i]);CHECK(paths[i]);}
 const char *mainBody="import shared\nfunc main(...) as int\n return shared.amount(1)\nend\n",*closedBody="import shared\nfunc main(...) as int\n return shared.amount(2)\nend\n",*sharedRaw="\xef\xbb\xbf" "export func amount(int value) as int\r\n return value\r\nend\r\n",*sharedBody="export func amount(int value) as int\n return value\nend\n",*isolated="func main(...) as int\n return 0\nend\n",*cBody="int main(void) { return 0; }\n";
 const char* bodies[]={mainBody,closedBody,sharedRaw,closedBody,isolated,closedBody,cBody};for(size_t i=0;i<7;++i)CHECK(xrtFileWriteAtomic(paths[i],(xbytesview){(const unsigned char*)bodies[i],strlen(bodies[i])}));
 char *settings=xidePathResolve(f->directory,"absent-settings.json"),*session=xidePathResolve(f->directory,"session"),*diagnostics=xidePathResolve(f->directory,"diagnostics"),*recovery=xidePathResolve(f->directory,"edit-recovery");CHECK(settings&&session&&diagnostics&&recovery);app.settingsPath=settings;app.sessionRoot=session;app.diagnosticRoot=diagnostics;CHECK(xideAppCreate(&app)==XUI_OK&&xgeBegin()==XGE_OK&&xideTestCatalogReady(&app));
 if(f->mode==23){const char* local="func main(...) as int\n int amount = 1\n return amount\nend\n";CHECK(xrtFileWriteAtomic(paths[0],(xbytesview){(const unsigned char*)local,strlen(local)})&&xideEditorOpen(&app,paths[6],&error));xide_document* native=app.activeDocument;CHECK(xideEditorOpen(&app,paths[0],&error));symbolUiScenario(&app,app.activeDocument,native,paths,f->directory,local,cBody,f->mode);xgeEnd();xideAppDestroy(&app);goto closed;}
 if(f->mode==14){standaloneManaged(&app,paths[0],paths[6],cBody);xgeEnd();xideAppDestroy(&app);goto closed;}
 CHECK(xideWorkspaceCreate(&app.workspaceModel,"Native async integration",&error));app.workspaceModel.projects=calloc(3,sizeof(xide_project));CHECK(app.workspaceModel.projects);app.workspaceModel.projectCount=app.workspaceModel.projectCapacity=3;
 const char* ids[]={"A","B","C"};const size_t entries[]={0,1,4};for(size_t i=0;i<3;++i){char name[20];snprintf(name,sizeof(name),"%s.xlp",ids[i]);char* manifest=xidePathResolve(f->directory,name);xide_project* p=app.workspaceModel.projects+i;CHECK(manifest&&xideProjectCreate(p,manifest,ids[i],paths[entries[i]],&error));p->id=copy(ids[i]);CHECK(p->id);if(i<2)CHECK(xideProjectAddFile(p,paths[2],"source",&error));if(i==1)CHECK(xideProjectAddFile(p,paths[3],"source",&error));free(manifest);}
 CHECK(xideWorkspaceAddFile(&app.workspaceModel,paths[5],&error)&&xideWorkspaceViewRefresh(&app)==XUI_OK&&xideWorkspaceViewSelectProject(&app,"A")&&xideEditorOpen(&app,paths[6],&error));xide_document* c=app.activeDocument;CHECK(xideEditorOpen(&app,paths[0],&error));xide_document* xl=app.activeDocument;
 if(f->mode>=15){symbolUiScenario(&app,xl,c,paths,f->directory,mainBody,cBody,f->mode);if(f->mode==26){xide_native_edit_async* first=app.languageView->nativeEdit->worker;uint64_t old=app.languageView->nativeEdit->token;char* scope=copy(app.languageView->nativeEdit->directory);CHECK(scope);nextSymbolName="final_amount";symbolUiScenario(&app,xl,c,paths,f->directory,mainBody,cBody,f->mode);CHECK(app.languageView->nativeEdit->worker==first&&app.languageView->nativeEdit->token>old&&strcmp(scope,app.languageView->nativeEdit->directory));free(scope);}xgeEnd();xideAppDestroy(&app);if(f->mode==20)checkDisk(paths,mainBody,closedBody,sharedBody,1);goto closed;}
 const xide_edit_candidate* candidate=controllerCandidate(&app,xl);xide_workspace_edit_info info;CHECK(candidate&&xideWorkspaceEditInfo(xideEditCandidatePlan(candidate),&info)&&info.documents==5&&info.edits==5);prepareCalls=applyCalls=finishCalls=ownerReads=ownerWaits=watchFailures=0;normalApi=1;
 if(f->mode==12){free(app.preferences.path);app.preferences.path=copy(paths[2]);CHECK(app.preferences.path);}
 if(f->mode==13)app.sessionRoot=f->directory;
 uint64_t token=f->mode>=9?xideLanguageEditApplyBeginManaged(&app,0,&error):xideLanguageEditApplyBegin(&app,recovery,NULL,0,0,&error);xide_native_apply_status status;CHECK(token&&xideLanguageEditApplyStatus(&app,&status)&&status.phase==XIDE_APPLY_CONFIRM&&!status.barrierHeld&&xideLanguageEditApplyPlan(&app,token)==xideEditCandidatePlan(candidate)&&!prepareCalls&&!xideSourceJobsHeld(&app.sourceJobs)&&!xideDocumentsSourcesHeld(&app.documents));CHECK(!xideLanguageEditApplyBegin(&app,recovery,NULL,0,0,&error)&&!xideLanguageEditApplyConfirm(&app,token+1,&error)&&!xideLanguageEditApplyCancel(&app,token+1));
 if(f->mode>=9){CHECK(app.languageView->nativeEdit->protection&&!xidePathExists(app.languageView->nativeEdit->directory)&&!ownerReads&&!ownerWaits);}
 if(f->mode==10){app.sessionRoot=diagnostics;CHECK(!xideLanguageEditApplyConfirm(&app,token,&error)&&error.code&&!prepareCalls&&!xideSourceJobsHeld(&app.sourceJobs));app.sessionRoot=session;CHECK(xideLanguageEditApplyCancel(&app,token));}
 else if(f->mode==3)CHECK(xideLanguageEditApplyCancel(&app,token));
 else if(f->mode==5){xide_document_changed_proc saved=app.documents.onChanged;app.documents.onChanged=NULL;CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"// suppressed callback\n")==XUI_OK&&!xideLanguageEditApplyConfirm(&app,token,&error));app.documents.onChanged=saved;}
 else {
  if(f->mode==1||f->mode==4||f->mode==11){xfileinfo file={0};CHECK(xideIoReceiptStat(paths[2],&file));sourceVolume=(uint32_t)file.Device;sourceIndex=file.Identity;pauseWrite=1;}if(f->mode==2)failNextCommit=1;if(f->mode==6)failJobs=1;if(f->mode==7)failWatch=1;
  CHECK(xideLanguageEditApplyConfirm(&app,token,&error)&&xideLanguageEditApplyStatus(&app,&status)&&status.barrierHeld);
  if(f->mode==8){CHECK(app.sourceJobs.token&&app.sourceJobs.token<UINT64_MAX);++app.sourceJobs.token;}
  if(f->mode==1||f->mode==4||f->mode==11){awaitSource(&app);CHECK(xideLanguageEditApplyStatus(&app,&status)&&status.barrierHeld&&status.phase==XIDE_APPLY_WRITING&&xideSourceJobsHeld(&app.sourceJobs)&&xideDocumentsSourcesHeld(&app.documents));
   if(f->mode==4){normalApi=0;HANDLE later=CreateThread(NULL,0,releaseLater,NULL,0,NULL);CHECK(later);xgeEnd();xideAppDestroy(&app);CHECK(WaitForSingleObject(later,10000)==WAIT_OBJECT_0&&CloseHandle(later)&&!resumeCalls);checkDisk(paths,mainBody,closedBody,sharedBody,0);goto closed;}
   if(f->mode==11)app.sessionRoot=diagnostics;else {input(&app,c,cBody);xide_edit_controller_status producer={0};CHECK(xideEditControllerStatus(app.languageView->editController,&producer)&&producer.state==XIDE_EDIT_READY&&producer.paused&&!producer.active&&app.languageView->editManaged&&!xideLanguageEditCandidate(&app));}CHECK(xideLanguageEditApplyStatus(&app,&status)&&status.active&&status.barrierHeld&&SetEvent(sourceRelease));
  }
 }
 ULONGLONG until=GetTickCount64()+30000;do{CHECK(frame(&app)&&xideLanguageEditApplyStatus(&app,&status)&&GetTickCount64()<until);if(!status.active||status.phase==XIDE_APPLY_RECOVERY)break;Sleep(1);}while(1);
 if(f->mode==2||f->mode==8){CHECK(status.phase==XIDE_APPLY_RECOVERY&&status.barrierHeld);if(f->mode==2)CHECK(status.transaction.committed&&status.transaction.needsRecovery&&finishCalls==1&&!failCommit);else CHECK(!status.transaction.committed&&!prepareCalls&&!applyCalls&&!finishCalls&&status.error.code);for(int i=0;i<4;++i)CHECK(frame(&app)&&xideLanguageEditApplyStatus(&app,&status)&&status.phase==XIDE_APPLY_RECOVERY&&status.barrierHeld);CHECK(xideLanguageEditApplyCancel(&app,token)&&frame(&app)&&xideLanguageEditApplyStatus(&app,&status)&&status.barrierHeld&&!xideLanguageEditApplyBegin(&app,recovery,NULL,0,0,&error));normalApi=0;uint64_t foreign=app.sourceJobs.token;unsigned before=resumeCalls;xgeEnd();xideAppDestroy(&app);CHECK(resumeCalls==before&&(f->mode!=8||app.sourceJobs.token==foreign));checkDisk(paths,mainBody,closedBody,sharedBody,f->mode==2);goto closed;}
 int committed=f->mode==0||f->mode==7||f->mode==9;CHECK(status.phase==(committed?XIDE_APPLY_DONE:(f->mode==11||f->mode==12||f->mode==13)?XIDE_APPLY_FAILED:XIDE_APPLY_CANCELLED)&&status.transaction.committed==committed&&!status.transaction.needsRecovery&&!status.barrierHeld&&!xideSourceJobsHeld(&app.sourceJobs)&&!xideDocumentsSourcesHeld(&app.documents)&&!ownerReads&&!ownerWaits);CHECK(!committed||strstr(xuiCodeDocumentGetText(xuiCodeEditGetDocument(xl->editor)),"next_amount"));CHECK(committed||!strcmp(xuiCodeDocumentGetText(xuiCodeEditGetDocument(xl->editor)),mainBody));if(f->mode==3||f->mode==5||f->mode==6)CHECK(!prepareCalls&&!applyCalls);if(f->mode==1)CHECK(app.languageView->nativeEdit->transaction.rolledBack);if(f->mode==7)CHECK(watchFailures==1);normalApi=0;checkDisk(paths,mainBody,closedBody,sharedBody,committed);if(f->mode==0||f->mode==9){xide_native_edit_async* sameWorker=app.languageView->nativeEdit->worker;DWORD sameThread=nativeWorker;uint64_t previousSerial=app.languageView->nativeEdit->barrier.serial;const xide_edit_candidate* again=nextCandidate(&app,xl);CHECK(again&&xideWorkspaceEditInfo(xideEditCandidatePlan(again),&info)&&info.documents==5);char* second=xidePathResolve(f->directory,"edit-recovery-2");CHECK(second);normalApi=1;char* firstScope=copy(status.directory);CHECK(firstScope);uint64_t next=f->mode==9?xideLanguageEditApplyBeginManaged(&app,0,&error):xideLanguageEditApplyBegin(&app,second,NULL,0,0,&error);CHECK(f->mode!=9||strcmp(firstScope,app.languageView->nativeEdit->directory));free(firstScope);CHECK(next>token&&app.languageView->nativeEdit->worker==sameWorker&&xideLanguageEditApplyConfirm(&app,next,&error));until=GetTickCount64()+30000;do{CHECK(frame(&app)&&xideLanguageEditApplyStatus(&app,&status)&&GetTickCount64()<until);if(!status.active)break;Sleep(1);}while(1);CHECK(status.phase==XIDE_APPLY_DONE&&status.transaction.committed&&!status.barrierHeld&&app.languageView->nativeEdit->barrier.serial>previousSerial&&nativeWorker==sameThread&&strstr(xuiCodeDocumentGetText(xuiCodeEditGetDocument(xl->editor)),"final_amount")&&!ownerReads&&!ownerWaits);normalApi=0;checkDisk(paths,mainBody,closedBody,sharedBody,2);free(second);input(&app,c,cBody);const char* now=xuiCodeDocumentGetText(xuiCodeEditGetDocument(xl->editor));char* kept=copy(now);CHECK(kept);input(&app,xl,kept);free(kept);}xgeEnd();xideAppDestroy(&app);
closed:
 for(size_t i=0;i<7;++i)free(paths[i]);
 free(settings);free(session);free(diagnostics);free(recovery);xgeQuit();return XGE_OK;
}
static DWORD WINAPI asyncGui(void* raw)
{async_view_fixture* f=raw;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t desc={.iWidth=1280,.iHeight=800,.sTitle="Native edit async",.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI,.iRunMode=XGE_RUN_GAME_LOOP};int result=xgeInit(&desc);if(result==XGE_OK)result=xgeRun(asyncVerify,f);xgeUnit();return result==XGE_OK&&f->started?0:1;}
#ifndef XIDE_NATIVE_EDIT_MAIN
#define XIDE_NATIVE_EDIT_MAIN main
#endif
int XIDE_NATIVE_EDIT_MAIN(int argc,char** argv)
{if(argc!=4||!xideIoCreateDirectories(argv[1]))return 2;sourceEntered=CreateEventW(NULL,TRUE,FALSE,NULL);sourceRelease=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(sourceEntered&&sourceRelease);async_view_fixture f={.directory=argv[1],.compiler=argv[2],.mode=atoi(argv[3])};CHECK(f.mode>=0&&f.mode<29);wchar_t name[80];swprintf(name,80,L"NativeEdit-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);CHECK(f.desktop);HANDLE thread=CreateThread(NULL,0,asyncGui,&f,0,NULL);CHECK(thread&&WaitForSingleObject(thread,180000)==WAIT_OBJECT_0);DWORD code=1;CHECK(GetExitCodeThread(thread,&code)&&!code&&CloseHandle(thread));CloseHandle(sourceEntered);CloseHandle(sourceRelease);if(!CloseDesktop(f.desktop))CHECK(GetLastError()==ERROR_BUSY);printf("PASS native edit IDE mode %d: %u checks; actual candidate, explicit consent, six-reader barrier, asynchronous mixed transaction and reader-safe teardown\n",f.mode,checks);return 0;}
