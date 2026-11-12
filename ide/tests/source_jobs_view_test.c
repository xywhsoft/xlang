#define _WIN32_WINNT 0x0601
#include "ui/template_view.c"
#include "ui/recovery_view.h"
#include "services/session.h"
#include "services/file_sync.h"
#include "ui/language_view.h"
#include "ui/completion_view.h"
#include "catalog_ready.h"
static unsigned checks;
static DWORD owner;
static volatile LONG captureNext,tailTimeout;
static HANDLE entered,releaseTail;
static xide_document_changed_proc originalChanged;
static xide_app* observingApp;
static uint64_t observingId;
static unsigned publicationCallbacks,publicationSuspendAccepted;
static void publicationObserver(xide_document* document,void* user)
{
 if(observingApp&&document&&document->id>=observingId){
  ++publicationCallbacks;uint64_t token=xideSourceJobsSuspend(&observingApp->sourceJobs,NULL);
  if(token){++publicationSuspendAccepted;xideSourceJobsResume(&observingApp->sourceJobs,token,NULL);}
 }
 originalChanged(document,user);
}
static int fixtureWrite(const char* path,const char* text)
{return xideIoWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"source-jobs-ui:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
extern HANDLE(WINAPI* __real___imp_CreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
typedef struct invocation{LPTHREAD_START_ROUTINE proc;void* data;} invocation;
static DWORD WINAPI tail(void* p)
{
 invocation c=*(invocation*)p;free(p);DWORD result=c.proc(c.data);SetEvent(entered);
 if(WaitForSingleObject(releaseTail,20000)!=WAIT_OBJECT_0)tailTimeout=1;
 return result;
}
static HANDLE WINAPI createProbe(LPSECURITY_ATTRIBUTES a,SIZE_T stack,LPTHREAD_START_ROUTINE proc,LPVOID data,DWORD flags,LPDWORD id)
{
 if(GetCurrentThreadId()!=owner || !InterlockedCompareExchange(&captureNext,0,1))return __real___imp_CreateThread(a,stack,proc,data,flags,id);
 invocation* p=malloc(sizeof(*p));if(!p)return NULL;*p=(invocation){proc,data};HANDLE h=__real___imp_CreateThread(a,stack,tail,p,flags,id);if(!h)free(p);return h;
}
HANDLE(WINAPI* __wrap___imp_CreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD)=createProbe;
typedef struct fixture{const char *directory,*compiler;HDESK desktop;int started;} fixture;
static int frame(xide_app* a)
{if(xuiDispatchPendingEvents(a->context)!=XUI_OK)return 0;xideEditorPoll(a);xideFileSyncPoll(a);xideLanguageViewPoll(a);xideCompletionViewPoll(a);xideSessionPoll(a);xideWorkbenchLayout(a);return xuiLayout(a->context)==XUI_OK&&!a->frameError;}
static int key(xide_app* a,int code,uint32_t mods)
{return xuiInputKeyDown(a->context,code,mods)==XUI_OK&&xuiInputKeyUp(a->context,code,mods)==XUI_OK&&frame(a);}
static int ime(xide_app* a,const char* text,int active)
{xui_ime_composition_t c={.iSize=sizeof(c),.sText=text,.iTextSize=(int)strlen(text),.bActive=active};c.iCursor=c.iSelectionStart=c.iSelectionEnd=c.iTextSize;return xuiInputImeCompositionEx(a->context,&c)==XUI_OK&&frame(a);}
static int verify(void* user)
{
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;owner=GetCurrentThreadId();xide_model_error e={0};xide_app a={.hidden=1,.noSession=1,.disableAutomaticDiagnostics=1,.compilerPath=f->compiler};
 char *settings=xidePathResolveLexical(f->directory,"settings.json"),*session=xidePathResolveLexical(f->directory,"session"),*c=xidePathResolveLexical(f->directory,"main.c"),*xl=xidePathResolveLexical(f->directory,"main.xl"),*picture=xidePathResolveLexical(f->directory,"held-source-jobs.png");CHECK(settings&&session&&c&&xl&&picture);a.settingsPath=settings;a.sessionRoot=session;
 const char* bodies[]={"int value;\n","func value()\nend\n"};CHECK(fixtureWrite(c,bodies[0])&&fixtureWrite(xl,bodies[1]));
 CHECK(xideAppCreate(&a)==XUI_OK&&xgeBegin()==XGE_OK&&xideTestCatalogReady(&a));CHECK(xideEditorOpen(&a,c,&e));xide_document* docs[2]={a.activeDocument,NULL};CHECK(xideEditorOpen(&a,xl,&e));docs[1]=a.activeDocument;CHECK(frame(&a));
 /* Built-in file commands create a blank editor directly. Exercise the real
  * asynchronous extension-file route with a complete template manifest. */
 char* extension=xidePathResolveLexical(f->directory,"lease-file");char* manifest=extension?xidePathResolveLexical(extension,"template.json"):NULL;
 CHECK(extension&&manifest&&xideIoCreateDirectories(extension)&&fixtureWrite(manifest,"{\"format\":\"xlang-ide-template-1\",\"id\":\"lease.file\",\"title\":\"Lease file\",\"kind\":\"file\",\"language\":\"xlang\",\"files\":[{\"path\":\"main.xl\",\"content\":\"func lease()\\nend\\n\"}]}"));
 xide_template_registry registry={0};CHECK(xideTemplateRegistryLoad(&registry,f->directory,&e)&&registry.count==XIDE_TEMPLATE_BUILTIN_COUNT+1);CHECK(registry.items[XIDE_TEMPLATE_BUILTIN_COUNT].kind==XIDE_TEMPLATE_FILE&&xideTemplateViewOpenDefinition(&a,&registry.items[XIDE_TEMPLATE_BUILTIN_COUNT]));free(extension);free(manifest);
 size_t before=a.documents.count;captureNext=1;CHECK(xideTemplateViewApply(&a,&e)&&!captureNext&&WaitForSingleObject(entered,15000)==WAIT_OBJECT_0);
 xide_template_job* job=a.templateView->job;CHECK(job&&xideTemplateJobDone(job)&&xideTemplateJobSucceeded(job)&&!xideTemplateJobStopped(job));
 /* A normal Poll cannot join a worker whose immutable result is already
  * published but whose physical thread is still retained by the trampoline. */
 xideTemplateViewLayout(&a);CHECK(!tailTimeout&&a.templateView->job==job&&a.documents.count==before);
 uint64_t token=xideSourceJobsSuspend(&a.sourceJobs,&e);xide_source_jobs_status status={0};CHECK(token&&xideSourceJobsState(&a.sourceJobs,token,&status)&&!status.quiet&&!status.invalidated);
 CHECK(!xideTemplateViewApply(&a,&e)&&e.code==XIDE_MODEL_CONFLICT&&!xideRecoveryViewOpen(&a));xideTemplateViewLayout(&a);CHECK(!xuiWidgetGetEnabled(a.templateView->controls[XIDE_TEMPLATE_CREATE]));
 xideDialogSetOpen(a.templateView->window,0);
 for(int i=0;i<2;++i){xide_document* d=docs[i];CHECK(xideEditorShow(&a,d)==XUI_OK&&frame(&a));xui_rect_t r=xuiWidgetGetWorldRect(d->editor);float x=r.fX+100,y=r.fY+12;CHECK(xuiInputPointerMove(a.context,x,y,0)==XUI_OK&&xuiInputPointerDown(a.context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK&&xuiInputPointerUp(a.context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK&&frame(&a)&&xuiGetFocusWidget(a.context)==d->editor);
  xui_code_document core=xuiCodeEditGetDocument(d->editor);int end=xuiCodeDocumentGetLength(core);CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(d->editor),core,end,end)==XUI_OK&&xuiInputKeyDown(a.context,'Z',0)==XUI_OK&&xuiInputText(a.context,'z')==XUI_OK&&xuiInputKeyUp(a.context,'Z',0)==XUI_OK&&frame(&a));CHECK(strstr(xuiCodeDocumentGetText(core),"z")&&key(&a,XUI_KEY_BACKSPACE,0)&&!strcmp(xuiCodeDocumentGetText(core),bodies[i]));CHECK(key(&a,'Z',XUI_MOD_CTRL)&&strstr(xuiCodeDocumentGetText(core),"z")&&key(&a,'Z',XUI_MOD_CTRL)&&!strcmp(xuiCodeDocumentGetText(core),bodies[i]));
  CHECK(ime(&a,"中文😀",1)&&ime(&a,"中文😀",0)&&strstr(xuiCodeDocumentGetText(core),"中文😀")&&key(&a,'Z',XUI_MOD_CTRL)&&!strcmp(xuiCodeDocumentGetText(core),bodies[i]));
 }
 CHECK(!tailTimeout&&xideSourceJobsState(&a.sourceJobs,token,&status)&&!status.quiet&&a.documents.count==before);
 CHECK(SetEvent(releaseTail));ULONGLONG until=GetTickCount64()+15000;while(!xideTemplateJobStopped(job)){CHECK(GetTickCount64()<until);Sleep(1);}CHECK(xideSourceJobsState(&a.sourceJobs,token,&status)&&status.quiet&&!status.invalidated);
 CHECK(frame(&a)&&a.templateView->job==job&&a.documents.count==before);originalChanged=a.documents.onChanged;CHECK(originalChanged);observingApp=&a;observingId=a.documents.nextId;a.documents.onChanged=publicationObserver;
 CHECK(xideSourceJobsResume(&a.sourceJobs,token,&e));token=0;int publishedFrame=frame(&a);a.documents.onChanged=originalChanged;observingApp=NULL;
 fprintf(stderr,"post-resume: frame=%d job=%d count=%zu before=%zu registered=%zu message=%s\n",publishedFrame,a.templateView->job!=NULL,a.documents.count,before,a.sourceJobs.count,xideTemplateViewMessage(&a));CHECK(publishedFrame&&!a.templateView->job&&a.documents.count==before+1&&!a.sourceJobs.count&&publicationCallbacks&&!publicationSuspendAccepted);
 /* Cancellation during a held, already completed result revokes UI consent.
  * It neither loses completed files nor creates a new document on resume. */
 CHECK(xideTemplateViewOpenDefinition(&a,&registry.items[XIDE_TEMPLATE_BUILTIN_COUNT]));before=a.documents.count;
 ResetEvent(entered);ResetEvent(releaseTail);captureNext=1;CHECK(xideTemplateViewApply(&a,&e)&&WaitForSingleObject(entered,15000)==WAIT_OBJECT_0);job=a.templateView->job;CHECK(job&&xideTemplateJobSucceeded(job));token=xideSourceJobsSuspend(&a.sourceJobs,&e);CHECK(token);
 cancelForm(NULL,&a);CHECK(a.templateView->cancelRequested&&a.templateView->job==job);CHECK(SetEvent(releaseTail));until=GetTickCount64()+15000;while(!xideTemplateJobStopped(job)){CHECK(GetTickCount64()<until);Sleep(1);}CHECK(frame(&a)&&a.documents.count==before&&a.templateView->job==job);
 CHECK(xideSourceJobsResume(&a.sourceJobs,token,&e));token=0;CHECK(frame(&a)&&!a.templateView->job&&a.documents.count==before&&strstr(xideTemplateViewMessage(&a),"取消"));xideDialogSetOpen(a.templateView->window,0);
 /* Save a real workspace so this case exercises association consent without
  * triggering an unrelated dirty-editor close confirmation. */
 char* workspace=xidePathResolveLexical(f->directory,"jobs.xlw");CHECK(workspace&&xideWorkspaceCreate(&a.workspaceModel,"Jobs",&e)&&xideWorkspaceSave(&a.workspaceModel,workspace,&e));free(workspace);
 CHECK(xideTemplateViewOpenDefinition(&a,&registry.items[1]));CHECK(xuiEditSetText(xideTemplateViewField(&a,"project.name"),"cancelled-project")==XUI_OK&&xuiEditSetText(xideTemplateViewField(&a,"project.location"),f->directory)==XUI_OK&&xuiComboBoxSetSelected(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_MODE),2)==XUI_OK&&xideTemplateViewRefresh(&a));
 size_t projectsBefore=a.workspaceModel.projectCount;ResetEvent(entered);ResetEvent(releaseTail);captureNext=1;CHECK(xideTemplateViewApply(&a,&e)&&WaitForSingleObject(entered,15000)==WAIT_OBJECT_0);job=a.templateView->job;CHECK(job&&xideTemplateJobSucceeded(job)&&*xideTemplateJobProject(job));char* generated=xidePathResolveLexical(NULL,xideTemplateJobProject(job));CHECK(generated&&xideIoExists(generated));
 token=xideSourceJobsSuspend(&a.sourceJobs,&e);CHECK(token);cancelForm(NULL,&a);CHECK(SetEvent(releaseTail));until=GetTickCount64()+15000;while(!xideTemplateJobStopped(job)){CHECK(GetTickCount64()<until);Sleep(1);}CHECK(xideSourceJobsResume(&a.sourceJobs,token,&e));token=0;
 CHECK(frame(&a)&&!a.templateView->job&&a.workspaceModel.projectCount==projectsBefore&&a.documents.count==before&&xideIoExists(generated)&&strstr(xideTemplateViewMessage(&a),"保留"));free(generated);xideDialogSetOpen(a.templateView->window,0);
 CHECK(xideRecoveryViewOpen(&a));xui_widget restore=xideRecoveryViewControl(&a,XIDE_RECOVERY_GLOBAL_KIND);CHECK(xuiWidgetGetEnabled(restore));token=xideSourceJobsSuspend(&a.sourceJobs,&e);CHECK(token&&frame(&a)&&!xuiWidgetGetEnabled(restore));CHECK(xideSourceJobsResume(&a.sourceJobs,token,&e)&&frame(&a)&&xuiWidgetGetEnabled(restore));token=0;xideDialogSetOpen(xideRecoveryViewWindow(&a),0);
 xui_rect_i_t rect={0,0,a.viewportWidth,a.viewportHeight};CHECK(frame(&a)&&xuiUpdate(a.context,0)==XUI_OK&&a.proxy.surfaceClear(&a.proxy,a.target,0xffeeeeeeu)==XUI_OK&&xuiRender(a.context,a.target,&rect,1)==XUI_OK&&xideAppCapture(&a,picture)==XGE_OK);
 xideTemplateRegistryClear(&registry);xgeEnd();xideAppDestroy(&a);CHECK(!a.sourceJobs.count&&!a.sourceJobs.admissions);free(settings);free(session);free(c);free(xl);free(picture);xgeQuit();return XGE_OK;
}
static DWORD WINAPI gui(void* user)
{fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={.iWidth=1280,.iHeight=800,.sTitle="Source jobs lease",.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI,.iRunMode=XGE_RUN_GAME_LOOP};int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(verify,f);xgeUnit();return result==XGE_OK&&f->started?0:1;}
int main(int argc,char** argv)
{
 if(argc!=3||!xideIoCreateDirectories(argv[1])){return 2;}entered=CreateEventW(NULL,TRUE,FALSE,NULL);releaseTail=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(entered&&releaseTail);wchar_t name[80];swprintf(name,80,L"SourceJobs-%lu",GetCurrentProcessId());fixture f={.directory=argv[1],.compiler=argv[2]};f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);CHECK(f.desktop);HANDLE t=CreateThread(NULL,0,gui,&f,0,NULL);CHECK(t&&WaitForSingleObject(t,120000)==WAIT_OBJECT_0);DWORD code=1;CHECK(GetExitCodeThread(t,&code)&&!code);CloseHandle(t);if(!CloseDesktop(f.desktop))CHECK(GetLastError()==ERROR_BUSY);CloseHandle(entered);CloseHandle(releaseTail);printf("PASS source job IDE lease: %u checks; actual nonblocking template Poll,delayed result admission,recovery controls,C/xlang typing/delete/undo/IME/click focus\n",checks);return 0;
}
