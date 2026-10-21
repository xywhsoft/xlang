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
/* Exercise the actual production adapter and its native widgets. */
#include "../src/ui/c_language_view.c"
typedef struct fixture { const char *directory,*server; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(v) do { ++f->checks; if(!(v)) { fprintf(stderr,"c_input_trace_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideCLanguageViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int settle(xide_app* app) { if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0; xideEditorPoll(app); xideTaskViewPoll(app); xideLanguageViewPoll(app); xideCLanguageViewPoll(app); xideWorkbenchLayout(app); return xuiLayout(app->context)==XUI_OK && !app->frameError; }
static int ready(xide_app* app) { uint64_t until=GetTickCount64()+30000; do { if(!settle(app)) return 0; if(app->cLanguageView->ready && app->cLanguageView->symbolResult && app->cLanguageView->received==(int)app->cLanguageView->inputs->count) return 1; if(app->cLanguageView->paused) return 0; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int check(xide_app* app) { return xideActionExecute(app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && ready(app); }
static int position(xide_app* app,int offset) { xide_document* document=app->activeDocument; return xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(document->editor),xuiCodeEditGetDocument(document->editor),offset,0)==XUI_OK && xuiSetFocusWidget(app->context,document->editor)==XUI_OK && settle(app); }
static int key(xide_app* app,int value,uint32_t modifiers) { return xuiInputKeyDownEx(app->context,value,modifiers,NULL)==XUI_OK && xuiInputKeyUpEx(app->context,value,modifiers,NULL)==XUI_OK && settle(app); }
static int hover(xide_app* app,const char* type) { uint64_t until=GetTickCount64()+30000; do { if(!settle(app)) return 0; xide_c_language_view* view=app->cLanguageView; if(view->hoverResult) { const char* content=cvstr(cvget(cvget(view->hoverResult,"contents"),"value")); return content && strstr(content,type) && xideCLanguageViewHintOpen(app); } if(!view->hover.token && !view->hoverDue) return 0; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int capture(xide_app* app,const char* path) { if(!settle(app) || xuiUpdate(app->context,0)!=XUI_OK || xuiLayout(app->context)!=XUI_OK) return 0; xui_rect_i_t r={0,0,app->viewportWidth,app->viewportHeight}; return app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&r,1)==XUI_OK && xideAppCapture(app,path)==XGE_OK; }
static int verify(void* user) {
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; xide_project project={0}; int began=0;
 char *root=NULL,*directory=NULL,*evidence=NULL,*entry=NULL,*outer=NULL,*leaf=NULL,*xl=NULL,*manifest=NULL,*settings=NULL,*before=NULL,*after=NULL,*navigation=NULL;
 char unique[90]; snprintf(unique,sizeof(unique),"native-trace-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(f->directory,unique); directory=xidePathResolve(root,"project 空间😀"); evidence=xidePathResolve(root,"evidence");
 entry=xidePathResolve(directory,"entry.c"); outer=xidePathResolve(directory,"outer.h"); leaf=xidePathResolve(directory,"隐式 空间😀.h"); xl=xidePathResolve(directory,"main.xl"); manifest=xidePathResolve(directory,"project.xlp"); settings=xidePathResolve(root,"settings.json"); before=xidePathResolve(evidence,"implicit-double.png"); after=xidePathResolve(evidence,"implicit-int.png"); navigation=xidePathResolve(evidence,"implicit-f12.png");
 const char* source="#include \"outer.h\"\nint main(void) { return traced_value; }\n"; const char* initial="static double traced_value = 1.0;\n"; const char* changed="static int traced_value = 1;\n";
 CHECK(root && directory && evidence && entry && outer && leaf && xl && manifest && settings && before && after && navigation && xrtDirCreateAll(directory) && xrtDirCreateAll(evidence));
 CHECK(writeText(entry,source) && writeText(outer,"#include \"隐式 空间😀.h\"\n") && writeText(leaf,initial) && writeText(xl,"func main(...) as int\n return 0\nend\n"));
 CHECK(xideProjectCreate(&project,manifest,"隐式 C 输入图",xl,&error) && xideProjectAddFile(&project,entry,"c",&error) && xideProjectSave(&project,manifest,&error));
 app.hidden=1; app.noSession=1; app.cServerPath=f->server; app.settingsPath=settings; app.projectPath=manifest; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideEditorOpen(&app,entry,&error)); xide_document* c=app.activeDocument; int offset=(int)(strstr(source,"traced_value")-source)+2; CHECK(position(&app,offset) && check(&app));
 xide_c_language_view* view=app.cLanguageView; xide_file_snapshot guard={0}; CHECK(view->inputs->count==1 && view->received==1 && xideCAnalysisJobGuard(view->job,leaf,&guard) && xideCAnalysisJobGuard(view->job,outer,&guard));
 CHECK(position(&app,offset) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"double")); CHECK(capture(&app,before));
 CHECK(writeText(leaf,changed)); uint64_t until=GetTickCount64()+6000; while(xideCCompletionJobState(view->job,NULL,0)!=XIDE_C_JOB_INVALIDATED && GetTickCount64()<until) Sleep(5);
 CHECK(xideCCompletionJobState(view->job,NULL,0)==XIDE_C_JOB_INVALIDATED); CHECK(!xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled);
 CHECK(settle(&app) && !view->ready && !view->symbolResult && !view->hoverResult && !view->diagnosticCount && !xideCLanguageViewHintOpen(&app));
 CHECK(check(&app) && position(&app,offset) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"int")); CHECK(xideCAnalysisJobGuard(view->job,leaf,&guard) && capture(&app,after));
 CHECK(key(&app,XUI_KEY_F12,0)); until=GetTickCount64()+30000; int settled=1; while(settled && view->definition.token && GetTickCount64()<until) { settled=settle(&app); Sleep(5); } CHECK(settled && !view->definition.token);
 CHECK(app.activeDocument!=c && xidePathLexicalEqual(app.activeDocument->path,leaf)); xui_code_selection_t selection={0}; CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(app.activeDocument->editor),&selection)==XUI_OK && selection.iAnchorOffset==(int)(strstr(changed,"traced_value")-changed) && selection.iCaretOffset==selection.iAnchorOffset+(int)strlen("traced_value"));
 CHECK(!strcmp(xuiCodeEditGetText(app.activeDocument->editor),changed) && !app.activeDocument->dirty && xideWorkbenchApplyTheme(&app,1)==XUI_OK && capture(&app,navigation));
 size_t count=0; unsigned char* bytes=xrtFileReadAllLimit(entry,1024,&count); int preserved=bytes && count==strlen(source) && !memcmp(bytes,source,count); xrtFree(bytes); CHECK(preserved && !c->dirty);
 printf("c_input_trace_view_test passed: %d native assertions; actual implicit-header hover, invalidation, reanalysis and guarded F12; renders in %s.\n",f->checks,root);
cleanup:
 if(began) { xgeEnd(); }
 xideAppDestroy(&app); xideProjectClear(&project); free(root); free(directory); free(evidence); free(entry); free(outer); free(leaf); free(xl); free(manifest); free(settings); free(before); free(after); free(navigation); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="C physical input trace"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=3 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.server=argv[2]}; wchar_t name[100]; swprintf(name,100,L"CInputTrace-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,120000)!=WAIT_OBJECT_0) { fputs("Native C input trace timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code; }
