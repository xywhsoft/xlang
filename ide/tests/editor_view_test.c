#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "platform/path.h"
#include "theme/theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(expression) do { if(!(expression)) { fprintf(stderr,"editor_view_test:%d: %s; %s\n",__LINE__,#expression,error.message); failed=1; goto cleanup; } } while(0)
static int settle(xide_app* app)
{
    if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0;
    xideEditorPoll(app); xideWorkbenchLayout(app);
    return xuiLayout(app->context)==XUI_OK;
}
static int statusIs(xide_app* app,const char* expected)
{
    xideEditorPoll(app);
    const xui_statusbar_item_t* item=xuiStatusBarGetItem(app->statusBar,2);
    if(item && item->sText && !strcmp(item->sText,expected)) return 1;
    fprintf(stderr,"Status expected '%s', got '%s'.\n",expected,item && item->sText ? item->sText : "(missing)");
    return 0;
}
static int clickClose(xide_app* app,xide_document* document)
{
    xui_dock_window_info_t info={0}; info.iSize=sizeof(info);
    xui_dock_pane_info_t pane={0}; pane.iSize=sizeof(pane);
    if(xuiDockPanelGetWindowInfo(app->dock,document->window,&info)!=XUI_OK || xuiDockPanelGetPaneInfo(app->dock,info.iPane,&pane)!=XUI_OK) return 0;
    float y=pane.tTabStripRect.fY+pane.tTabStripRect.fH/2,x=-1;
    for(float probe=pane.tTabStripRect.fX;probe<pane.tTabStripRect.fX+pane.tTabStripRect.fW;probe+=1) {
        xui_dock_hit_t hit={0}; hit.iSize=sizeof(hit);
        if(xuiDockPanelHitTest(app->dock,probe,y,&hit)==XUI_OK && hit.iWindow==document->window &&
           (hit.iType==XUI_DOCK_PANEL_HIT_PANE_TAB_CLOSE || hit.iType==XUI_DOCK_PANEL_HIT_PANE_CLOSE)) { x=hit.tRect.fX+hit.tRect.fW/2; break; }
    }
    if(x<0) { fprintf(stderr,"No real dock close hit for window %d.\n",document->window); return 0; }
    xui_rect_t dock=xuiWidgetGetWorldRect(app->dock); x+=dock.fX; y+=dock.fY;
    if(xuiInputPointerMove(app->context,x,y,0)!=XUI_OK || xuiInputPointerDown(app->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)!=XUI_OK ||
       xuiInputPointerUp(app->context,x,y,XUI_POINTER_BUTTON_LEFT,0)!=XUI_OK) return 0;
    return settle(app);
}
int main(int argc,char** argv)
{
    xide_app app={0}; xide_model_error error={0}; int failed=0; char *root=NULL,*a=NULL,*b=NULL;
    CHECK(argc==2); root=xidePathResolve(argv[1],"中文 原生编辑"); CHECK(root && xrtDirCreateAll(root));
    a=xidePathResolve(root,"main.xl"); b=xidePathResolve(root,"helper.c"); CHECK(a && b);
    const char* original="func main(...) as int\n    return 0\nend\n";
    CHECK(xrtFileWriteAtomic(a,(xbytesview){(unsigned char*)original,strlen(original)}));
    CHECK(xrtFileWriteAtomic(b,(xbytesview){(unsigned char*)"int helper = 42;\n",17}));
    app.hidden=1; app.viewportWidth=1280; app.viewportHeight=800;
    CHECK(xuiCreate(&app.context)==XUI_OK && ideThemeInit(app.context,IDE_THEME_CLASSIC)==XUI_OK && xuiInputViewport(app.context,1280,800)==XUI_OK);
    CHECK(xideWorkbenchCreate(&app)==XUI_OK && settle(&app));
    CHECK(xideEditorOpen(&app,a,&error) && settle(&app));
    xide_document* first=app.activeDocument; CHECK(first && first->open && xuiGetFocusWidget(app.context)==first->editor && !xuiCodeEditIsReadonly(first->editor));
    xui_code_document core=xuiCodeEditGetDocument(first->editor); CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(first->editor),core,xuiCodeDocumentGetLength(core),0)==XUI_OK);
    CHECK(xuiInputTextEx(app.context,'/',NULL)==XUI_OK && xuiInputTextEx(app.context,'/',NULL)==XUI_OK && xuiInputTextEx(app.context,0x4e2d,NULL)==XUI_OK && xuiInputTextEx(app.context,0x1f600,NULL)==XUI_OK && settle(&app));
    CHECK(xuiStatusBarGetItem(app.statusBar,2) && strstr(xuiStatusBarGetItem(app.statusBar,2)->sText,"xlang") && strstr(xuiStatusBarGetItem(app.statusBar,2)->sText,"UTF-8"));
    CHECK(statusIs(&app,"xlang · UTF-8 · LF · 4:5"));
    CHECK(first->dirty && strstr(xuiCodeEditGetText(first->editor),"//中😀") && strstr(xuiDockPanelGetWindowTitle(app.dock,first->window),"*"));
    uint32_t tokenVersion=0; CHECK(xuiCodeTokenBufferGetVersion(xuiCodeEditGetTokenBuffer(first->editor),&tokenVersion)==XUI_OK && tokenVersion==xuiCodeDocumentGetVersion(core));
    CHECK(xideEditorOpen(&app,b,&error) && settle(&app)); xide_document* second=app.activeDocument;
    CHECK(statusIs(&app,"c · UTF-8 · LF · 1:1"));
    CHECK(second!=first && !strcmp(second->language,"c") && xideEditorOpen(&app,a,&error) && settle(&app));
    CHECK(app.activeDocument==first && app.documents.count==2 && strstr(xuiCodeEditGetText(first->editor),"//中😀"));
    CHECK(statusIs(&app,"xlang · UTF-8 · LF · 4:5"));
    uint32_t version=xuiCodeDocumentGetVersion(core); xideDispatchAction(&app,XIDE_ACTION_LANGUAGE_C); CHECK(settle(&app));
    CHECK(!strcmp(first->language,"c") && xuiCodeDocumentGetVersion(core)==version && first->dirty && xuiCodeDocumentCanUndo(core));
    CHECK(statusIs(&app,"c · UTF-8 · LF · 4:5"));
    xideDispatchAction(&app,XIDE_ACTION_LANGUAGE_XLANG); CHECK(settle(&app));
    for(int i=0;i<4;++i) { xideDispatchAction(&app,XIDE_ACTION_EDITOR_UNDO); CHECK(settle(&app)); }
    CHECK(!first->dirty && !strcmp(xuiCodeEditGetText(first->editor),original));
    CHECK(statusIs(&app,"xlang · UTF-8 · LF · 4:1"));
    xideDispatchAction(&app,XIDE_ACTION_EDITOR_REDO); CHECK(settle(&app) && first->dirty);
    CHECK(statusIs(&app,"xlang · UTF-8 · LF · 4:2"));
    CHECK(!xideWorkspaceQuitRequest(&app));
    CHECK(clickClose(&app,first));
    xui_dock_window_info_t info={0}; info.iSize=sizeof(info);
    CHECK(first->open && first->dirty && xuiDockPanelGetWindowInfo(app.dock,first->window,&info)==XUI_OK && info.iState==XUI_DOCK_PANEL_WINDOW_DOCKED);
    CHECK(xideDocumentSave(first,NULL,0,&error) && !first->dirty && settle(&app));
    xideDispatchAction(&app,XIDE_ACTION_EDITOR_FIND); CHECK(settle(&app));
    CHECK(xuiCodeEditGetFindWindow(first->editor)!=NULL && xuiWidgetGetVisible(xuiCodeEditGetFindWindow(first->editor)));
    xuiInputKeyDownEx(app.context,XUI_KEY_ESCAPE,0,NULL); xuiInputKeyUpEx(app.context,XUI_KEY_ESCAPE,0,NULL); CHECK(settle(&app));
    CHECK(!xuiWidgetGetVisible(xuiCodeEditGetFindWindow(first->editor)));
    CHECK(xideWorkbenchReset(&app)==XUI_OK && settle(&app));
    CHECK(app.activeDocument==first && first->open && second->open && !strcmp(first->language,"xlang") && xuiCodeDocumentCanUndo(core));
    xui_dock_window_info_t firstInfo={0},helpInfo={0}; firstInfo.iSize=helpInfo.iSize=sizeof(firstInfo);
    CHECK(xuiDockPanelGetWindowInfo(app.dock,first->window,&firstInfo)==XUI_OK && xuiDockPanelGetWindowInfo(app.dock,app.windows[XIDE_HELP],&helpInfo)==XUI_OK);
    xui_dock_pane_info_t editorPane={0},helpPane={0}; editorPane.iSize=helpPane.iSize=sizeof(editorPane);
    CHECK(xuiDockPanelGetPaneInfo(app.dock,firstInfo.iPane,&editorPane)==XUI_OK && xuiDockPanelGetPaneInfo(app.dock,helpInfo.iPane,&helpPane)==XUI_OK);
    CHECK(editorPane.tRect.fX==helpPane.tRect.fX && editorPane.tRect.fW==helpPane.tRect.fW && app.activeDocument==first);
    CHECK(clickClose(&app,first) && !first->open && second->open && app.activeDocument==second);
    int windows=xuiDockPanelGetWindowCount(app.dock);
    xideDispatchAction(&app,XIDE_ACTION_NEW_FILE); CHECK(settle(&app));
    CHECK(app.activeDocument==first && first->open && !first->path && !xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(first->editor)) && xuiDockPanelGetWindowCount(app.dock)==windows);
    CHECK(statusIs(&app,"xlang · UTF-8 · LF · 1:1"));
    core=xuiCodeEditGetDocument(first->editor);
    const char* unicode="中é👨‍👩‍👧‍👦\tZ\r\n第二行\r\n";
    CHECK(xuiCodeEditSetText(first->editor,unicode)==XUI_OK);
    int afterFirst=(int)(strstr(unicode,"\r\n")-unicode);
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(first->editor),core,afterFirst,0)==XUI_OK && settle(&app));
    CHECK(statusIs(&app,"xlang · UTF-8 · LF · 1:6"));
    for(int i=0;i<16;++i) CHECK(statusIs(&app,"xlang · UTF-8 · LF · 1:6"));
    version=xuiCodeDocumentGetVersion(core);
    first->baseline.bom=1; first->baseline.eol=XUI_CODE_EOL_CRLF;
    CHECK(statusIs(&app,"xlang · UTF-8 BOM · CRLF · 1:6") && xuiCodeDocumentGetVersion(core)==version);
    CHECK(xideDocumentSetLanguage(first,"c",&error) && statusIs(&app,"c · UTF-8 BOM · CRLF · 1:6") && xuiCodeDocumentGetVersion(core)==version);
    CHECK(xuiCodeDocumentReplace(core,0,3,"abc")==XUI_OK);
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(first->editor),core,afterFirst,0)==XUI_OK && settle(&app));
    CHECK(statusIs(&app,"c · UTF-8 BOM · CRLF · 1:8") && xuiCodeDocumentGetVersion(core)!=version);
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(first->editor),core,xuiCodeDocumentGetLength(core),0)==XUI_OK && settle(&app));
    CHECK(statusIs(&app,"c · UTF-8 BOM · CRLF · 3:1"));
    CHECK(xideEditorClose(&app,first,1,&error) && xideEditorClose(&app,second,0,&error) && settle(&app));
    CHECK(!app.activeDocument && xideWorkspaceQuitRequest(&app));
    CHECK(statusIs(&app,"C / xlang · UTF-8"));
    puts("editor_view_test passed: real Unicode keyboard input, live syntax cache, independent C/xlang tabs, undo/redo, Save, find, dirty-close cancellation, slot reuse, current grapheme status and reset preserving center-only bottom");
cleanup:
    xideAppDestroy(&app); free(root); free(a); free(b); return failed;
}
