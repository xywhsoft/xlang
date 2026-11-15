#define _WIN32_WINNT 0x0601
#include "services/rename_prepare.h"
static int prepareReceiveProbe(xide_rename_prepare*,xvalue*,size_t);
#define xideRenamePrepareReceive prepareReceiveProbe
#define XIDE_NATIVE_EDIT_MAIN nativeRenameExistingMain
#include "native_edit_view_test.c"
#undef XIDE_NATIVE_EDIT_MAIN
#undef xideRenamePrepareReceive
/* Delay a real response without blocking the transport or its shutdown.
 * This simulates an old network reply; only immutable metadata is retained. */
static CRITICAL_SECTION gateLock;static HANDLE gateEntered;static LONG gateMode;
static xide_rename_prepare* heldQueue;static xvalue* heldFrame;static size_t heldBytes;
static int prepareReceiveProbe(xide_rename_prepare* q,xvalue* m,size_t bytes)
{
 xstrview id={0};if(!xrtValueGetString(get(m,"id"),&id)||id.Size<19||memcmp(id.Data,"xide.prepareRename/",19))return xideRenamePrepareReceive(q,m,bytes);
 LONG mode=InterlockedExchange(&gateMode,0);
 if(mode==1){EnterCriticalSection(&gateLock);CHECK(!heldFrame);heldQueue=q;heldFrame=m;heldBytes=bytes;LeaveCriticalSection(&gateLock);CHECK(SetEvent(gateEntered));return 1;}
 xvalue* result=(xvalue*)get(m,"result");xvalue* range=(xvalue*)get(result,"range");
 if(mode==2)CHECK(put(result,"placeholder",xrtValueString(sv("tamper"))));
 if(mode==3){CHECK(put((xvalue*)get(range,"start"),"character",xrtValueInt(INT_MAX-16)));CHECK(put((xvalue*)get(range,"end"),"character",xrtValueInt(INT_MAX-10)));}
 if(mode==4){int start=0,end=0;CHECK(number(get(get(range,"start"),"character"),&start)&&number(get(get(range,"end"),"character"),&end)&&start>0);CHECK(put((xvalue*)get(range,"start"),"character",xrtValueInt(start-1))&&put((xvalue*)get(range,"end"),"character",xrtValueInt(end-1)));}
 return xideRenamePrepareReceive(q,m,bytes);
}
static DWORD WINAPI deliverHeld(void* unused)
{(void)unused;EnterCriticalSection(&gateLock);xide_rename_prepare* q=heldQueue;xvalue* m=heldFrame;size_t n=heldBytes;heldQueue=NULL;heldFrame=NULL;LeaveCriticalSection(&gateLock);CHECK(m&&xideRenamePrepareReceive(q,m,n));return 0;}
static void releaseHeld(void)
{HANDLE t=CreateThread(NULL,0,deliverHeld,NULL,0,NULL);CHECK(t&&WaitForSingleObject(t,30000)==WAIT_OBJECT_0&&CloseHandle(t));}
static void awaitHeld(xide_app* a)
{ULONGLONG until=GetTickCount64()+30000;while(WaitForSingleObject(gateEntered,0)!=WAIT_OBJECT_0){CHECK(frame(a)&&GetTickCount64()<until);Sleep(1);}CHECK(xideSymbolRenamePhase(a)==XIDE_SYMBOL_PREPARING&&!xideSymbolRenameIsModal(a));}
static int prefillVerify(void* raw)
{
 async_view_fixture* f=raw;if(f->started)return XGE_OK;f->started=1;uiOwner=GetCurrentThreadId();
 xide_app app={.hidden=1,.noSession=1,.compilerPath=f->compiler,.disableAutomaticDiagnostics=f->mode==40||f->mode==45};xide_model_error error={0};char *main=xidePathResolve(f->directory,"main.xl"),*shared=xidePathResolve(f->directory,"shared.xl"),*native=xidePathResolve(f->directory,"native.c");CHECK(main&&shared&&native);
 int unicode=f->mode==37||f->mode==38,legacy=f->mode==39;
 const char *plain="import shared\nfunc main(...) as int\n return shared.amount(1)\nend\n",
  *local="func main(...) as int\n int amount = 1\n return amount\nend\n",
  *wide="\xef\xbb\xbf" "func main(...) as int\r\n int 数量😀 = 1\r\n return 数量😀\r\nend\r\n",
  *sharedBody="export func amount(int value) as int\n return value\nend\n",*cBody="int main(void) { return 0; }\n",*body=unicode?wide:legacy?local:plain;
 CHECK(xrtFileWriteAtomic(main,(xbytesview){(const unsigned char*)body,strlen(body)})&&xrtFileWriteAtomic(shared,(xbytesview){(const unsigned char*)sharedBody,strlen(sharedBody)})&&xrtFileWriteAtomic(native,(xbytesview){(const unsigned char*)cBody,strlen(cBody)}));
 char* settings=xidePathResolve(f->directory,"missing-settings.json");char* diagnostic=xidePathResolve(f->directory,"diagnostics");app.settingsPath=settings;app.sessionRoot=f->directory;app.diagnosticRoot=diagnostic;CHECK(settings&&diagnostic&&xideAppCreate(&app)==XUI_OK&&xgeBegin()==XGE_OK&&xideTestCatalogReady(&app));
 if(!unicode&&!legacy){CHECK(xideWorkspaceCreate(&app.workspaceModel,"未打开声明",&error)&&xideWorkspaceAddFile(&app.workspaceModel,main,&error)&&xideWorkspaceAddFile(&app.workspaceModel,shared,&error)&&xideWorkspaceViewRefresh(&app)==XUI_OK);}
 CHECK(xideEditorOpen(&app,native,&error));xide_document* c=app.activeDocument;CHECK(xideEditorOpen(&app,main,&error));xide_document* xl=app.activeDocument;
 xide_edit_request* initial=capture(&app,xl);CHECK(initial);end(&app,initial);CHECK(frame(&app));
 const char* text=xuiCodeEditGetText(xl->editor);char* original=copy(text);const char* word=unicode?"数量😀":"amount";const char* at=strstr(original,word);CHECK(at);int caret=(int)(at-original)+(f->mode==38?6:unicode?3:2);
 CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(xl->editor),xuiCodeEditGetDocument(xl->editor),caret,0)==XUI_OK&&xuiSetFocusWidget(app.context,xl->editor)==XUI_OK);
 if((f->mode>=30&&f->mode<=34)||f->mode>=41)InterlockedExchange(&gateMode,1);
 if(f->mode==35)InterlockedExchange(&gateMode,2);
 if(f->mode==36)InterlockedExchange(&gateMode,3);
 if(f->mode==38)InterlockedExchange(&gateMode,4);
 prepareCalls=applyCalls=finishCalls=ownerReads=ownerWaits=0;normalApi=1;CHECK(xideSymbolRenameOpen(&app));
 if(legacy){CHECK(!app.languageView->prepareSupported&&xideSymbolRenamePhase(&app)==XIDE_SYMBOL_INPUT&&!*xuiEditGetText(xideSymbolRenameControl(&app,XIDE_SYMBOL_NAME)));}
 else CHECK(app.languageView->prepareSupported&&xideSymbolRenamePhase(&app)==XIDE_SYMBOL_PREPARING&&!xideSymbolRenameIsModal(&app));
 if((f->mode>=30&&f->mode<=34)||f->mode>=41){awaitHeld(&app);
  if(f->mode>=41){
   if(f->mode==41)CHECK(key(&app,XUI_KEY_ESCAPE,0)&&xideSymbolRenamePhase(&app)==XIDE_SYMBOL_CLOSED);
   if(f->mode==42)CHECK(xideSymbolRenameBeforeQuit(&app)==1&&xideSymbolRenamePhase(&app)==XIDE_SYMBOL_CLOSED);
   if(f->mode==45)CHECK(xideLanguageViewCanCancel(&app)&&xideLanguageViewAction(&app,XIDE_ACTION_CANCEL_CHECK)&&xideSymbolRenamePhase(&app)==XIDE_SYMBOL_CLOSED);
   if(f->mode==46){uint64_t token=xideDocumentsSuspendSources(&app.documents,&error);CHECK(token&&xideDocumentsResumeSources(&app.documents,token,&error)&&frame(&app)&&xideSymbolRenamePhase(&app)==XIDE_SYMBOL_RESULT);}
   if(f->mode==47)CHECK(xuiCodeEditSetReadonly(xl->editor,1)==XUI_OK&&frame(&app)&&xideSymbolRenamePhase(&app)==XIDE_SYMBOL_RESULT);
   if(f->mode==43||f->mode==44)CHECK(xuiSetFocusWidget(app.context,xl->editor)==XUI_OK&&frame(&app));
   releaseHeld();
   if(f->mode==43||f->mode==44){symbolWait(&app,XIDE_SYMBOL_INPUT);CHECK(xuiGetFocusWidget(app.context)==xl->editor&&!xideSymbolRenameIsModal(&app)&&!strcmp(xuiEditGetText(xideSymbolRenameControl(&app,XIDE_SYMBOL_NAME)),word));
    if(f->mode==44){CHECK(xuiEditSetText(xideSymbolRenameControl(&app,XIDE_SYMBOL_NAME),"typed_name")==XUI_OK);input(&app,xl,original);symbolWait(&app,XIDE_SYMBOL_RESULT);CHECK(!strcmp(xuiEditGetText(xideSymbolRenameControl(&app,XIDE_SYMBOL_NAME)),"typed_name"));}
   }else for(int i=0;i<4;++i)CHECK(frame(&app));
  }
  else if(f->mode==30){CHECK(symbolClick(&app,xideSymbolRenameControl(&app,XIDE_SYMBOL_CANCEL))&&xideSymbolRenamePhase(&app)==XIDE_SYMBOL_CLOSED&&xideSymbolRenameOpen(&app));symbolWait(&app,XIDE_SYMBOL_INPUT);CHECK(xuiEditSetText(xideSymbolRenameControl(&app,XIDE_SYMBOL_NAME),"typed_name")==XUI_OK);releaseHeld();for(int i=0;i<4;++i)CHECK(frame(&app));CHECK(!strcmp(xuiEditGetText(xideSymbolRenameControl(&app,XIDE_SYMBOL_NAME)),"typed_name"));}
  else {
   if(f->mode==31)input(&app,xl,original);
   if(f->mode==32)input(&app,c,cBody);
   if(f->mode==33)CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(xl->editor),xuiCodeEditGetDocument(xl->editor),caret+1,0)==XUI_OK);
   if(f->mode==34){CHECK(xideWorkspaceRemoveFile(&app.workspaceModel,app.workspaceModel.files.items[1].id,&error));xideLanguageViewConfigurationChanged(&app);}
   symbolWait(&app,XIDE_SYMBOL_RESULT);releaseHeld();for(int i=0;i<4;++i)CHECK(frame(&app));CHECK(!*xuiEditGetText(xideSymbolRenameControl(&app,XIDE_SYMBOL_NAME)));
  }
 }else if(f->mode==35||f->mode==36||f->mode==38){symbolWait(&app,XIDE_SYMBOL_RESULT);CHECK(!*xuiEditGetText(xideSymbolRenameControl(&app,XIDE_SYMBOL_NAME)));}
 else if(!legacy){symbolWait(&app,XIDE_SYMBOL_INPUT);CHECK(!strcmp(xuiEditGetText(xideSymbolRenameControl(&app,XIDE_SYMBOL_NAME)),word));if(unicode)symbolScreenshot(&app,f->directory,"unicode-prefill.png");}
 xide_rename_prepared leftover={0};CHECK(!xideRenamePrepareTake(app.languageView->renamePrepare,&leftover));
 CHECK(!prepareCalls&&!applyCalls&&!finishCalls&&!ownerReads&&!ownerWaits&&!xideSourceJobsHeld(&app.sourceJobs));
 if(f->mode==40){CHECK(xuiEditSetText(xideSymbolRenameControl(&app,XIDE_SYMBOL_NAME),"prefilled_next")==XUI_OK&&symbolClick(&app,xideSymbolRenameControl(&app,XIDE_SYMBOL_PREVIEW)));symbolWait(&app,XIDE_SYMBOL_CONFIRM);CHECK(!prepareCalls&&!applyCalls);}
 if(xideDialogIsOpen(xideSymbolRenameWindow(&app))){CHECK(symbolClick(&app,xideSymbolRenameControl(&app,XIDE_SYMBOL_CANCEL)));symbolWait(&app,XIDE_SYMBOL_CLOSED);}else CHECK(xideSymbolRenamePhase(&app)==XIDE_SYMBOL_CLOSED);
 CHECK(!strcmp(xuiCodeEditGetText(xl->editor),original)&&!strcmp(xuiCodeEditGetText(c->editor),cBody));normalApi=0;xide_text_file disk={0};CHECK(xideTextFileRead(main,&disk,&error)&&disk.rawLength==strlen(body)&&!memcmp(disk.raw,body,disk.rawLength));xideTextFileClear(&disk);free(original);xgeEnd();xideAppDestroy(&app);free(main);free(shared);free(native);free(settings);free(diagnostic);xgeQuit();return XGE_OK;
}
static DWORD WINAPI prefillWorker(void* raw)
{async_view_fixture* f=raw;CHECK(SetThreadDesktop(f->desktop));xge_desc_t d={.iWidth=1280,.iHeight=800,.sTitle="Prepare rename integration",.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI,.iRunMode=XGE_RUN_GAME_LOOP};int code=xgeInit(&d);if(code==XGE_OK)code=xgeRun(prefillVerify,f);xgeUnit();return code==XGE_OK&&f->started?0:1;}
int main(int argc,char** argv)
{CHECK(argc==4&&xideIoCreateDirectories(argv[1]));InitializeCriticalSection(&gateLock);gateEntered=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(gateEntered);async_view_fixture f={.directory=argv[1],.compiler=argv[2],.mode=atoi(argv[3])};CHECK(f.mode>=29&&f.mode<=47);wchar_t name[80];swprintf(name,80,L"RenamePrefill-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);CHECK(f.desktop);HANDLE t=CreateThread(NULL,0,prefillWorker,&f,0,NULL);CHECK(t&&WaitForSingleObject(t,180000)==WAIT_OBJECT_0);DWORD code=1;CHECK(GetExitCodeThread(t,&code)&&!code&&CloseHandle(t));CHECK(!heldFrame&&CloseHandle(gateEntered));CloseDesktop(f.desktop);DeleteCriticalSection(&gateLock);printf("PASS rename prefill IDE mode %d: %u checks; real semantic name,source/caret/config freshness,delayed/cancelled replies and editable C/xlang.\n",f.mode,checks);return 0;}
