#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "services/file_sync.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Real view included only to assert late-response acceptance decisions. */
#include "../src/ui/language_view.c"
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"symbol_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideLanguageViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int settle(xide_app* app) { if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0; xideEditorPoll(app); xideFileSyncPoll(app); xideTaskViewPoll(app); xideLanguageViewPoll(app); xideWorkbenchLayout(app); return xuiLayout(app->context)==XUI_OK && !app->frameError; }
static int waitChecked(xide_app* a) { uint64_t until=GetTickCount64()+20000; do { if(!settle(a)) return 0; if(!a->languageView->busy && a->languageView->snapshotResult) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int caret(xide_document* d) { xui_code_selection_t s={0}; return xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&s)==XUI_OK ? s.iCaretOffset : -1; }
static int gotoOffset(xide_app* a,xide_document* d,int offset) { return xideEditorShow(a,d)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),offset,0)==XUI_OK && xuiSetFocusWidget(a->context,d->editor)==XUI_OK; }
static int replace(xide_document* d,const char* text) { xui_code_document core=xuiCodeEditGetDocument(d->editor); return xuiCodeDocumentReplace(core,0,xuiCodeDocumentGetLength(core),text)==XUI_OK; }
static int capture(xide_app* a,const char* path) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK; }
#include "ui/layout_state.h"

static int waitOutline(xide_app* a)
{ uint64_t until=GetTickCount64()+6000; do { if(!settle(a)) return 0; if(!a->languageView->outlineRequest && a->languageView->outlineResult && a->languageView->outlineDocument==a->activeDocument) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int waitHover(xide_app* a)
{ uint64_t until=GetTickCount64()+6000; do { if(!settle(a)) return 0; if(a->languageView->hoverResult && xuiCodeEditIsHintOpen(a->activeDocument->editor)) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int legacyRemove(xvalue* v)
{
 if(xrtValueType(v)==XVALUE_OBJECT) {
  xvalue* tabs=get(v,"tabs");
  if(tabs && xrtValueType(tabs)==XVALUE_ARRAY) for(size_t i=xrtValueCount(tabs);i>0;--i) if(formatted(xrtValueArrayGet(tabs,i-1),"outline")) xrtValueArrayRemove(tabs,i-1,1);
  for(size_t i=0;i<xrtValueCount(v);++i) { xstrview k={0}; xvalue* value=NULL; value=xrtValueObjectAt(v,i,&k); if(value) legacyRemove(value); }
 } else if(xrtValueType(v)==XVALUE_ARRAY) for(size_t i=xrtValueCount(v);i>0;--i) { xvalue* item=xrtValueArrayGet(v,i-1); if(formatted(get(item,"id"),"outline")) xrtValueArrayRemove(v,i-1,1); else legacyRemove(item); }
 return 1;
}
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; int began=0;
 char *root=NULL,*entry=NULL,*dep=NULL,*native=NULL,*light=NULL,*dark=NULL; xvalue *state=NULL,*fake=NULL;
 char unique[60]; snprintf(unique,sizeof(unique),"symbols-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(f->directory,unique); entry=xidePathResolve(root,"入口😀.xl"); dep=xidePathResolve(root,"helper.xl"); native=xidePathResolve(root,"helper.c"); light=xidePathResolve(root,"outline-hover-light.png"); dark=xidePathResolve(root,"outline-hover-dark.png");
 const char *saved="func main(...) as int\n    return 0\nend\n",*overload="func pick(int value) as int\n    return value\nend\nfunc pick(string value) as int\n    return 2\nend\nfunc main(...) as int\n    string 中文 = \"😀\"; auto second = pick(\"x\")\n    return second - 2\nend\n",*helper="export func answer() as int\n    return 42\nend\n",*overlay="\n\nexport func answer() as int\n    return 42\nend\n",*imported="import helper\nfunc main(...) as int\n    return helper.answer() - 42\nend\n";
 CHECK(root && entry && dep && native && light && dark && xrtDirCreateAll(root) && writeText(entry,saved) && writeText(dep,helper) && writeText(native,"int helper(void) { return 1; }\n"));
 app.hidden=1; app.noSession=1; app.compilerPath=f->compiler; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideActionCount()==71 && xideActionNamed("language.outline")->id==74 && xideActionNamed("language.hover")->id==75 && !xideLanguageViewCanHover(&app));
 CHECK(xuiDockPanelGetPaneActiveWindow(app.dock,app.helpPane)==app.windows[XIDE_HELP]);
 state=xideLayoutCapture(&app,&error); CHECK(state && legacyRemove(state) && xideLayoutRestore(&app,state,&error) && settle(&app)); xrtValueRelease(state); state=NULL;
 xui_dock_window_info_t help={.iSize=sizeof(help)},left={.iSize=sizeof(left)},right={.iSize=sizeof(right)};
 CHECK(xuiDockPanelGetWindowInfo(app.dock,app.windows[XIDE_HELP],&help)==XUI_OK && help.iRegion==XUI_DOCK_PANEL_REGION_DOCUMENT && xuiDockPanelGetWindowInfo(app.dock,app.windows[XIDE_COMMANDS],&left)==XUI_OK && left.iRegion==XUI_DOCK_PANEL_REGION_LEFT && xuiDockPanelGetWindowInfo(app.dock,app.windows[XIDE_WORKSPACE],&right)==XUI_OK && right.iRegion==XUI_DOCK_PANEL_REGION_RIGHT && xuiDockPanelGetPaneActiveWindow(app.dock,help.iPane)==app.windows[XIDE_HELP]);
 CHECK(xideActionExecute(&app,XIDE_ACTION_OUTLINE)==XIDE_ACTION_DISPATCHED && settle(&app));
 xui_dock_window_info_t outline={.iSize=sizeof(outline)}; CHECK(xuiDockPanelGetWindowInfo(app.dock,app.windows[XIDE_OUTLINE],&outline)==XUI_OK && outline.iPane==help.iPane && outline.iRegion==XUI_DOCK_PANEL_REGION_DOCUMENT);
 CHECK(xideEditorOpen(&app,entry,&error) && xideEditorOpen(&app,dep,&error) && xideEditorOpen(&app,native,&error) && settle(&app)); xide_document *xl=xideDocumentsFind(&app.documents,entry),*dependency=xideDocumentsFind(&app.documents,dep),*c=app.activeDocument;
 CHECK(!strcmp(c->language,"c") && !xideLanguageViewCanHover(&app) && !app.languageView->outlineCount);
 CHECK(replace(xl,overload) && xideEditorShow(&app,xl)==XUI_OK && xideActionExecute(&app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && waitChecked(&app) && waitOutline(&app) && app.languageView->outlineCount==3 && xideLanguageViewCanHover(&app));
 CHECK(!strcmp(string(get(app.languageView->outlineItems[1].symbol,"name")),"pick"));
 CHECK(xideActionExecute(&app,XIDE_ACTION_OUTLINE)==XIDE_ACTION_DISPATCHED && settle(&app));
 CHECK(xuiTreeViewSetSelected(app.languageView->outline,2)==XUI_OK && xuiSetFocusWidget(app.context,app.languageView->outline)==XUI_OK && xuiInputKeyDown(app.context,XUI_KEY_ENTER,0)==XUI_OK && xuiInputKeyUp(app.context,XUI_KEY_ENTER,0)==XUI_OK && settle(&app));
 int definition=(int)(strstr(overload,"pick(string")-overload),call=(int)(strstr(overload,"pick(\"x\")")-overload); CHECK(caret(xl)==definition);
 CHECK(gotoOffset(&app,xl,call) && settle(&app)); xui_rect_t glyph={0}; CHECK(xuiCodeEditGetTextOffsetRect(xl->editor,call,&glyph)==XUI_OK && glyph.fW>0 && xuiInputPointerMove(app.context,(int)(glyph.fX+1),(int)(glyph.fY+glyph.fH/2),0)==XUI_OK && waitHover(&app));
 CHECK(strstr(string(get(get(app.languageView->hoverResult,"contents"),"value")),"pick(string value)") && capture(&app,light));
 CHECK(xuiInputPointerMove(app.context,20,120,0)==XUI_OK && settle(&app) && !xuiCodeEditIsHintOpen(xl->editor) && !app.languageView->hoverResult);
 CHECK(gotoOffset(&app,xl,call) && xuiInputKeyDown(app.context,'I',XUI_MOD_CTRL)==XUI_OK && xuiInputKeyUp(app.context,'I',XUI_MOD_CTRL)==XUI_OK && waitHover(&app));
 CHECK(xideWorkbenchApplyTheme(&app,1)==XUI_OK && capture(&app,dark));
 fake=xrtValueDeepClone(app.languageView->hoverResult); CHECK(fake && requestCaretHover(&app)); put(get(get(fake,"range"),"end"),"character",xrtValueInt(999999)); applyHover(&app,fake,NULL); xrtValueRelease(fake); fake=NULL; CHECK(!app.languageView->hoverResult && !xuiCodeEditIsHintOpen(xl->editor));
 fake=xrtValueDeepClone(app.languageView->outlineResult); CHECK(fake); clearOutline(app.languageView); app.languageView->outlineDocument=xl; app.languageView->outlineId=xl->id; app.languageView->outlineVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor)); app.languageView->outlineCheck=app.languageView->lastSnapshotId; put(get(get(xrtValueArrayGet(fake,0),"selectionRange"),"end"),"character",xrtValueInt(999999)); applyOutline(&app,fake,NULL); xrtValueRelease(fake); fake=NULL; CHECK(!app.languageView->outlineResult && waitOutline(&app));
 /* Current caret, tab, document and batch guard every late hover/outline. */
 CHECK(requestCaretHover(&app) && gotoOffset(&app,xl,0) && settle(&app) && !app.languageView->hoverResult && !xuiCodeEditIsHintOpen(xl->editor));
 CHECK(gotoOffset(&app,xl,call) && requestCaretHover(&app) && xideEditorShow(&app,c)==XUI_OK && settle(&app) && !app.languageView->hoverResult && !app.languageView->outlineCount && !xideLanguageViewCanHover(&app));
 CHECK(xideEditorShow(&app,xl)==XUI_OK && waitOutline(&app) && replace(xl,imported) && replace(dependency,overlay) && settle(&app) && !app.languageView->outlineCount && !app.languageView->hoverResult && waitChecked(&app) && waitOutline(&app));
 call=(int)(strstr(imported,"answer")-imported); CHECK(gotoOffset(&app,xl,call) && requestCaretHover(&app) && waitHover(&app) && strstr(string(get(get(app.languageView->hoverResult,"contents"),"value")),"export func answer"));
 CHECK(xideEditorShow(&app,dependency)==XUI_OK && settle(&app) && waitOutline(&app) && app.languageView->outlineCount==1 && !strcmp(string(get(app.languageView->outlineItems[0].symbol,"name")),"answer"));
 CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && settle(&app) && !app.languageView->outlineResult && !app.languageView->hoverResult && !xideLanguageViewCanHover(&app));
 xide_text_file file={0}; CHECK(xideTextFileRead(entry,&file,&error) && !strcmp(file.text,saved)); xideTextFileClear(&file); CHECK(xideTextFileRead(dep,&file,&error) && !strcmp(file.text,helper)); xideTextFileClear(&file);
 printf("symbol_view_test passed: real mouse hover, Ctrl+I, resolved overload, central outline Enter jump, old ten-panel layout restore, unsaved dependency outline, malformed range and caret/tab/edit/cancel/C rejection and original files preserved; renders in %s.\n",root);
cleanup:
 xrtValueRelease(fake); xrtValueRelease(state); if(began) xgeEnd(); xideAppDestroy(&app); free(root); free(entry); free(dep); free(native); free(light); free(dark); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang symbol verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=3 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argv[2]}; wchar_t name[100]; swprintf(name,100,L"XlangDefinitionVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(t) { if(WaitForSingleObject(t,120000)!=WAIT_OBJECT_0) { fputs("symbol_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(t,&code); CloseHandle(t); } CloseDesktop(f.desktop); return (int)code; }
