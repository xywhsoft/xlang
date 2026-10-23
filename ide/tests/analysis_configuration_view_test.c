#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/language_view.h"
#include "ui/workspace_view.h"
#include "services/analysis_configuration.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ui/completion_view.c"
typedef struct fixture {const char *directory,*compiler;HDESK desktop;int started,failed,checks;} fixture;
#define CHECK(e) do {if(!(e)) {fprintf(stderr,"analysis_configuration_view_test:%d %s; %s; %s\n",__LINE__,#e,error.message,xideLanguageViewMessage(&app));f->failed=1;goto cleanup;}++f->checks;}while(0)
static int writeText(const char* path,const char* text) {return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int pump(xide_app* app)
{if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0;xideEditorPoll(app);xideTaskViewPoll(app);xideLanguageViewPoll(app);xideCompletionViewPoll(app);xideWorkbenchLayout(app);return xuiLayout(app->context)==XUI_OK && !app->frameError;}
static int key(xide_app* app,int code,uint32_t modifiers)
{return xuiInputKeyDownEx(app->context,code,modifiers,NULL)==XUI_OK && xuiInputKeyUpEx(app->context,code,modifiers,NULL)==XUI_OK && pump(app);}
static int replace(xide_document* doc,const char* text)
{return xuiCodeDocumentReplace(xuiCodeEditGetDocument(doc->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(doc->editor)),text)==XUI_OK;}
static int query(xide_app* app,xide_document* doc,const char* text,const char* marker)
{
 cvdismiss(app->completionView);const char* at=strstr(text,marker);if(!at || !replace(doc,text) || xideEditorShow(app,doc)!=XUI_OK ||
  xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(doc->editor),xuiCodeEditGetDocument(doc->editor),(int)(at-text+strlen(marker)),0)!=XUI_OK || xuiSetFocusWidget(app->context,doc->editor)!=XUI_OK || !key(app,XUI_KEY_SPACE,XUI_MOD_CTRL)) return 0;
 uint64_t until=GetTickCount64()+20000;do {if(!pump(app)) return 0;if(app->completionView->ready) return 1;Sleep(5);}while(GetTickCount64()<until);return 0;
}
static int has(xide_completion_view* v,const char* name)
{for(int i=0;i<v->count;++i) if(!strcmp(v->items[i].sLabel,name)) return 1;return 0;}
static int textIs(const xvalue* value,const char* expected)
{xstrview s={0};return xrtValueGetString(value,&s) && s.Size==strlen(expected) && !memcmp(s.Data,expected,s.Size);}
static int manual(xide_app* app,int errors)
{
 uint64_t until=GetTickCount64()+20000;
 while(!xideLanguageViewCanCheckCurrent(app)) {if(!pump(app) || GetTickCount64()>=until) return 0;Sleep(5);}
 if(!xideLanguageViewAction(app,XIDE_ACTION_CHECK_CURRENT)) return 0;
 do {if(!pump(app)) return 0;if(!xideLanguageViewBusy(app) && (errors ? xideLanguageViewDiagnosticCount(app)>0 : xideLanguageViewDiagnosticCount(app)==0)) return 1;Sleep(5);}while(GetTickCount64()<until);return 0;
}
static xvalue* model(const char* entry,const char* helper)
{
 const char* json="{\"format\":\"xlang-project-1\",\"entry\":\"placeholder.xl\",\"ide\":{\"name\":\"分析配置\",\"files\":[{\"id\":\"helper\",\"path\":\"helper.xl\",\"kind\":\"xlang\"}],\"activeConfiguration\":\"debug\",\"configurations\":[{\"id\":\"debug\",\"output\":\"build/app.exe\",\"run\":{\"environment\":{\"XLANG_PACKAGE_PATH\":\"run-only-wrong\"}}},{\"id\":\"release\",\"output\":\"build/release.exe\"}]}}";
 xvalue* root=xrtJsonParse((xstrview){json,strlen(json)});if(!root) return NULL;
 int okay=cvput(root,"entry",xrtValueString(cvsv(entry))) && cvput(xrtValueArrayGet(cvget(cvget(root,"ide"),"files"),0),"path",xrtValueString(cvsv(helper)));
 if(!okay) {xrtValueRelease(root);root=NULL;}return root;
}
static int verify(void* user)
{
 fixture* f=user;if(f->started) return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};int began=0,touched=0;
 char *root=NULL,*entry=NULL,*helper=NULL,*image=NULL;wchar_t previous[32768];SetLastError(ERROR_SUCCESS);DWORD old=GetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",previous,32768);int present=old || GetLastError()==ERROR_SUCCESS;
 char unique[80];snprintf(unique,sizeof(unique),"configured-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());root=xidePathResolve(f->directory,unique);entry=xidePathResolve(root,"main.xl");helper=xidePathResolve(root,"helper.xl");image=xidePathResolve(root,"configured-completion.png");
 const char* saved="import context_package as cp\nexport func value() as int\n return cp.value()\nend\n";
 const char* cursor="import context_package as cp\nexport func value() as int\n cp.\n return 0\nend\n";
 CHECK(root && entry && helper && image && xrtDirCreateAll(root));CHECK(writeText(entry,"\xef\xbb\xbf" "import helper\r\nfunc main(...) as int\r\n return helper.value()\r\nend\r\n") && writeText(helper,saved));
 CHECK(old<32768 && SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",L"packages"));touched=1;
 app.hidden=1;app.noSession=1;app.compilerPath=f->compiler;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;
 CHECK(xideWorkspaceCreate(&app.workspaceModel,"配置验证",&error));app.workspaceModel.projects=calloc(2,sizeof(xide_project));CHECK(app.workspaceModel.projects);app.workspaceModel.projectCount=app.workspaceModel.projectCapacity=2;
 for(size_t i=0;i<2;++i) {
  char* directory=xidePathResolve(root,i ? "B" : "A");char* package=directory ? xidePathResolve(directory,"packages/context_package") : NULL;
  char* manifest=directory ? xidePathResolve(directory,"project.xlp") : NULL;char* pm=package ? xidePathResolve(package,"xlang.package.json") : NULL;char* api=package ? xidePathResolve(package,"api.xl") : NULL;
  CHECK(directory && package && manifest && pm && api && xrtDirCreateAll(package));
  CHECK(writeText(pm,"{\"format\":\"xlang-package-1\",\"name\":\"context_package\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"api.xl\"]},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}"));
  CHECK(writeText(api,i ? "export func value() as string\n return \"B\"\nend\nexport func beta() as int\n return 2\nend\n" : "export func value() as int\n return 7\nend\nexport func alpha() as int\n return 1\nend\n"));
  xvalue* json=model(entry,helper);CHECK(json && xideProjectDecode(app.workspaceModel.projects+i,manifest,json,&error));xrtValueRelease(json);
  app.workspaceModel.projects[i].id=cvcopy(i ? "B" : "A");CHECK(app.workspaceModel.projects[i].id);free(directory);free(package);free(manifest);free(pm);free(api);
 }
 CHECK(xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,"A") && pump(&app));
 CHECK(xideEditorOpen(&app,helper,&error) && app.documents.count==1 && pump(&app));xide_document* doc=app.activeDocument;xide_completion_view* v=app.completionView;
 CHECK(query(&app,doc,cursor,"cp.") && v->nativeBackend && has(v,"alpha") && !has(v,"beta"));
 CHECK(textIs(cvget(cvget(v->result,"analysisContext"),"project"),app.workspaceModel.projects[0].manifest) && textIs(cvget(cvget(v->result,"analysisContext"),"configuration"),"debug"));
 CHECK(query(&app,doc,"import context_package as cp\nexport func value() as int\n cp.al\n return 0\nend\n","cp.al") && v->count==1 && has(v,"alpha"));
 CHECK(key(&app,XUI_KEY_ENTER,0) && strstr(xuiCodeEditGetText(doc->editor)," cp.alpha\n") && key(&app,'Z',XUI_MOD_CTRL) && strstr(xuiCodeEditGetText(doc->editor)," cp.al\n"));
 uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor));uint64_t session=v->session;
 CHECK(xideWorkspaceViewSelectProject(&app,"B") && !v->result && v->session>session && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor))==version && !*app.workspaceModel.startupProject);
 CHECK(query(&app,doc,cursor,"cp.") && has(v,"beta") && !has(v,"alpha") && textIs(cvget(cvget(v->result,"analysisContext"),"project"),app.workspaceModel.projects[1].manifest));
 CHECK(xuiUpdate(app.context,0)==XUI_OK && xuiLayout(app.context)==XUI_OK);xui_rect_i_t rect={0,0,app.viewportWidth,app.viewportHeight};CHECK(app.proxy.surfaceClear(&app.proxy,app.target,0xffeeeeeeu)==XUI_OK && xuiRender(app.context,app.target,&rect,1)==XUI_OK && xideAppCapture(&app,image)==XGE_OK);
 version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor));CHECK(xideWorkspaceViewSelectConfiguration(&app,"release",&error) && !v->result && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor))==version);
 CHECK(query(&app,doc,cursor,"cp.") && has(v,"beta") && textIs(cvget(cvget(v->result,"analysisContext"),"configuration"),"release"));
 cvdismiss(v);CHECK(replace(doc,saved) && pump(&app) && manual(&app,1));CHECK(xideWorkspaceViewSelectProject(&app,"A") && !xideLanguageViewDiagnosticCount(&app) && manual(&app,0));
 xide_text_file original={0};CHECK(xideTextFileRead(helper,&original,&error) && !strcmp(original.text,saved));xideTextFileClear(&original);
 printf("analysis_configuration_view_test passed: %d checks; actual model capture,unopened project entry,selected project/configuration,real namespace/type diagnostics,immediate withdrawal,Enter/Undo and preserved source;render=%s\n",f->checks,image);
cleanup:
 if(began) xgeEnd();
 xideAppDestroy(&app);if(touched) SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",present ? previous : NULL);free(root);free(entry);free(helper);free(image);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* value)
{fixture* f=value;if(!SetThreadDesktop(f->desktop)) return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="xlang analysis configuration";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;int result=xgeInit(&d);if(result==XGE_OK) result=xgeRun(verify,f);xgeUnit();return result==XGE_OK && f->started && !f->failed ? 0 : 1;}
int main(int argc,char** argv)
{if(argc!=3 || !xrtDirCreateAll(argv[1])) return 2;fixture f={.directory=argv[1],.compiler=argv[2]};wchar_t name[100];swprintf(name,100,L"XlangConfiguredVerification-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop) return 1;HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(t) {if(WaitForSingleObject(t,120000)!=WAIT_OBJECT_0) ExitProcess(1);GetExitCodeThread(t,&code);CloseHandle(t);}CloseDesktop(f.desktop);return (int)code;}
