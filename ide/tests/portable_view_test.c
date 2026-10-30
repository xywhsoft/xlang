#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/language_view.h"
#include "ui/task_view.h"
#include "ui/catalog_view.h"
#include "ui/c_completion_view.h"
#include "services/ui_font.h"
#include "services/editor_font.h"
#include "platform/path.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Inspect the real bridge view; the child has only its package-local defaults. */
#include "../src/ui/c_language_view.c"
typedef struct fixture { const char* directory; int moved; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(t) do { if(!(t)) { fprintf(stderr,"portable_view_test:%d: %s; %s; C %s; xlang %s\n",__LINE__,#t,error.message,xideCLanguageViewMessage(&app),xideLanguageViewMessage(&app)); f->failed=1; goto cleanup; } ++f->checks; } while(0)
static int writeText(const char* p,const char* text) { return xrtFileWriteAtomic(p,(xbytesview){(unsigned char*)text,strlen(text)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideTaskViewPoll(a); xideLanguageViewPoll(a); xideCLanguageViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int token(xui_widget e,const char* word,int kind) { const char* text=xuiCodeEditGetText(e),*p=strstr(text,word); xui_code_token_t ts[512]; int count=0; if(!p || xuiCodeTokenBufferGetTokens(xuiCodeEditGetTokenBuffer(e),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(e)),ts,512,&count)!=XUI_OK || count>512) return 0; for(int i=0;i<count;++i) if(ts[i].iStartOffset==(int)(p-text) && ts[i].iKind==kind) return 1; return 0; }
static int waitC(xide_app* a) { uint64_t until=GetTickCount64()+60000; do { if(!settle(a)) return 0; xide_c_language_view* v=a->cLanguageView; if(v->ready && v->received==(int)v->inputs->count && v->symbolResult) return 1; if(v->paused) return 0; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int waitXlang(xide_app* a) { uint64_t until=GetTickCount64()+60000; do { if(!settle(a)) return 0; if(!xideLanguageViewBusy(a) && strstr(xideLanguageViewMessage(a),"快照检查：")) return xideLanguageViewDiagnosticCount(a)==0; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int capture(xide_app* a,const char* p) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK || xuiLayout(a->context)!=XUI_OK) return 0; xui_rect_i_t rect={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&rect,1)==XUI_OK && xideAppCapture(a,p)==XGE_OK; }
static int layout(xide_app* a,xide_document* d) { xui_dock_window_info_t wi[5]={{0}}; xui_dock_pane_info_t pi[5]={{0}}; int ids[]={d->window,a->windows[XIDE_HELP],a->windows[XIDE_COMMANDS],a->windows[XIDE_WORKSPACE],a->windows[XIDE_PROPERTIES]}; for(int i=0;i<5;++i) { wi[i].iSize=sizeof(wi[i]); pi[i].iSize=sizeof(pi[i]); if(xuiDockPanelGetWindowInfo(a->dock,ids[i],&wi[i])!=XUI_OK || xuiDockPanelGetPaneInfo(a->dock,wi[i].iPane,&pi[i])!=XUI_OK) return 0; } return pi[0].tRect.fX==pi[1].tRect.fX && pi[0].tRect.fW==pi[1].tRect.fW && pi[1].tRect.fY>=pi[0].tRect.fY+pi[0].tRect.fH && pi[2].tRect.fX<pi[0].tRect.fX && pi[3].tRect.fX>pi[0].tRect.fX && pi[3].tRect.fX==pi[4].tRect.fX && pi[3].tRect.fW==pi[4].tRect.fW && pi[4].tRect.fY>=pi[3].tRect.fY+pi[3].tRect.fH && wi[1].iRegion==XUI_DOCK_PANEL_REGION_DOCUMENT && xuiWidgetGetLayoutType(a->helpContent)==XUI_LAYOUT_COLUMN; }
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1;
 xide_app app={0}; xide_model_error error={0}; xide_project project={0}; xide_workspace workspace={0}; int began=0; char* owned[24]={0}; size_t count=0; unsigned char* runBytes=NULL; size_t runLength=0; xvalue *result=NULL,*testConfigs=NULL;
 #define KEEP(p) (owned[count++]=(p))
 char *root=KEEP(xidePathResolve(f->directory,f->moved ? "迁移后 工程😀" : "初次 工程😀")),*entry=KEEP(xidePathResolve(root,"入口😀.xl")),*native=KEEP(xidePathResolve(root,"native.c")),*manifest=KEEP(xidePathResolve(root,"工程.xlp")),*scene=KEEP(xidePathResolve(root,"工作空间.xlw"));
 char *defaultSettings=KEEP(xideSettingsDefaultPath()),*options=KEEP(defaultSettings ? xidePathDirectory(defaultSettings) : NULL),*program=KEEP(options ? xidePathDirectory(options) : NULL),*compiler=KEEP(program ? xidePathResolve(program,"toolchains/xlang/xl.exe") : NULL),*server=KEEP(program ? xidePathResolve(program,"toolchains/clangd/bin/clangd.exe") : NULL),*catalog=KEEP(program ? xidePathResolve(program,"data/commands.json") : NULL);
 char *light=KEEP(xidePathResolve(root,"portable-light.png")),*dark=KEEP(xidePathResolve(root,"portable-dark.png")),*cImage=KEEP(xidePathResolve(root,"portable-c.png")),*runImage=KEEP(xidePathResolve(root,"portable-run.png")),*report=KEEP(xidePathResolve(root,"native-result.json"));
 const char* saved="export \"xl_add\" func 原稿(c_int a,c_int b) as c_int\n return a+b\nend\n";
 const char* draft="// 中文 😀 unsaved package source\nexport \"xl_add\" func 便携(c_int a,c_int b) as c_int\n return a+b\nend\n";
 const char* runText="export \"xl_add\" func 便携(c_int a,c_int b) as c_int\n return a+b\nend\nextern \"native_add\" func native_add(c_int a,c_int b) as c_int\nfunc main(...) as int\n c_int result = native_add(19,23)\n if result != 42 then\n  return 1\n end\n print(\"PORTABLE_MIXED_42\")\n return 0\nend\n";
 const char* cText="#include <stddef.h>\nint native_add(int a,int b) { return a+b; }\nint package_answer(void) { return xl_add(19,23); }\n";
 CHECK(count<=24 && root && entry && native && manifest && scene && defaultSettings && options && program && compiler && server && light && dark && cImage && runImage && report && xrtDirCreateAll(root));
 CHECK(writeText(entry,saved) && writeText(native,cText));
 CHECK(xideProjectCreate(&project,manifest,"便携混编项目",entry,&error) && xideProjectAddFile(&project,native,"c",&error) && xideProjectSave(&project,manifest,&error));
 CHECK(xideWorkspaceCreate(&workspace,"便携工作空间",&error) && xideWorkspaceAddProject(&workspace,manifest,project.id,&error) && xideWorkspaceSetStartup(&workspace,workspace.projects[0].id,&error) && xideWorkspaceSave(&workspace,scene,&error));
 /* A visible app on an isolated desktop takes the actual default preferences
    branch. --hidden deliberately skips defaults for ordinary developer probes. */
 app.noSession=1; app.workspacePath=scene;
 CHECK(!app.compilerPath && !app.cServerPath && !app.catalogPath && !app.settingsPath && !app.hidden);
 CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xidePathLexicalEqual(app.preferences.path,defaultSettings) && xidePathEqual(xideTaskViewCompiler(&app),compiler) && xidePathEqual(xideCCompletionViewServer(&app),server));
 CHECK(catalog && xidePathEqual(app.catalog.path,catalog) && app.catalog.count>4400 && app.catalog.documentCount>=6 && xideCatalogDocument(&app.catalog,"package:csv") && app.uiFontSize==(f->moved ? 18 : 16) && app.codeFontSize==(f->moved ? 18 : 16));
 CHECK(xideCatalogFind(&app.catalog,"language:func") && xideCatalogFind(&app.catalog,"c:keyword:return"));
 CHECK(xideEditorOpen(&app,entry,&error)); xide_document* xl=app.activeDocument;
 CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(xl->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(xl->editor)),draft)==XUI_OK && settle(&app) && xl->dirty && token(xl->editor,"export",XUI_CODE_TOKEN_KEYWORD));
 CHECK(xideActionExecute(&app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && waitXlang(&app));
 CHECK(xideEditorOpen(&app,native,&error)); xide_document* c=app.activeDocument;
 CHECK(settle(&app) && token(c->editor,"#include",XUI_CODE_TOKEN_PREPROCESSOR) && token(c->editor,"return",XUI_CODE_TOKEN_KEYWORD));
 CHECK(xideActionExecute(&app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && waitC(&app) && !app.cLanguageView->diagnosticCount);
 size_t helpIndex=0; while(helpIndex<app.catalog.count && !app.catalog.entries[helpIndex].documentCount) ++helpIndex;
 CHECK(helpIndex<app.catalog.count && xideCatalogViewSelect(&app,helpIndex) && xideHelpChapterCount(xideCatalogHelpDocument(&app))>0 && settle(&app) && layout(&app,c) && capture(&app,cImage));
 const char* at=strstr(xuiCodeEditGetText(c->editor),"xl_add"); CHECK(at && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(c->editor),xuiCodeEditGetDocument(c->editor),(int)(at-xuiCodeEditGetText(c->editor))+1,0)==XUI_OK && xuiSetFocusWidget(app.context,c->editor)==XUI_OK);
 CHECK(xuiInputKeyDownEx(app.context,XUI_KEY_F12,0,NULL)==XUI_OK && xuiInputKeyUpEx(app.context,XUI_KEY_F12,0,NULL)==XUI_OK);
 uint64_t until=GetTickCount64()+30000; do { CHECK(settle(&app)); if(!app.cLanguageView->definition.token) break; Sleep(5); } while(GetTickCount64()<until);
 CHECK(!app.cLanguageView->definition.token && app.activeDocument==xl && xl->dirty && !strcmp(xuiCodeEditGetText(xl->editor),draft));
 xui_code_selection_t selection={0}; const char* definition=strstr(draft,"便携"); CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(xl->editor),&selection)==XUI_OK && selection.iAnchorOffset==(int)(definition-draft) && selection.iCaretOffset==(int)(definition-draft+strlen("便携")));
 CHECK(layout(&app,xl) && capture(&app,light) && xideWorkbenchApplyTheme(&app,1)==XUI_OK && capture(&app,dark));
 CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(xl->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(xl->editor)),runText)==XUI_OK && settle(&app));
 CHECK(xideActionExecute(&app,XIDE_ACTION_RUN)==XIDE_ACTION_DISPATCHED); xide_task* task=xideTaskViewSelected(&app); xide_task_info info={0}; CHECK(task);
 until=GetTickCount64()+60000; do { CHECK(settle(&app)); xideTaskInfo(task,&info); if(info.state>=XIDE_TASK_SUCCEEDED) break; Sleep(5); } while(GetTickCount64()<until);
 CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && !xl->dirty && xidePathExists(xideTaskOutput(task)));
 runBytes=xrtFileReadAllLimit(xideTaskRunLog(task),1024*1024,&runLength); CHECK(runBytes && runLength && strstr((char*)runBytes,"PORTABLE_MIXED_42"));
 CHECK(capture(&app,runImage));
 xide_project* testedProject=xideWorkspaceFindProject(&app.workspaceModel,app.selectedProject); CHECK(testedProject);
 char* testDirectory=KEEP(xidePathDirectory(testedProject->manifest)),*testProgram=KEEP(testDirectory ? xidePathRelative(testDirectory,xideTaskOutput(task)) : NULL); CHECK(testDirectory && testProgram);
 testConfigs=xideProjectConfigurationsJson(testedProject); const char* testJson="{\"workingDirectory\":\".\",\"arguments\":[],\"environment\":{}}"; xvalue* testSpec=xrtJsonParse((xstrview){testJson,strlen(testJson)});
 CHECK(testConfigs && testSpec);
 int prepared=xrtValueObjectSetNew(testSpec,(xstrview){"program",7},xrtValueString((xstrview){testProgram,strlen(testProgram)}));
 if(prepared) prepared=xrtValueObjectSetNew(xrtValueArrayGet(testConfigs,0),(xstrview){"test",4},testSpec); else xrtValueRelease(testSpec);
 CHECK(prepared && xideProjectApplyConfigurations(testedProject,testConfigs,testedProject->activeConfiguration,&error)); xrtValueRelease(testConfigs); testConfigs=NULL;
 CHECK(xideActionQuery(&app,XIDE_ACTION_TEST).enabled && xideActionExecute(&app,XIDE_ACTION_TEST)==XIDE_ACTION_DISPATCHED); task=xideTaskViewSelected(&app); CHECK(task);
 until=GetTickCount64()+60000; do { CHECK(settle(&app)); xideTaskInfo(task,&info); if(info.state>=XIDE_TASK_SUCCEEDED) break; Sleep(5); } while(GetTickCount64()<until);
 CHECK(info.state==XIDE_TASK_SUCCEEDED && info.kind==XIDE_TASK_KIND_TEST && info.tested && !info.built && !info.ran && info.exit.Code==0 && !*xideTaskBuildLog(task));
 xrtFree(runBytes); runBytes=xrtFileReadAllLimit(xideTaskTestLog(task),1024*1024,&runLength); CHECK(runBytes && runLength && strstr((char*)runBytes,"PORTABLE_MIXED_42"));
 CHECK(xideSettingsSetCompiler(&app.preferences,compiler,&error) && xideSettingsSetCServer(&app.preferences,server,&error));
 xvalue* tools=xrtValueObjectGet(app.preferences.source,(xstrview){"toolchain",9}); xstrview compilerSetting={0},serverSetting={0};
 CHECK(tools && xrtValueGetString(xrtValueObjectGet(tools,(xstrview){"compiler",8}),&compilerSetting) && compilerSetting.Size==strlen("../toolchains/xlang/xl.exe") && !memcmp(compilerSetting.Data,"../toolchains/xlang/xl.exe",compilerSetting.Size));
 CHECK(xrtValueGetString(xrtValueObjectGet(tools,(xstrview){"cServer",7}),&serverSetting) && serverSetting.Size==strlen("../toolchains/clangd/bin/clangd.exe") && !memcmp(serverSetting.Data,"../toolchains/clangd/bin/clangd.exe",serverSetting.Size));
 CHECK(xrtValueObjectGet(app.preferences.source,(xstrview){"packageFixture",14}));
 CHECK(xideSettingsSetUiFont(&app.preferences,app.uiFontPath,18,&error) && xideSettingsSetEditorFont(&app.preferences,app.codeFontPath,18,&error));
 result=xrtValueObject(); CHECK(result);
 CHECK(xrtValueObjectSetNew(result,(xstrview){"explicitTest",12},xrtValueBool(info.tested && info.kind==XIDE_TASK_KIND_TEST && !info.built && !info.ran && info.exit.Code==0)));
 CHECK(xrtValueObjectSetNew(result,(xstrview){"compiler",8},xrtValueString((xstrview){compiler,strlen(compiler)})) && xrtValueObjectSetNew(result,(xstrview){"cServer",7},xrtValueString((xstrview){server,strlen(server)})) && xrtValueObjectSetNew(result,(xstrview){"program",7},xrtValueString((xstrview){program,strlen(program)})) && xrtValueObjectSetNew(result,(xstrview){"checks",6},xrtValueInt(f->checks)) && xrtValueObjectSetNew(result,(xstrview){"moved",5},xrtValueBool(f->moved!=0)));
 size_t length=0; char* json=xrtJsonStringify(result,true,&length); CHECK(json && writeText(report,json)); xrtFree(json);
 printf("portable_view_test passed: %d native assertions, package-local defaults, C/xlang tokens, embedded help, exact central layout, real xlang-ls/clangd unsaved F12, mixed build/run and explicit configured test, relative tool saves, font and unknown preference retention; moved=%d; evidence=%s\n",f->checks,f->moved,root);
cleanup:
 xrtFree(runBytes); xrtValueRelease(result); xrtValueRelease(testConfigs); if(began) xgeEnd(); xideAppDestroy(&app); xideWorkspaceClear(&workspace); xideProjectClear(&project); for(size_t i=0;i<count;++i) free(owned[i]); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="Portable native acceptance"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int r=xgeInit(&d); if(r==XGE_OK) r=xgeRun(verify,f); xgeUnit(); return r==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(void)
{
 int argc=0; wchar_t** wide=CommandLineToArgvW(GetCommandLineW(),&argc); if(!wide || argc!=3) { if(wide) LocalFree(wide); return 2; }
 int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[1],-1,NULL,0,NULL,NULL); char* directory=bytes ? malloc((size_t)bytes) : NULL;
 if(!directory || !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[1],-1,directory,bytes,NULL,NULL)) { free(directory); LocalFree(wide); return 2; }
 fixture f={.directory=directory,.moved=!wcscmp(wide[2],L"moved")}; LocalFree(wide);
 wchar_t name[100]; swprintf(name,100,L"PortableNative-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);
 if(!f.desktop) { free(directory); return 1; } HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1;
 if(thread) { if(WaitForSingleObject(thread,240000)!=WAIT_OBJECT_0) { fputs("portable_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); }
 CloseDesktop(f.desktop); free(directory); return (int)code;
}
