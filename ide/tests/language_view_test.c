#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/language_view.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"language_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideLanguageViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideTaskViewPoll(a); xideLanguageViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int waitCheck(xide_app* a) { uint64_t until=GetTickCount64()+15000; do { if(!settle(a)) return 0; if(!xideLanguageViewBusy(a)) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int capture(xide_app* a,const char* p) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,p)==XGE_OK; }
static int resize(xide_app* a,int w,int h) { xui_surface_desc_t d={0}; d.iKind=XUI_SURFACE_KIND_TEXTURE; d.iWidth=w; d.iHeight=h; d.iFormat=XUI_SURFACE_FORMAT_RGBA8; d.iFlags=XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED; xui_surface target=NULL; if(a->proxy.surfaceCreate(&a->proxy,&target,&d)!=XUI_OK) return 0; a->proxy.surfaceDestroy(&a->proxy,a->target); a->target=target; a->viewportWidth=w; a->viewportHeight=h; return xuiInputViewport(a->context,(float)w,(float)h)==XUI_OK && settle(a); }
static int menuCheck(xide_app* a) { int index=-1; for(int i=0;i<xuiMenuGetItemCount(a->fileMenu);++i) if(xuiMenuGetItem(a->fileMenu,i)->iValue==XIDE_ACTION_CHECK_SAVED) index=i; if(index<0 || xuiMenuOpenAt(a->fileMenu,a->root,80,20)!=XUI_OK || !settle(a)) return 0; xui_rect_t r=xuiMenuGetItemRect(a->fileMenu,index),world=xuiWidgetGetWorldRect(a->fileMenu); float x=world.fX+r.fX+r.fW/2,y=world.fY+r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int token(xui_widget editor,const char* word,int kind) { const char* text=xuiCodeEditGetText(editor),*p=strstr(text,word); xui_code_token_t ts[256]; int count=0; if(!p || xuiCodeTokenBufferGetTokens(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),ts,256,&count)!=XUI_OK) return 0; for(int i=0;i<count;++i) if(ts[i].iStartOffset==(int)(p-text) && ts[i].iKind==kind) return 1; return 0; }
static int centerBottom(xide_app* a,xide_document* d) { xui_dock_window_info_t editor={.iSize=sizeof(editor)},problem={.iSize=sizeof(problem)}; xui_dock_pane_info_t ep={.iSize=sizeof(ep)},pp={.iSize=sizeof(pp)}; return xuiDockPanelGetWindowInfo(a->dock,d->window,&editor)==XUI_OK && xuiDockPanelGetWindowInfo(a->dock,a->windows[XIDE_PROBLEMS],&problem)==XUI_OK && xuiDockPanelGetPaneInfo(a->dock,editor.iPane,&ep)==XUI_OK && xuiDockPanelGetPaneInfo(a->dock,problem.iPane,&pp)==XUI_OK && ep.tRect.fX==pp.tRect.fX && ep.tRect.fW==pp.tRect.fW && pp.tRect.fY>=ep.tRect.fY+ep.tRect.fH; }
static int killOwnedServer(void)
{
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); if(snapshot==INVALID_HANDLE_VALUE) return 0;
 PROCESSENTRY32W process={.dwSize=sizeof(process)}; int killed=0;
 if(Process32FirstW(snapshot,&process)) do {
  if(process.th32ParentProcessID==GetCurrentProcessId() && !_wcsicmp(process.szExeFile,L"xlang-ls.exe")) {
   HANDLE owned=OpenProcess(PROCESS_TERMINATE|SYNCHRONIZE,FALSE,process.th32ProcessID); if(owned) { killed=TerminateProcess(owned,77) && WaitForSingleObject(owned,2000)==WAIT_OBJECT_0; CloseHandle(owned); } break;
  }
 } while(Process32NextW(snapshot,&process)); CloseHandle(snapshot); return killed;
}
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; int began=0; char* owned[30]={0}; size_t ownedCount=0;
 #define KEEP(s) (owned[ownedCount++]=(s))
 char unique[90]; snprintf(unique,sizeof(unique),"native-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 char *root=KEEP(xidePathResolve(f->directory,unique)),*tools=KEEP(xidePathResolve(root,"工具链😀")),*compiler=KEEP(xidePathResolve(tools,"xl.exe")),*mode=KEEP(xidePathResolve(root,"language-mode.txt"));
 char *entry=KEEP(xidePathResolve(root,"中文 源码😀.xl")),*native=KEEP(xidePathResolve(root,"helper.c")),*moved=KEEP(xidePathResolve(root,"改名.xl")),*settings=KEEP(xidePathResolve(root,"settings.json"));
 char *light=KEEP(xidePathResolve(root,"native-language-light.png")),*dark=KEEP(xidePathResolve(root,"native-language-dark.png")),*large=KEEP(xidePathResolve(root,"native-language-large.png")),*real=KEEP(xidePathResolve(root,"native-language-real.png"));
 wchar_t module[32768]; char binary[131072]; CHECK(GetModuleFileNameW(NULL,module,32768)); wchar_t* slash=wcsrchr(module,L'\\'); CHECK(slash); wcscpy(slash+1,L"xide_language_program.exe"); CHECK(WideCharToMultiByte(CP_UTF8,0,module,-1,binary,sizeof(binary),NULL,NULL));
 const char* saved="int 中文 = missingName\n"; const char* nativeText="int helper(void) { return 42; }\n";
 CHECK(root && tools && compiler && entry && mode && xrtDirCreateAll(tools) && xrtFileCopy(binary,compiler,true) && writeText(mode,"bad") && writeText(entry,saved) && writeText(native,nativeText));
 app.hidden=1; app.noSession=1; app.highDpi=1; app.dpiOverride=1.25f; app.compilerPath=compiler; app.settingsPath=settings; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideActionCount()==71 && xideActionNamed("language.checkSaved")->id==66 && xideActionNamed("language.cancelCheck")->id==67 && !xideActionQuery(&app,XIDE_ACTION_CHECK_SAVED).enabled);
 CHECK(xideEditorOpen(&app,entry,&error) && xideEditorOpen(&app,native,&error) && settle(&app)); xide_document *xl=xideDocumentsFind(&app.documents,entry),*c=app.activeDocument; CHECK(xl && c && token(xl->editor,"int",XUI_CODE_TOKEN_TYPE) && token(c->editor,"return",XUI_CODE_TOKEN_KEYWORD) && !xideActionQuery(&app,XIDE_ACTION_CHECK_SAVED).enabled);
 xui_code_document core=xuiCodeEditGetDocument(xl->editor); uint32_t cv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)),xv=xuiCodeDocumentGetVersion(core); int windows=xuiDockPanelGetWindowCount(app.dock);
 CHECK(xideEditorShow(&app,xl)==XUI_OK && settle(&app) && menuCheck(&app) && waitCheck(&app) && xideLanguageViewDiagnosticCount(&app)==1 && centerBottom(&app,xl));
 xui_code_diagnostic_t diagnostic={.iSize=sizeof(diagnostic)}; CHECK(xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(xl->editor))==1 && xuiCodeAnnotationGetDiagnostic(xuiCodeEditGetAnnotations(xl->editor),0,&diagnostic)==XUI_OK && diagnostic.tRange.iStart==13 && diagnostic.tRange.iEnd==14 && strstr(diagnostic.sMessage,"中文😀") && capture(&app,light));
 CHECK(xuiCodeDocumentGetVersion(core)==xv && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor))==cv && !xl->dirty && !c->dirty && xuiDockPanelGetWindowCount(app.dock)==windows);
 CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(xl->editor),core,xuiCodeDocumentGetLength(core),0)==XUI_OK);
 CHECK(xuiTreeViewSetSelected(app.panels[XIDE_PROBLEMS],1)==XUI_OK);
 CHECK(xuiSetFocusWidget(app.context,app.panels[XIDE_PROBLEMS])==XUI_OK && settle(&app) && xuiGetFocusWidget(app.context)==app.panels[XIDE_PROBLEMS]);
 CHECK(xuiInputKeyDown(app.context,XUI_KEY_ENTER,0)==XUI_OK && xuiInputKeyUp(app.context,XUI_KEY_ENTER,0)==XUI_OK && settle(&app));
 int selectionStart=0,selectionEnd=0; CHECK(xuiCodeSelectionGetRange(xuiCodeEditGetSelection(xl->editor),&selectionStart,&selectionEnd)==XUI_OK && selectionStart==13 && selectionEnd==13);
 CHECK(app.activeDocument==xl && xuiGetFocusWidget(app.context)==xl->editor && xideTaskViewJump(&app,0));
 CHECK(xideWorkbenchApplyTheme(&app,1)==XUI_OK && capture(&app,dark));
 CHECK(xideUiFontApply(&app,app.uiFontPath,24,&error) && resize(&app,900,600) && capture(&app,large) && xideUiFontApply(&app,app.uiFontPath,16,&error) && resize(&app,1280,800));
 CHECK(xideDocumentSetLanguage(xl,"c",&error) && settle(&app) && !xideLanguageViewDiagnosticCount(&app) && !xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(xl->editor)) && !xideActionQuery(&app,XIDE_ACTION_CHECK_SAVED).enabled && xuiCodeDocumentGetVersion(core)==xv);
 CHECK(xideDocumentSetLanguage(xl,"xlang",&error) && settle(&app) && xideActionExecute(&app,XIDE_ACTION_CHECK_SAVED)==XIDE_ACTION_DISPATCHED && waitCheck(&app) && xideLanguageViewDiagnosticCount(&app)==1);
 int length=xuiCodeDocumentGetLength(core); CHECK(xuiCodeDocumentInsert(core,length,"//未保存😀\n")==XUI_OK && settle(&app) && xl->dirty && !xideLanguageViewDiagnosticCount(&app) && !xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(xl->editor)) && !xideActionQuery(&app,XIDE_ACTION_CHECK_SAVED).enabled);
 CHECK(writeText(mode,"good") && xideDocumentSave(xl,NULL,0,&error) && settle(&app) && waitCheck(&app) && strstr(xideLanguageViewMessage(&app),"0 个错误") && xuiCodeDocumentCanUndo(core));
 CHECK(writeText(mode,"slow-bad") && xideActionExecute(&app,XIDE_ACTION_CHECK_SAVED)==XIDE_ACTION_DISPATCHED && settle(&app) && xideLanguageViewBusy(&app));
 length=xuiCodeDocumentGetLength(core); CHECK(xuiCodeDocumentInsert(core,length,"//during check\n")==XUI_OK && settle(&app) && waitCheck(&app) && xl->dirty && !xideLanguageViewDiagnosticCount(&app) && !xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(xl->editor)));
 CHECK(writeText(mode,"bad") && xideDocumentSave(xl,NULL,0,&error) && settle(&app) && waitCheck(&app) && xideLanguageViewDiagnosticCount(&app)==1);
 CHECK(writeText(mode,"slow") && xideActionExecute(&app,XIDE_ACTION_CHECK_SAVED)==XIDE_ACTION_DISPATCHED && settle(&app) && xideLanguageViewBusy(&app)); uint64_t cancelled=GetTickCount64(); CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && waitCheck(&app) && GetTickCount64()-cancelled<2000 && !xideLanguageViewDiagnosticCount(&app));
 CHECK(writeText(mode,"bad") && xideActionExecute(&app,XIDE_ACTION_CHECK_SAVED)==XIDE_ACTION_DISPATCHED && waitCheck(&app) && xideLanguageViewDiagnosticCount(&app)==1);
 CHECK(xideDocumentRename(xl,moved,&error) && settle(&app) && !xideLanguageViewDiagnosticCount(&app) && xuiCodeDocumentCanUndo(core) && waitCheck(&app) && xideLanguageViewDiagnosticCount(&app)==1);
 CHECK(xideEditorClose(&app,xl,0,&error) && settle(&app) && !xl->open && !xideLanguageViewDiagnosticCount(&app)); xide_document* fresh=xideDocumentNew(&app.documents,"c",&error); CHECK(fresh==xl && !xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(fresh->editor)) && xideEditorClose(&app,fresh,0,&error));
 char* newEntry=KEEP(xidePathResolve(root,"new-document.xl")); fresh=xideDocumentNew(&app.documents,"xlang",&error);
 CHECK(fresh==xl && xideEditorShow(&app,fresh)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(fresh->editor),0,"int fresh = 0\n")==XUI_OK && xideDocumentSave(fresh,newEntry,0,&error) && settle(&app) && !xideLanguageViewBusy(&app) && !xideLanguageViewDiagnosticCount(&app) && xideEditorClose(&app,fresh,0,&error) && settle(&app));
 CHECK(xideEditorOpen(&app,moved,&error) && settle(&app)); xl=app.activeDocument; CHECK(xideActionExecute(&app,XIDE_ACTION_CHECK_SAVED)==XIDE_ACTION_DISPATCHED && waitCheck(&app) && xideLanguageViewDiagnosticCount(&app)==1);
 CHECK(killOwnedServer()); uint64_t until=GetTickCount64()+5000; do { CHECK(settle(&app)); if(!xideLanguageViewDiagnosticCount(&app)) break; Sleep(5); } while(GetTickCount64()<until);
 CHECK(!xideLanguageViewDiagnosticCount(&app) && strstr(xideLanguageViewMessage(&app),"已结束") && xideActionQuery(&app,XIDE_ACTION_CHECK_SAVED).enabled && xideActionExecute(&app,XIDE_ACTION_CHECK_SAVED)==XIDE_ACTION_DISPATCHED && waitCheck(&app) && xideLanguageViewDiagnosticCount(&app)==1);
 if(f->compiler) { xideLanguageViewStop(&app); app.compilerPath=f->compiler; /* Recreate compiler ownership through a fresh native app, no settings mutation. */
  if(began) { CHECK(xgeEnd()==XGE_OK); began=0; } xideAppDestroy(&app); memset(&app,0,sizeof(app)); app.hidden=1; app.noSession=1; app.highDpi=1; app.compilerPath=f->compiler; app.settingsPath=settings; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
  CHECK(xideEditorOpen(&app,moved,&error) && xideEditorOpen(&app,native,&error) && settle(&app)); xl=xideDocumentsFind(&app.documents,moved); CHECK(xl && xideEditorShow(&app,xl)==XUI_OK && xideActionExecute(&app,XIDE_ACTION_CHECK_SAVED)==XIDE_ACTION_DISPATCHED && waitCheck(&app) && xideLanguageViewDiagnosticCount(&app)==1 && centerBottom(&app,xl) && capture(&app,real));
 }
 printf("language_view_test passed: native menu/actions, real CodeEdit diagnostics and central problem jump, C/xlang syntax, stale edit/save/cancel/language/rename/close guards, owned server crash/restart/full resync and native renders in %s.\n",root);
cleanup:
 if(began) { xgeEnd(); } xideAppDestroy(&app); for(size_t i=0;i<ownedCount;++i) free(owned[i]); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang language service verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc<2 || argc>3 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argc==3 ? argv[2] : NULL}; wchar_t name[100]; swprintf(name,100,L"XlangLanguageVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(t) { if(WaitForSingleObject(t,90000)!=WAIT_OBJECT_0) { fputs("language_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(t,&code); CloseHandle(t); } CloseDesktop(f.desktop); return (int)code; }
