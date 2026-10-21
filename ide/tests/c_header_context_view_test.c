#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/language_view.h"
#include "ui/task_view.h"
#include "ui/workspace_view.h"
#include "ui/rename_view.h"
#include "services/session.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Exercise the actual production adapter and its native widgets. */
#include "../src/ui/c_language_view.c"
typedef struct fixture { const char *directory,*server; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(v) do { ++f->checks; if(!(v)) { fprintf(stderr,"c_header_context_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideCLanguageViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int settle(xide_app* app) { if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0; xideEditorPoll(app); xideTaskViewPoll(app); xideLanguageViewPoll(app); xideCLanguageViewPoll(app); xideWorkbenchLayout(app); return xuiLayout(app->context)==XUI_OK && !app->frameError; }
static int ready(xide_app* app) { uint64_t until=GetTickCount64()+30000; do { if(!settle(app)) return 0; if(app->cLanguageView->ready && app->cLanguageView->symbolResult && app->cLanguageView->received==(int)app->cLanguageView->inputs->count) return 1; if(app->cLanguageView->paused) return 0; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int check(xide_app* app) { return xideActionExecute(app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && ready(app); }
static int position(xide_app* app,int offset) { xide_document* document=app->activeDocument; return xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(document->editor),xuiCodeEditGetDocument(document->editor),offset,0)==XUI_OK && xuiSetFocusWidget(app->context,document->editor)==XUI_OK && settle(app); }
static int key(xide_app* app,int value,uint32_t modifiers) { return xuiInputKeyDownEx(app->context,value,modifiers,NULL)==XUI_OK && xuiInputKeyUpEx(app->context,value,modifiers,NULL)==XUI_OK && settle(app); }
static int hover(xide_app* app,const char* type) { uint64_t until=GetTickCount64()+30000; do { if(!settle(app)) return 0; xide_c_language_view* view=app->cLanguageView; if(view->hoverResult) { const char* content=cvstr(cvget(cvget(view->hoverResult,"contents"),"value")); fprintf(stderr,"ACTUAL HOVER: %s\n",content ? content : "null"); return content && strstr(content,type) && xideCLanguageViewHintOpen(app); } if(!view->hover.token && !view->hoverDue) return 0; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int capture(xide_app* app,const char* path) { if(!settle(app) || xuiUpdate(app->context,0)!=XUI_OK || xuiLayout(app->context)!=XUI_OK) return 0; xui_rect_i_t r={0,0,app->viewportWidth,app->viewportHeight}; return app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&r,1)==XUI_OK && xideAppCapture(app,path)==XGE_OK; }
static int setMacro(xide_project* project,const char* type,xide_model_error* error) {
 xvalue* configs=xideProjectConfigurationsJson(project); char data[256]; snprintf(data,sizeof(data),"{\"defines\":[\"XIDE_HEADER_TYPE=%s\"],\"undefines\":[],\"systemIncludes\":[],\"forcedIncludes\":[]}",type);
 xvalue* analysis=xrtJsonParse((xstrview){data,strlen(data)}); int okay=configs && analysis && xrtValueObjectSet(xrtValueArrayGet(configs,0),(xstrview){"cAnalysis",9},analysis) && xideProjectApplyConfigurations(project,configs,project->activeConfiguration,error);
 xrtValueRelease(configs);xrtValueRelease(analysis);return okay;
}
static int navigate(xide_app* app) {
 if(!key(app,XUI_KEY_F12,0)) return 0;
 uint64_t until=GetTickCount64()+30000; int okay=1;
 while(okay && app->cLanguageView->definition.token && GetTickCount64()<until) { okay=settle(app); Sleep(5); }
 return okay && !app->cLanguageView->definition.token;
}
static xvalue* savedDocument(xvalue* saved,const char* name) {
 xvalue* docs=(xvalue*)cvget(saved,"openDocuments");
 for(size_t i=0;i<xrtValueCount(docs);++i) { xvalue* item=xrtValueArrayGet(docs,i); const char* path=cvstr(cvget(item,"path")); if(path && !strcmp(xidePathFilename(path),name)) return item; }
 return NULL;
}
static int verify(void* user) {
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; xide_project project={0},second={0}; int began=0;
 xvalue *saved=NULL,*invalid=NULL; char *sessions=NULL,*entry2=NULL,*manifest2=NULL,*metadata=NULL,*primaryId=NULL,*originId=NULL,*floatImage=NULL,*restoredImage=NULL;
 char *root=NULL,*directory=NULL,*evidence=NULL,*entry=NULL,*outer=NULL,*leaf=NULL,*xl=NULL,*manifest=NULL,*settings=NULL,*before=NULL,*after=NULL,*navigation=NULL;
 char unique[90]; snprintf(unique,sizeof(unique),"native-trace-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(f->directory,unique); directory=xidePathResolve(root,"project 空间😀"); evidence=xidePathResolve(root,"evidence");
 entry=xidePathResolve(directory,"entry.c"); outer=xidePathResolve(directory,"outer.h"); leaf=xidePathResolve(directory,"隐式 空间😀.h"); xl=xidePathResolve(directory,"main.xl"); manifest=xidePathResolve(directory,"project.xlp"); settings=xidePathResolve(root,"settings.json"); before=xidePathResolve(evidence,"implicit-double.png"); after=xidePathResolve(evidence,"implicit-int.png"); navigation=xidePathResolve(evidence,"implicit-f12.png");
 const char* source="#include \"outer.h\"\nint main(void) { return traced_value; }\n"; const char* initial="#ifndef XIDE_HEADER_TYPE\n#define XIDE_HEADER_TYPE int\n#endif\nstatic XIDE_HEADER_TYPE traced_value = 1;\n"; const char* changed=initial;
 CHECK(root && directory && evidence && entry && outer && leaf && xl && manifest && settings && before && after && navigation && xrtDirCreateAll(directory) && xrtDirCreateAll(evidence));
 CHECK(writeText(entry,source) && writeText(outer,"#include \"隐式 空间😀.h\"\n") && writeText(leaf,initial) && writeText(xl,"func main(...) as int\n return 0\nend\n"));
 sessions=xidePathResolve(root,"sessions"); entry2=xidePathResolve(directory,"second.c"); manifest2=xidePathResolve(directory,"second.xlp"); floatImage=xidePathResolve(evidence,"header-float.png"); restoredImage=xidePathResolve(evidence,"header-restored-long.png"); CHECK(sessions && entry2 && manifest2 && floatImage && restoredImage);
 CHECK(xideProjectCreate(&project,manifest,"隐式头文件上下文",xl,&error) && xideProjectAddFile(&project,entry,"c",&error) && setMacro(&project,"double",&error) && xideProjectSave(&project,manifest,&error));
 app.hidden=1; app.sessionRoot=sessions; app.cServerPath=f->server; app.settingsPath=settings; app.projectPath=manifest; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 primaryId=cvcopy(app.workspaceModel.projects[0].id); CHECK(primaryId && xideSessionDirectory(&app));
 CHECK(xideEditorOpen(&app,entry,&error)); xide_document* c=app.activeDocument; int offset=(int)(strstr(source,"traced_value")-source)+2; CHECK(position(&app,offset) && check(&app));
 xide_c_language_view* view=app.cLanguageView; xide_file_snapshot guard={0}; CHECK(view->inputs->count==1 && view->received==1 && xideCAnalysisJobGuard(view->job,leaf,&guard) && xideCAnalysisJobGuard(view->job,outer,&guard));
 CHECK(position(&app,offset) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"double")); CHECK(capture(&app,before));
 CHECK(key(&app,XUI_KEY_F12,0)); uint64_t until=GetTickCount64()+30000; int settled=1; while(settled && view->definition.token && GetTickCount64()<until) { settled=settle(&app); Sleep(5); } CHECK(settled && !view->definition.token);
 CHECK(app.activeDocument!=c && xidePathLexicalEqual(app.activeDocument->path,leaf)); xui_code_selection_t selection={0}; CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(app.activeDocument->editor),&selection)==XUI_OK && selection.iAnchorOffset==(int)(strstr(changed,"traced_value")-changed) && selection.iCaretOffset==selection.iAnchorOffset+(int)strlen("traced_value"));
 CHECK(!strcmp(xuiCodeEditGetText(app.activeDocument->editor),changed) && !app.activeDocument->dirty && xideWorkbenchApplyTheme(&app,1)==XUI_OK && capture(&app,navigation));
 xide_document* header=app.activeDocument; int headerOffset=(int)(strstr(initial,"traced_value")-initial)+2;
 CHECK(check(&app)); CHECK(position(&app,headerOffset) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"static double traced_value") && capture(&app,navigation));
 xide_project* p=xideWorkspaceFindProject(&app.workspaceModel,primaryId);
 CHECK(p && p->cSources.count==1 && p->files.count==2 && header->cAnalysisContext && !strcmp(cvstr(cvget(header->cAnalysisContext,"project")),primaryId));
 originId=cvcopy(cvstr(cvget(header->cAnalysisContext,"sourceId"))); CHECK(originId);
 CHECK(setMacro(p,"long",&error) && settle(&app) && !view->ready && !view->hoverResult && !view->symbolResult && !xideCLanguageViewHintOpen(&app));
 CHECK(check(&app) && position(&app,headerOffset) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"static long traced_value"));
 CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(header->editor),0,"// unsaved header\n")==XUI_OK && header->dirty && settle(&app) && !view->ready);
 CHECK(check(&app) && position(&app,headerOffset+(int)strlen("// unsaved header\n")) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"static long traced_value"));
 CHECK(key(&app,'Z',XUI_MOD_CTRL) && !header->dirty && !strcmp(xuiCodeEditGetText(header->editor),initial) && header->cAnalysisContext);
 CHECK(writeText(entry2,source) && xideProjectCreate(&second,manifest2,"另一 C 上下文",xl,&error) && xideProjectAddFile(&second,entry2,"c",&error) && setMacro(&second,"float",&error) && xideProjectSave(&second,manifest2,&error));
 CHECK(xideWorkspaceAddProject(&app.workspaceModel,manifest2,"secondary",&error) && xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,"secondary"));
 CHECK(xideEditorOpen(&app,entry2,&error)); xide_document* c2=app.activeDocument;
 CHECK(position(&app,offset) && check(&app) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"static float traced_value") && navigate(&app));
 CHECK(app.activeDocument==header && !strcmp(cvstr(cvget(header->cAnalysisContext,"project")),"secondary") && !header->dirty);
 CHECK(check(&app) && position(&app,headerOffset) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"static float traced_value") && capture(&app,floatImage));
 CHECK(xideWorkspaceViewSelectProject(&app,primaryId) && settle(&app) && check(&app) && position(&app,headerOffset) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"static float traced_value"));
 CHECK(xideEditorShow(&app,c)==XUI_OK && position(&app,offset) && check(&app) && navigate(&app) && app.activeDocument==header);
 CHECK(check(&app) && position(&app,headerOffset) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"static long traced_value"));
 CHECK(xideEditorClose(&app,c2,1,&error) && xideWorkspaceRemoveProject(&app.workspaceModel,"secondary",&error) && xideWorkspaceViewRefresh(&app)==XUI_OK && xideEditorShow(&app,header)==XUI_OK);
 CHECK(xideEditorShow(&app,c)==XUI_OK && xideRenameViewOpen(&app) && xuiEditSetText(xideRenameViewControl(&app,XIDE_RENAME_NAME),"renamed.c")==XUI_OK && xideRenameViewApply(&app,&error) && !xideRenameViewIsOpen(&app));
 CHECK(!strcmp(xidePathFilename(c->path),"renamed.c") && !strcmp(cvstr(cvget(header->cAnalysisContext,"sourceId")),originId) && xideEditorShow(&app,header)==XUI_OK && check(&app));
 CHECK(position(&app,headerOffset) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"static long traced_value"));
 p=xideWorkspaceFindProject(&app.workspaceModel,primaryId);
 CHECK(p && xrtValueObjectSetNew(header->cAnalysisContext,cvsv("future"),xrtJsonParse(cvsv("{\"preserve\":true}"))) && xideProjectSave(p,NULL,&error) && xideSessionSave(&app,0,&error));
 metadata=xidePathResolve(xideSessionDirectory(&app),"session.json"); saved=metadata ? xrtJsonParseFile(metadata) : NULL;
 xvalue* item=saved ? savedDocument(saved,xidePathFilename(leaf)) : NULL;
 CHECK(item && xideSessionValidate(saved) && xrtValueEqual(cvget(item,"cAnalysisContext"),header->cAnalysisContext));
 const char* invalidFields[]={"project","source","sourceId"};
 for(size_t i=0;i<3;++i) { invalid=xrtValueDeepClone(saved); xvalue* context=(xvalue*)cvget(savedDocument(invalid,xidePathFilename(leaf)),"cAnalysisContext"); CHECK(invalid && context && xrtValueObjectSetNew(context,cvsv(invalidFields[i]),xrtValueInt(7)) && !xideSessionValidate(invalid)); xrtValueRelease(invalid); invalid=NULL; }
 invalid=xrtValueDeepClone(saved); CHECK(invalid && xrtValueObjectSetNew(savedDocument(invalid,xidePathFilename(leaf)),cvsv("cAnalysisContext"),xrtValueNull()) && !xideSessionValidate(invalid)); xrtValueRelease(invalid); invalid=NULL;
 if(began) { xgeEnd(); began=0; } xideAppDestroy(&app); memset(&app,0,sizeof(app));
 app.hidden=1; app.sessionRoot=sessions; app.cServerPath=f->server; app.settingsPath=settings; app.projectPath=manifest;
 CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; view=app.cLanguageView; header=xideDocumentsFind(&app.documents,leaf);
 CHECK(header && header->cAnalysisContext && cvget(header->cAnalysisContext,"future") && !strcmp(cvstr(cvget(header->cAnalysisContext,"sourceId")),originId) && !header->dirty && xideEditorShow(&app,header)==XUI_OK);
 CHECK(position(&app,headerOffset) && check(&app) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"static long traced_value") && capture(&app,restoredImage));
 p=xideWorkspaceFindProject(&app.workspaceModel,primaryId); CHECK(p && xideProjectRemoveFile(p,originId,&error) && settle(&app) && !view->ready && !view->hoverResult && !view->symbolResult && !xideActionQuery(&app,XIDE_ACTION_DEFINITION).enabled);
 xide_c_inputs* rejected=xideCInputsCapture(&app,header,&error); CHECK(!rejected); memset(&error,0,sizeof(error));
 CHECK(xideEditorClose(&app,header,1,&error) && !header->cAnalysisContext && xideEditorOpen(&app,leaf,&error) && !app.activeDocument->cAnalysisContext);
 CHECK(position(&app,headerOffset) && check(&app) && key(&app,'I',XUI_MOD_CTRL) && hover(&app,"static int traced_value") && capture(&app,after));
 CHECK(!app.activeDocument->dirty && !strcmp(xuiCodeEditGetText(app.activeDocument->editor),initial));
 printf("c_header_context_view_test passed: %d native assertions; real project macro F12, two-project rebinding, invalidation, unsaved overlay/undo, source rename, session restoration and removed-origin rejection; 5 renders in %s.\n",f->checks,root);
cleanup:
 if(began) { xgeEnd(); }
 xideAppDestroy(&app); xideProjectClear(&project); xideProjectClear(&second); xrtValueRelease(saved); xrtValueRelease(invalid); free(sessions); free(entry2); free(manifest2); free(metadata); free(primaryId); free(originId); free(floatImage); free(restoredImage); free(root); free(directory); free(evidence); free(entry); free(outer); free(leaf); free(xl); free(manifest); free(settings); free(before); free(after); free(navigation); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="C physical input trace"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=3 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.server=argv[2]}; wchar_t name[100]; swprintf(name,100,L"CInputTrace-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,120000)!=WAIT_OBJECT_0) { fputs("Native C input trace timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code; }
