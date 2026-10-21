#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include "theme/theme.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fixture { const char* directory; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(e) do { ++f->checks; if(!(e)) { fprintf(stderr,"find_view_test:%d: %s; %s\n",__LINE__,#e,error.message); f->failed=1; goto cleanup; } } while(0)
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int key(xide_app* a,int k,uint32_t mods) { return xuiInputKeyDownEx(a->context,k,mods,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,0,NULL)==XUI_OK && settle(a); }
static int pointer(xide_app* a,xui_rect_t r) { float x=r.fX+r.fW/2,y=r.fY+r.fH/2; return r.fW>0 && r.fH>0 && xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static xui_widget control(xide_app* a,xui_widget parent,const char* type,const char* text,int* index)
{
    if(!parent) return NULL;
    if(xuiWidgetGetType(parent)==xuiWidgetFindType(a->context,type)) {
        const char* label=!strcmp(type,"button") ? xuiButtonGetText(parent) : !strcmp(type,"checkbox") ? xuiCheckBoxGetText(parent) : "";
        if((!text || !strcmp(label,text)) && (*index)--==0) return parent;
    }
    for(xui_widget child=xuiWidgetGetFirstChild(parent);child;child=xuiWidgetGetNextSibling(child)) { xui_widget found=control(a,child,type,text,index); if(found) return found; }
    return NULL;
}
static xui_widget get(xide_app* a,xide_document* d,const char* type,int text,int index) { return control(a,xuiCodeEditGetFindWindow(d->editor),type,text ? xuiTranslate(a->context,text) : NULL,&index); }
static int button(xide_app* a,xide_document* d,int text) { xui_widget w=get(a,d,"button",text,0); return w && pointer(a,xuiWidgetGetWorldRect(w)); }
static int field(xide_app* a,xide_document* d,int index,const char* text) { xui_widget w=get(a,d,"input",0,index); return w && xuiInputSetText(w,text)==XUI_OK && settle(a); }
static int openSearch(xide_app* a,xide_document* d,int replace) { return xideEditorShow(a,d)==XUI_OK && settle(a) && key(a,replace ? 'H' : 'F',XUI_MOD_CTRL) && xuiWidgetGetVisible(xuiCodeEditGetFindWindow(d->editor)); }
static int dismiss(xide_app* a,xide_document* d) { xui_widget input=get(a,d,"input",0,0);return input && xuiSetFocusWidget(a->context,input)==XUI_OK && settle(a) && key(a,XUI_KEY_ESCAPE,0) && !xuiWidgetGetVisible(xuiCodeEditGetFindWindow(d->editor)); }
static int capture(xide_app* a,const char* p) { xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return xuiUpdate(a->context,0)==XUI_OK && a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,p)==XGE_OK; }
static int sameFile(const char* p,const char* text) { size_t n=0;unsigned char* raw=xrtFileReadAllLimit(p,4096,&n);int same=raw && n==strlen(text) && !memcmp(raw,text,n);xrtFree(raw);return same; }
static int verify(void* user)
{
    fixture* f=user; if(f->started) return XGE_OK; f->started=1;
    xide_app app={0}; xide_model_error error={0}; int began=0; char *root=NULL,*cPath=NULL,*xlPath=NULL,*settings=NULL,*savedAs=NULL,*images[4]={0};
    const char* cText="// 中😀 target 中😀\nint alpha(void) { return 1; } // alpha2\n";
    const char* xlText="#doc\n描述：中😀\n#end\nfunc main() as int\n var 变量 as int = 1\n return 变量 // 中😀\nend\n";
    char* id=xideCreateId("find"); root=id ? xidePathResolve(f->directory,id) : NULL;free(id);CHECK(root && xrtDirCreateAll(root));
    cPath=xidePathResolve(root,"same.c");xlPath=xidePathResolve(root,"same.xl");settings=xidePathResolve(root,"settings.json");savedAs=xidePathResolve(root,"改名后 中文.c");CHECK(cPath && xlPath && settings && savedAs);
    CHECK(xrtFileWriteAtomic(cPath,(xbytesview){(const unsigned char*)cText,strlen(cText)}) && xrtFileWriteAtomic(xlPath,(xbytesview){(const unsigned char*)xlText,strlen(xlText)}));
    const char* names[]={"native-find-open-documents.png","native-replace-c-dark.png","native-find-xlang.png","native-find-renamed.png"};
    for(int i=0;i<4;++i) { images[i]=xidePathResolve(root,names[i]); CHECK(images[i]); }
    app.hidden=1;app.noSession=1;app.settingsPath=settings;
    CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(settle(&app) && app.editorFindScope && xuiCodeFindScopeGetEditorCount(app.editorFindScope)==0);
    CHECK(xideEditorOpen(&app,cPath,&error) && settle(&app));xide_document* c=app.activeDocument;
    CHECK(xideEditorOpen(&app,xlPath,&error) && settle(&app));xide_document* xl=app.activeDocument;
    CHECK(!strcmp(c->language,"c") && !strcmp(xl->language,"xlang") && xuiCodeFindScopeGetEditorCount(app.editorFindScope)==2);
    uint32_t cv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)),xv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor));
    CHECK(openSearch(&app,c,0) && field(&app,c,0,"中😀"));
    xui_widget scopeCheck=get(&app,c,"checkbox",XUI_TR_FIND_SCOPE,0);CHECK(scopeCheck && pointer(&app,xuiWidgetGetWorldRect(scopeCheck)) && xuiCheckBoxGetChecked(scopeCheck));
    CHECK(button(&app,c,XUI_TR_FIND_ALL) && xuiCodeFindScopeGetResultCount(app.editorFindScope)==4);
    xui_widget table=get(&app,c,"tableview",0,0);CHECK(table && xuiTableViewGetRowCount(table)==4 && capture(&app,images[0]));
    int xlIndex=-1;xui_code_find_result_t match={0};
    for(int i=0;i<4;++i) { CHECK(xuiCodeFindScopeGetResult(app.editorFindScope,i,&match)==XUI_OK);if(match.pEditor==xl->editor) { xlIndex=i;break; } }
    CHECK(xlIndex>=0 && !strcmp(xuiCodeFindScopeGetResultEditorName(app.editorFindScope,xlIndex),xl->path));
    xui_rect_t cell={0};CHECK(xuiTableViewEnsureCellVisible(table,xlIndex,0)==XUI_OK && settle(&app) && xuiTableViewGetCellRect(table,xlIndex,0,&cell)==XUI_OK);
    xui_rect_t tableWorld=xuiWidgetGetWorldRect(table);cell.fX+=tableWorld.fX;cell.fY+=tableWorld.fY;
    CHECK(pointer(&app,cell) && app.activeDocument==xl && xuiGetFocusWidget(app.context)==xl->editor);
    int start=0,end=0;CHECK(xuiCodeSelectionGetRange(xuiCodeEditGetSelection(xl->editor),&start,&end)==XUI_OK && end-start==(int)strlen("中😀") && !memcmp(xlText+start,"中😀",strlen("中😀")));
    CHECK(xuiCodeFindScopeGetResultCount(app.editorFindScope)==4 && !c->dirty && !xl->dirty && cv==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)) && xv==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor)));
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"// changed\n")==XUI_OK && settle(&app));
    CHECK(xuiCodeFindScopeGetResultCount(app.editorFindScope)==0 && xuiTableViewGetRowCount(table)==0 && xuiCodeFindScopeActivateResult(app.editorFindScope,xlIndex)!=XUI_OK);
    CHECK(xideEditorShow(&app,c)==XUI_OK && settle(&app) && key(&app,'Z',XUI_MOD_CTRL) && !c->dirty && !strcmp(xuiCodeEditGetText(c->editor),cText));
    CHECK(dismiss(&app,c));
    CHECK(xideWorkbenchApplyTheme(&app,IDE_THEME_GRAPHITE)==XUI_OK && openSearch(&app,c,1) && field(&app,c,0,"中😀") && field(&app,c,1,"结果😀"));
    CHECK(button(&app,c,XUI_TR_REPLACE_ALL) && c->dirty && !xl->dirty && !strcmp(xuiCodeEditGetText(xl->editor),xlText));
    CHECK(!strcmp(xuiCodeEditGetText(c->editor),"// 结果😀 target 结果😀\nint alpha(void) { return 1; } // alpha2\n") && xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(c->editor)) && capture(&app,images[1]));
    CHECK(dismiss(&app,c) && xideEditorShow(&app,c)==XUI_OK && settle(&app) && key(&app,'Z',XUI_MOD_CTRL) && !c->dirty && !strcmp(xuiCodeEditGetText(c->editor),cText));
    CHECK(key(&app,'Y',XUI_MOD_CTRL) && c->dirty && key(&app,'Z',XUI_MOD_CTRL) && !c->dirty);
    CHECK(xuiCodeEditSetReadonly(c->editor,1)==XUI_OK && settle(&app) && xideActionQuery(&app,XIDE_ACTION_EDITOR_FIND).enabled && !xideActionQuery(&app,XIDE_ACTION_EDITOR_REPLACE).enabled);
    cv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor));CHECK(key(&app,'H',XUI_MOD_CTRL) && !xuiWidgetGetVisible(xuiCodeEditGetFindWindow(c->editor)) && cv==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)));
    CHECK(xuiCodeEditSetReadonly(c->editor,0)==XUI_OK && openSearch(&app,xl,0) && field(&app,xl,0,"变量") && button(&app,xl,XUI_TR_FIND_ALL));
    CHECK(xuiCodeEditGetFindResultCount(xl->editor)==2 && capture(&app,images[2]));
    xui_find_options_t options={0};options.iSize=sizeof(options);options.sPattern="alpha";options.iFlags=XUI_FIND_CASE_SENSITIVE|XUI_FIND_WHOLE_WORD;options.iStartOffset=-1;options.iRangeEnd=-1;int count=0;
    CHECK(xuiCodeFindScopeFindAll(app.editorFindScope,&options,&count)==XUI_OK && count==1);
    CHECK(xuiCodeFindScopeGetResult(app.editorFindScope,0,&match)==XUI_OK && match.pEditor==c->editor && !memcmp(cText+match.iStart,"alpha",5));
    CHECK(xideEditorShow(&app,c)==XUI_OK && settle(&app) && xideActionExecute(&app,XIDE_ACTION_LANGUAGE_XLANG)==XIDE_ACTION_DISPATCHED && settle(&app) && xuiCodeFindScopeGetResultCount(app.editorFindScope)==1);
    CHECK(xideActionExecute(&app,XIDE_ACTION_LANGUAGE_C)==XIDE_ACTION_DISPATCHED && settle(&app) && !c->dirty && !strcmp(xuiCodeEditGetText(c->editor),cText));
    CHECK(xideDocumentSave(c,savedAs,0,&error) && settle(&app));
    CHECK(xuiCodeFindScopeGetResultCount(app.editorFindScope)==1 && !strcmp(xuiCodeFindScopeGetResultEditorName(app.editorFindScope,0),savedAs));
    CHECK(openSearch(&app,c,0) && field(&app,c,0,"alpha") && button(&app,c,XUI_TR_FIND_ALL) && capture(&app,images[3]));
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"!")==XUI_OK && settle(&app));
    CHECK(!xideEditorClose(&app,c,0,&error) && c->open && xuiCodeFindScopeGetEditorCount(app.editorFindScope)==2);
    CHECK(xideEditorClose(&app,c,1,&error) && settle(&app) && xuiCodeFindScopeGetEditorCount(app.editorFindScope)==1 && !xuiCodeEditGetFindScope(c->editor) && !xuiWidgetGetVisible(xuiCodeEditGetFindWindow(c->editor)));
    uint64_t previous=xl->id;CHECK(xideDocumentClose(xl,1,&error) && app.activeDocument==NULL && xuiCodeFindScopeGetEditorCount(app.editorFindScope)==0);
    xide_document* plain=xideDocumentNew(&app.documents,"text",&error);CHECK(plain && plain->id!=previous && xideEditorShow(&app,plain)==XUI_OK && settle(&app));
    CHECK(xuiCodeEditSetText(plain->editor,"plain 中😀\n")==XUI_OK && settle(&app) && xuiCodeFindScopeGetEditorCount(app.editorFindScope)==1);
    options.sPattern="中😀";options.iFlags=0;CHECK(xuiCodeFindScopeFindAll(app.editorFindScope,&options,&count)==XUI_OK && count==1 && xuiCodeFindScopeGetResult(app.editorFindScope,0,&match)==XUI_OK && match.pEditor==plain->editor);
    CHECK(xideWorkbenchReset(&app)==XUI_OK && settle(&app) && xuiCodeFindScopeGetResultCount(app.editorFindScope)==1 && xuiCodeFindScopeActivateResult(app.editorFindScope,0)==XUI_OK && app.activeDocument==plain);
    CHECK(sameFile(cPath,cText) && sameFile(xlPath,xlText) && sameFile(savedAs,cText));
    printf("find_view_test passed: %d native assertions; C/xlang/open-document search, actual table activation and Dock focus, Unicode replacement and single undo/redo, readonly actions, whole-word, language override, Save As names, stale-result removal, dirty-close refusal, direct close, slot reuse, untitled/plain and reset; four native images; source files preserved\n",f->checks);
cleanup:
    if(began) { xgeEnd(); }
    xideAppDestroy(&app);free(root);free(cPath);free(xlPath);free(settings);free(savedAs);for(int i=0;i<4;++i) free(images[i]);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data;if(!SetThreadDesktop(f->desktop)) return 1;xge_desc_t d={0};d.iWidth=1280;d.iHeight=800;d.sTitle="xlang IDE native find verification";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;int ret=xgeInit(&d);if(ret==XGE_OK) ret=xgeRun(verify,f);xgeUnit();return ret==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2;fixture f={.directory=argv[1]};wchar_t name[100];swprintf(name,100,L"XlangFindVerification-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop) return 1;HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(t) { if(WaitForSingleObject(t,90000)!=WAIT_OBJECT_0) { fputs("find_view_test timed out\n",stderr);ExitProcess(1); }GetExitCodeThread(t,&code);CloseHandle(t); }CloseDesktop(f.desktop);return (int)code; }
