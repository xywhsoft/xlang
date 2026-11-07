#define _WIN32_WINNT 0x0601
#include "ui/find_view.h"
#include "ui/editor_view.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture {HDESK desktop;const char* directory;int started,failed,checks;} fixture;
#define CHECK(e) do {++f->checks;if(!(e)){fprintf(stderr,"find_follow_view_test:%d: %s; %s\n",__LINE__,#e,error.message);f->failed=1;goto cleanup;}}while(0)
static int settle(xide_app* a){if(xuiDispatchPendingEvents(a->context)!=XUI_OK)return 0;xideEditorPoll(a);xideWorkbenchLayout(a);return xuiUpdate(a->context,0.02f)==XUI_OK && xuiLayout(a->context)==XUI_OK && !a->frameError;}
static int key(xide_app* a,int k,uint32_t m){return xuiInputKeyDownEx(a->context,k,m,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,0,NULL)==XUI_OK && settle(a);}

/* Inspect actual XGE native controls; the IDE has no control catalogue/UI. */
enum {XIDE_FIND_QUERY,XIDE_FIND_REPLACEMENT,XIDE_FIND_CASE,XIDE_FIND_WORD,XIDE_FIND_REGEX,XIDE_FIND_ESCAPE,XIDE_FIND_SELECTION,XIDE_FIND_ALL_EDITORS,XIDE_FIND_PREVIOUS,XIDE_FIND_NEXT,XIDE_FIND_ALL,XIDE_FIND_REPLACE,XIDE_FIND_REPLACE_ALL,XIDE_FIND_CLOSE,XIDE_FIND_RESULTS,XIDE_FIND_STATUS};
static xui_widget findControl(xide_app* a,xui_widget w,const char* type,const char* text,int* index){
 if(!w)return NULL;
 if(xuiWidgetGetType(w)==xuiWidgetFindType(a->context,type)){
  const char* label=!strcmp(type,"button") ? xuiButtonGetText(w):!strcmp(type,"checkbox") ? xuiCheckBoxGetText(w):"";
  if((!text || !strcmp(label,text)) && (*index)--==0)return w;
 }
 for(xui_widget c=xuiWidgetGetFirstChild(w);c;c=xuiWidgetGetNextSibling(c)){xui_widget found=findControl(a,c,type,text,index);if(found)return found;}
 return NULL;
}
static xui_widget control(xide_app* a,int i){
 const int labels[]={XUI_TR_FIND_CASE,XUI_TR_FIND_WORD,XUI_TR_FIND_REGEX,XUI_TR_FIND_ESCAPE,XUI_TR_FIND_SELECTION,XUI_TR_FIND_SCOPE,XUI_TR_FIND_PREVIOUS,XUI_TR_FIND_NEXT,XUI_TR_FIND_ALL,XUI_TR_REPLACE_CURRENT,XUI_TR_REPLACE_ALL,XUI_TR_DOCK_CLOSE};
 const char* type=i<2 ? "input":i<=XIDE_FIND_ALL_EDITORS ? "checkbox":i<=XIDE_FIND_CLOSE ? "button":i==XIDE_FIND_RESULTS ? "tableview":"label";
 int index=i==XIDE_FIND_REPLACEMENT ? 1:0;
 return findControl(a,xideFindViewWindow(a),type,i>=2 && i<=XIDE_FIND_CLOSE ? xuiTranslate(a->context,labels[i-2]):NULL,&index);
}
static int refresh(xide_app* a){return xuiCodeFindScopeRefresh(a->editorFindScope);}

static int click(xide_app* a,int i){xui_rect_t r=xuiWidgetGetWorldRect(control(a,i));return r.fW>0 && r.fH>0 && xuiInputPointerMove(a->context,r.fX+r.fW/2,r.fY+r.fH/2,0)==XUI_OK && xuiInputPointerDown(a->context,r.fX+r.fW/2,r.fY+r.fH/2,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,r.fX+r.fW/2,r.fY+r.fH/2,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a);}
static int query(xide_app* a,const char* s){return xuiInputSetText(control(a,XIDE_FIND_QUERY),s)==XUI_OK && refresh(a)==XUI_OK && settle(a);}
static int matches(xide_app* a){return xuiTableViewGetRowCount(control(a,XIDE_FIND_RESULTS));}
static int waitMatches(xide_app* a,int n){uint64_t until=GetTickCount64()+1500;do{if(!settle(a))return 0;if(matches(a)==n)return 1;Sleep(5);}while(GetTickCount64()<until);return 0;}
static int run(void* user){
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;xide_app a={0};xide_model_error error={0};int began=0;char *settings=xidePathResolve(f->directory,"settings.json"),*image=xidePathResolve(f->directory,"shared-find.png");
 CHECK(settings && image);a.hidden=1;a.noSession=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;
 CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(settle(&a));
 xide_document* c=xideDocumentNew(&a.documents,"c",&error);CHECK(c && xideEditorShow(&a,c)==XUI_OK && xuiCodeEditSetText(c->editor,"alpha alpha 中😀\n")==XUI_OK);
 xide_document* xl=xideDocumentNew(&a.documents,"xlang",&error);CHECK(xl && xideEditorShow(&a,xl)==XUI_OK && xuiCodeEditSetText(xl->editor,"alpha beta 中😀\n")==XUI_OK && settle(&a));
 CHECK(xuiSetFocusWidget(a.context,xl->editor)==XUI_OK && key(&a,'F',XUI_MOD_CTRL));xui_widget window=xideFindViewWindow(&a);
 CHECK(window==xuiCodeFindScopeGetFindWindow(a.editorFindScope) && window==xuiCodeEditGetFindWindow(c->editor) && window==xuiCodeEditGetFindWindow(xl->editor) && xuiWindowIsOpen(window) && !xideActionsHasModal(&a) && query(&a,"alpha") && matches(&a)==1);
 CHECK(xideEditorShow(&a,c)==XUI_OK && settle(&a));CHECK(waitMatches(&a,2) && !strcmp(xuiInputGetText(control(&a,XIDE_FIND_QUERY)),"alpha") && window==xideFindViewWindow(&a) && xuiGetFocusWidget(a.context)==c->editor);
 CHECK(xuiDockPanelSetPaneActiveWindow(a.dock,a.documentPane,xl->window)==XUI_OK && settle(&a));CHECK(waitMatches(&a,1) && a.activeDocument==xl);
 CHECK(xuiSetFocusWidget(a.context,xl->editor)==XUI_OK && key(&a,'H',XUI_MOD_CTRL) && xuiWidgetGetVisible(control(&a,XIDE_FIND_REPLACEMENT)));
 CHECK(xuiInputSetText(control(&a,XIDE_FIND_REPLACEMENT),"result")==XUI_OK && click(&a,XIDE_FIND_REPLACE_ALL));
 CHECK(!strcmp(xuiCodeEditGetText(xl->editor),"result beta 中😀\n") && !strcmp(xuiCodeEditGetText(c->editor),"alpha alpha 中😀\n"));
 CHECK(click(&a,XIDE_FIND_ESCAPE) && xuiInputSetText(control(&a,XIDE_FIND_REPLACEMENT),"\\u0000tail")==XUI_OK && click(&a,XIDE_FIND_REPLACE_ALL) && !strcmp(xuiCodeEditGetText(xl->editor),"result beta 中😀\n") && !xrtGetError() && !strcmp(xuiLabelGetText(control(&a,XIDE_FIND_STATUS)),xuiTranslate(a.context,XUI_TR_FIND_INVALID_PATTERN)));
 CHECK(click(&a,XIDE_FIND_ESCAPE));
 CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(xl->editor))==XUI_OK && settle(&a) && !strcmp(xuiCodeEditGetText(xl->editor),"alpha beta 中😀\n"));
 CHECK(xuiInputSetText(control(&a,XIDE_FIND_QUERY),"alpha")==XUI_OK && xuiInputSetText(control(&a,XIDE_FIND_QUERY),"beta")==XUI_OK && waitMatches(&a,1));xui_code_find_result_t found={.iSize=sizeof(found)};CHECK(xuiCodeEditGetFindResult(xl->editor,0,&found)==XUI_OK && found.iStart==6 && query(&a,"alpha"));
 CHECK(click(&a,XIDE_FIND_ESCAPE));
 const char* invalid[]={"\\0tail","\\x00tail","\\u0000tail","\\uD800","\\uDC00","\\uD83D\\u0041","\\x","\\u123"};
 uint32_t guardedVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor));
 for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i)CHECK(xuiInputSetText(control(&a,XIDE_FIND_REPLACEMENT),invalid[i])==XUI_OK && click(&a,XIDE_FIND_REPLACE_ALL) && !strcmp(xuiLabelGetText(control(&a,XIDE_FIND_STATUS)),xuiTranslate(a.context,XUI_TR_FIND_INVALID_PATTERN)) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor))==guardedVersion && !xrtGetError());
 CHECK(click(&a,XIDE_FIND_REGEX) && query(&a,"not_present") && matches(&a)==0);
 CHECK(xuiInputSetText(control(&a,XIDE_FIND_REPLACEMENT),"\\uD800")==XUI_OK && click(&a,XIDE_FIND_REPLACE_ALL) && !strcmp(xuiLabelGetText(control(&a,XIDE_FIND_STATUS)),xuiTranslate(a.context,XUI_TR_FIND_INVALID_PATTERN)) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor))==guardedVersion && !xrtGetError());
 CHECK(click(&a,XIDE_FIND_REGEX) && query(&a,"alpha"));
 CHECK(xuiInputSetText(control(&a,XIDE_FIND_REPLACEMENT),"\\uD83D\\uDE00")==XUI_OK && click(&a,XIDE_FIND_REPLACE_ALL) && !strcmp(xuiCodeEditGetText(xl->editor),"😀 beta 中😀\n") && xuiCodeDocumentUndo(xuiCodeEditGetDocument(xl->editor))==XUI_OK);
 CHECK(click(&a,XIDE_FIND_ESCAPE));
 CHECK(click(&a,XIDE_FIND_ALL_EDITORS) && xuiCheckBoxGetChecked(control(&a,XIDE_FIND_ALL_EDITORS)) && query(&a,"alpha") && matches(&a)==3);
 CHECK(xuiSetFocusWidget(a.context,control(&a,XIDE_FIND_QUERY))==XUI_OK);a.activeDocument=NULL;CHECK(settle(&a) && query(&a,"alpha") && matches(&a)==3 && !xuiWidgetGetEnabled(control(&a,XIDE_FIND_REPLACE_ALL)));
 CHECK(click(&a,XIDE_FIND_NEXT) && a.activeDocument==c && xuiGetFocusWidget(a.context)==c->editor && matches(&a)==3);
 CHECK(key(&a,XUI_KEY_F3,0) && a.activeDocument==c && key(&a,XUI_KEY_F3,0) && a.activeDocument==xl && key(&a,XUI_KEY_F3,0) && a.activeDocument==c);
 CHECK(!strcmp(xuiInputGetText(control(&a,XIDE_FIND_QUERY)),"alpha") && query(&a,"中😀") && matches(&a)==2);
 CHECK(xuiCodeEditSetReadonly(c->editor,1)==XUI_OK && settle(&a) && !xuiWidgetGetEnabled(control(&a,XIDE_FIND_REPLACE_ALL)));
 CHECK(query(&a,"alpha") && matches(&a)==3 && click(&a,XIDE_FIND_REGEX));
 CHECK(xuiInputSetText(control(&a,XIDE_FIND_QUERY),"[")==XUI_OK && refresh(&a)==XUI_OK && !xrtGetError() && matches(&a)==0);
 CHECK(click(&a,XIDE_FIND_REGEX) && query(&a,"alpha") && matches(&a)==3);
 CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(xl->editor),0,"alpha ")==XUI_OK && matches(&a)==0);CHECK(waitMatches(&a,4));
 CHECK(xideEditorClose(&a,xl,1,&error) && settle(&a));CHECK(waitMatches(&a,2));
 uint64_t old=xl->id;xide_document* reused=xideDocumentNew(&a.documents,"text",&error);CHECK(reused==xl && reused->id!=old && xideEditorShow(&a,reused)==XUI_OK && xuiCodeEditSetText(reused->editor,"alpha alpha alpha\n")==XUI_OK && settle(&a));CHECK(waitMatches(&a,5));
 CHECK(xuiSetFocusWidget(a.context,control(&a,XIDE_FIND_QUERY))==XUI_OK && key(&a,XUI_KEY_ESCAPE,0) && !xuiWindowIsOpen(window));
 CHECK(xideEditorShow(&a,c)==XUI_OK && settle(&a) && !xuiWindowIsOpen(window));
 CHECK(xuiSetFocusWidget(a.context,c->editor)==XUI_OK && key(&a,'F',XUI_MOD_CTRL) && window==xideFindViewWindow(&a) && xuiCheckBoxGetChecked(control(&a,XIDE_FIND_ALL_EDITORS)) && !strcmp(xuiInputGetText(control(&a,XIDE_FIND_QUERY)),"alpha"));
 CHECK(xuiCodeEditSetReadonly(c->editor,0)==XUI_OK && xuiSetFocusWidget(a.context,c->editor)==XUI_OK && key(&a,'H',XUI_MOD_CTRL));
 CHECK(click(&a,XIDE_FIND_ALL_EDITORS) && !xuiCheckBoxGetChecked(control(&a,XIDE_FIND_ALL_EDITORS)) && xuiCodeSelectionSetRange(xuiCodeEditGetSelection(c->editor),xuiCodeEditGetDocument(c->editor),0,5)==XUI_OK && click(&a,XIDE_FIND_SELECTION) && query(&a,"alpha") && matches(&a)==1);
 CHECK(xuiInputSetText(control(&a,XIDE_FIND_REPLACEMENT),"z")==XUI_OK && click(&a,XIDE_FIND_REPLACE_ALL) && !strcmp(xuiCodeEditGetText(c->editor),"z alpha 中😀\n") && xuiCodeDocumentUndo(xuiCodeEditGetDocument(c->editor))==XUI_OK && !strcmp(xuiCodeEditGetText(c->editor),"alpha alpha 中😀\n"));
 CHECK(xuiDockPanelFloatWindow(a.dock,xl->window,(xui_rect_t){400,100,500,300})==XUI_OK && settle(&a));CHECK(xuiSetFocusWidget(a.context,xl->editor)==XUI_OK && settle(&a) && a.activeDocument==xl);
 CHECK(xuiInputSetText(control(&a,XIDE_FIND_REPLACEMENT),"b")==XUI_OK && query(&a,"alpha") && click(&a,XIDE_FIND_REPLACE_ALL) && !strcmp(xuiCodeEditGetText(xl->editor),"b b b\n") && !strcmp(xuiCodeEditGetText(c->editor),"alpha alpha 中😀\n"));
 CHECK(xgeEnd()==XGE_OK);began=0;a.dpiOverride=2;CHECK(xideAppFrame(&a)==XGE_OK && xgeBegin()==XGE_OK);began=1;CHECK(settle(&a));
 xui_rect_t bounds=xuiWidgetGetWorldRect(window),close=xuiWidgetGetWorldRect(control(&a,XIDE_FIND_CLOSE));CHECK(bounds.fX>=0 && bounds.fY>=0 && bounds.fX+bounds.fW<=a.viewportWidth && bounds.fY+bounds.fH<=a.viewportHeight && close.fY+close.fH<=bounds.fY+bounds.fH);
 xui_widget scroll=control(&a,XIDE_FIND_RESULTS);while(scroll && xuiWidgetGetType(scroll)!=xuiWidgetFindType(a.context,"scrollview"))scroll=xuiWidgetGetParent(scroll);CHECK(scroll && xuiScrollViewEnsureChildVisible(scroll,control(&a,XIDE_FIND_RESULTS))==XUI_OK && settle(&a));
 xui_rect_t table=xuiWidgetGetWorldRect(control(&a,XIDE_FIND_RESULTS)),viewport=xuiWidgetGetWorldRect(xuiScrollViewGetViewportWidget(scroll));CHECK(table.fY+table.fH<=viewport.fY+viewport.fH+1);
 const int fontSizes[]={8,24};for(size_t i=0;i<2;++i){CHECK(xideUiFontApply(&a,a.uiFontPath,fontSizes[i],&error) && settle(&a) && xuiLabelGetFont(control(&a,XIDE_FIND_STATUS))==a.uiFont && xuiWindowGetFont(window)==a.uiFont);bounds=xuiWidgetGetWorldRect(window);close=xuiWidgetGetWorldRect(control(&a,XIDE_FIND_CLOSE));CHECK(close.fY+close.fH<=bounds.fY+bounds.fH && close.fX+close.fW<=bounds.fX+bounds.fW);}
 CHECK(xuiUpdate(a.context,0)==XUI_OK);xui_rect_i_t rect={0,0,a.viewportWidth,a.viewportHeight};CHECK(a.proxy.surfaceClear(&a.proxy,a.target,0)==XUI_OK && xuiRender(a.context,a.target,&rect,1)==XUI_OK && xideAppCapture(&a,image)==XGE_OK);
 printf("find_follow_view_test passed: %d checks; shared modeless find/replace, native shortcuts, Dock/programmatic target switch, Unicode, all open C/xlang/text editors, navigation wrap, readonly, regex errors, stale ranges, close/reuse, one-step undo and retained session.\n",f->checks);
cleanup:if(began)xgeEnd();xideAppDestroy(&a);free(settings);free(image);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user){fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=1280;d.iHeight=900;d.sTitle="Shared IDE find verification";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;int r=xgeInit(&d);if(r==XGE_OK)r=xgeRun(run,f);xgeUnit();return r==XGE_OK && f->started && !f->failed ? 0:1;}
int main(int argc,char** argv){if(argc!=2 || !xrtDirCreateAll(argv[1]))return 2;fixture f={.directory=argv[1]};wchar_t name[80];swprintf(name,80,L"XlangFindFollow-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(t){if(WaitForSingleObject(t,90000)!=WAIT_OBJECT_0)ExitProcess(1);GetExitCodeThread(t,&code);CloseHandle(t);}CloseDesktop(f.desktop);return code;}
