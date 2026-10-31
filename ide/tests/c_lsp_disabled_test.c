#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/language_view.h"
#include "ui/task_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if XIDE_ENABLE_C_LSP
#error "This fixture validates the default build without C LSP"
#endif
typedef struct fixture {const char *directory,*compiler;HDESK desktop;int started,failed,checks;} fixture;
#define CHECK(e) do {++f->checks;if(!(e)){fprintf(stderr,"c_lsp_disabled_test:%d:%s;%s\n",__LINE__,#e,error.message);f->failed=1;goto done;}}while(0)
static int frame(xide_app* app) {return xideAppFrame(app)==XGE_OK && !app->frameError;}
static int run(void* user)
{
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;
 xide_app app={0};xide_model_error error={0};char *entry=NULL,*settings=NULL;
 CHECK(xrtDirCreateAll(f->directory));entry=xidePathResolve(f->directory,"without-c-lsp.c");settings=xidePathResolve(f->directory,"preferences.json");CHECK(entry && settings);
 const char* source="int main(void) { return 0; }\n";
 const char* preferences="{\"format\":\"xlang-ide-settings-1\",\"future\":17,\"toolchain\":{\"cServer\":\"missing-clangd.exe\",\"futureTool\":true}}";
 CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)source,strlen(source)}) && xrtFileWriteAtomic(settings,(xbytesview){(const unsigned char*)preferences,strlen(preferences)}));
 app.hidden=1;app.noSession=1;app.maxFrames=10000;app.compilerPath=f->compiler;app.settingsPath=settings;
 CHECK(frame(&app) && !app.cLanguageView && !app.cCompletionView && !app.preferences.cServer);
 CHECK(!xideActionFind(XIDE_ACTION_C_SERVER));
 CHECK(xideSettingsSetCompiler(&app.preferences,f->compiler,&error));
 xvalue* tools=xrtValueObjectGet(app.preferences.source,xrtStrView("toolchain"));
 CHECK(tools && !xrtValueObjectGet(tools,xrtStrView("cServer")) && xrtValueObjectGet(tools,xrtStrView("futureTool")) && xrtValueObjectGet(app.preferences.source,xrtStrView("future")));
 CHECK(xideEditorOpen(&app,entry,&error) && frame(&app));xide_document* d=app.activeDocument;
 CHECK(d && !strcmp(d->language,"c") && !xuiCodeEditIsReadonly(d->editor));
 CHECK(!xideLanguageViewCanCheckCurrent(&app) && !xideLanguageViewCanDefine(&app) && !xideLanguageViewCanHover(&app));
 xui_code_document document=xuiCodeEditGetDocument(d->editor);uint32_t before=xuiCodeDocumentGetVersion(document);
 CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),document,0,0)==XUI_OK && xuiSetFocusWidget(app.context,d->editor)==XUI_OK);
 CHECK(xuiInputTextEx(app.context,'z',NULL)==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK && frame(&app));
 CHECK(xuiCodeDocumentGetVersion(document)>before && xuiCodeEditGetText(d->editor)[0]=='z' && !app.cLanguageView && !app.cCompletionView);
 CHECK(xuiInputKeyDownEx(app.context,'Z',XUI_MOD_CTRL,NULL)==XUI_OK && xuiInputKeyUpEx(app.context,'Z',XUI_MOD_CTRL,NULL)==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK && frame(&app));
 CHECK(!strcmp(xuiCodeEditGetText(d->editor),source));
 CHECK(xideActionExecute(&app,XIDE_ACTION_RUN_FILE)==XIDE_ACTION_DISPATCHED);xide_task* task=xideTaskViewSelected(&app);CHECK(task);
 uint64_t until=GetTickCount64()+30000;xide_task_info info={0};
 do {CHECK(frame(&app));xideTaskInfo(task,&info);if(info.state>=XIDE_TASK_SUCCEEDED)break;Sleep(5);}while(GetTickCount64()<until);
 CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && info.exit.Code==0);
 CHECK(!app.cLanguageView && !app.cCompletionView && xidePathExists(xideTaskOutput(task)));
 printf("c_lsp_disabled_test passed:%d checks;no C service/actions,legacy preference migration,C text/undo and actual xlang-driven single-C-file build/run\n",f->checks);
done:xideAppDestroy(&app);xgeSetQuitRequestCallback(NULL,NULL);free(entry);free(settings);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
 fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;
 xge_desc_t desc={0};desc.iWidth=1400;desc.iHeight=900;desc.sTitle="IDE without C LSP";desc.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;desc.iRunMode=XGE_RUN_GAME_LOOP;
 int result=xgeInit(&desc);if(result==XGE_OK)result=xgeRun(run,f);xgeUnit();return result==XGE_OK && f->started && !f->failed?0:1;
}
int main(int argc,char** argv)
{
 if(argc!=3)return 2;
 fixture f={.directory=argv[1],.compiler=argv[2]};wchar_t name[100];swprintf(name,100,L"XlangNoCLsp-%lu",GetCurrentProcessId());
 f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;
 HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;
 if(thread){if(WaitForSingleObject(thread,60000)!=WAIT_OBJECT_0)ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f.desktop);return (int)code;
}
