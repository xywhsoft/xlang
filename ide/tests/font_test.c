#include "app/xide_app.h"
#include "services/editor_font.h"
#include "ui/font_view.h"
#include "ui/editor_view.h"
#include "ui/catalog_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"font_test:%d: %s; %s\n",__LINE__,#e,error.message); failed=1; goto cleanup; } } while(0)
static int started,verificationFailed;
static xstrview sv(const char* text) { return (xstrview){text,strlen(text)}; }
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,strlen(text)}); }
static int settle(xide_app* app)
{
    if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0;
    xideEditorPoll(app); xideWorkbenchLayout(app); if(xuiLayout(app->context)!=XUI_OK) return 0;
    xideCatalogViewLayout(app); return xuiLayout(app->context)==XUI_OK && !app->frameError;
}
static int click(xide_app* app,int control)
{
    if(!settle(app)) return 0;
    xui_rect_t rect=xuiWidgetGetWorldRect(xideFontViewControl(app,control)); int x=rect.fX+rect.fW/2,y=rect.fY+rect.fH/2;
    return xuiInputPointerMove(app->context,x,y,0)==XUI_OK && xuiInputPointerDown(app->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK &&
        xuiInputPointerUp(app->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(app);
}
static xui_widget listIn(xide_app* app,xui_widget widget)
{
    if(!widget) return NULL;
    if(xuiWidgetGetType(widget)==xuiListViewGetType(app->context)) return widget;
    for(xui_widget child=xuiWidgetGetFirstChild(widget);child;child=xuiWidgetGetNextSibling(child)) { xui_widget found=listIn(app,child); if(found) return found; }
    return NULL;
}
static int capture(xide_app* app,const char* path)
{
    xui_rect_i_t rect={0,0,app->viewportWidth,app->viewportHeight}; size_t bytes=(size_t)rect.iW*(size_t)rect.iH*4; unsigned char* pixels=malloc(bytes); if(!pixels) return 0;
    int ret=xuiUpdate(app->context,0); if(ret==XUI_OK) ret=app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu);
    if(ret==XUI_OK) ret=xuiRender(app->context,app->target,&rect,1);
    if(ret==XUI_OK) ret=app->proxy.surfaceReadRGBA(&app->proxy,app->target,pixels,rect.iW*4);
    if(ret==XUI_OK) ret=xgeImageSavePNG(path,rect.iW,rect.iH,pixels,rect.iW*4);
    free(pixels); return ret==XUI_OK;
}
static int verify(void* user)
{
    const char* directory=user; started=1; xide_app app={0}; xide_model_error error={0}; xide_settings loaded={0}; int failed=0,began=0;
    char *root=NULL,*settings=NULL,*font=NULL,*windowsFont=NULL,*badFont=NULL,*image=NULL,*compiler=NULL; unsigned char* bytes=NULL;
    root=xidePathResolve(directory,"中文 字体"); CHECK(root && xrtDirCreateAll(root)); settings=xidePathResolve(root,"settings.json");
    font=xidePathResolve(root,"等宽 字体.ttf"); windowsFont=xideWindowsFontPath("consola.ttf"); badFont=xidePathResolve(root,"invalid.ttf"); image=xidePathResolve(root,"native-font-settings.png"); compiler=xidePathResolve(root,"compiler.exe");
    CHECK(settings && font && windowsFont && badFont && image && compiler);
    size_t size=0; bytes=xrtFileReadAllLimit(windowsFont,16u*1024u*1024u,&size); CHECK(bytes && xrtFileWriteAtomic(font,(xbytesview){bytes,size})); xrtFree(bytes); bytes=NULL;
    CHECK(writeText(badFont,"invalid font") && writeText(compiler,"compiler path fixture"));
    CHECK(writeText(settings,"{\"format\":\"xlang-ide-settings-1\",\"future\":7,\"editor\":{\"futureEditor\":true,\"font\":{\"futureFont\":9}}}"));
    app.hidden=1; app.noSession=1; app.maxFrames=1; app.settingsPath=settings; CHECK(xideAppCreate(&app)==XUI_OK);
    CHECK(xgeBegin()==XGE_OK); began=1; CHECK(settle(&app));
    CHECK(xideSettingsSetCompiler(&app.preferences,compiler,&error));
    xide_document* c=xideDocumentNew(&app.documents,"c",&error); CHECK(c && xideEditorShow(&app,c)==XUI_OK && settle(&app));
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"ret")==XUI_OK && settle(&app));
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(c->editor),xuiCodeEditGetDocument(c->editor),3,0)==XUI_OK);
    CHECK(xuiCodeEditShowCompletion(c->editor)==XUI_OK && settle(&app));
    xui_widget completion=listIn(&app,xuiCodeEditGetCompletionPopupWidget(c->editor)); CHECK(completion && xuiCodeEditIsCompletionOpen(c->editor));
    CHECK(xuiCodeEditCancelCompletion(c->editor)==XUI_OK);
    char longText[3000]={0}; for(int i=0;i<150;++i) strcat(longText,"// scroll font\n");
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),3,longText)==XUI_OK);
    xide_document* xl=xideDocumentNew(&app.documents,"xlang",&error); CHECK(xl && xideEditorShow(&app,xl)==XUI_OK && settle(&app));
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(xl->editor),0,"int 中文 = 1 // 😀\n")==XUI_OK && settle(&app));
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(xl->editor),xuiCodeEditGetDocument(xl->editor),xuiCodeDocumentGetLength(xuiCodeEditGetDocument(xl->editor)),0)==XUI_OK);
    xui_code_selection_t before={0},after={0}; CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(xl->editor),&before)==XUI_OK);
    uint32_t cVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)),xlVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor));
    xide_document* closed=xideDocumentNew(&app.documents,"c",&error); CHECK(closed && xideEditorShow(&app,closed)==XUI_OK && xideEditorClose(&app,closed,1,&error));
    CHECK(xideEditorShow(&app,xl)==XUI_OK && settle(&app));
    const xide_command* helpCommand=xideCatalogFind(&app.catalog,"language:func"); CHECK(helpCommand);
    xideWorkbenchShowHelp(&app,(int)(helpCommand-app.catalog.entries)); CHECK(settle(&app));
    int oldHelpHeight=xideHelpLayout(xideCatalogHelpDocument(&app),400);
    xui_font original=app.codeFont; CHECK(xideFontViewOpen(&app) && settle(&app));
    size_t count=app.documents.count; xideDispatchAction(&app,XIDE_ACTION_NEW_FILE); CHECK(app.documents.count==count);
    CHECK(xuiEditSetText(xideFontViewControl(&app,XIDE_FONT_PATH),font)==XUI_OK && xuiEditSetText(xideFontViewControl(&app,XIDE_FONT_SIZE),"24")==XUI_OK);
    CHECK(click(&app,XIDE_FONT_CANCEL) && !xideFontViewIsOpen(&app) && app.codeFont==original && app.codeFontSize==16);
    CHECK(xideFontViewOpen(&app) && xuiEditSetText(xideFontViewControl(&app,XIDE_FONT_PATH),badFont)==XUI_OK && click(&app,XIDE_FONT_APPLY));
    CHECK(xideFontViewIsOpen(&app) && app.codeFont==original);
    CHECK(xuiEditSetText(xideFontViewControl(&app,XIDE_FONT_PATH),font)==XUI_OK && xuiEditSetText(xideFontViewControl(&app,XIDE_FONT_SIZE),"41")==XUI_OK && click(&app,XIDE_FONT_APPLY)); CHECK(app.codeFont==original);
    CHECK(xuiEditSetText(xideFontViewControl(&app,XIDE_FONT_SIZE),"24")==XUI_OK && click(&app,XIDE_FONT_APPLY) && !xideFontViewIsOpen(&app));
    CHECK(xideHelpLayout(xideCatalogHelpDocument(&app),400)>oldHelpHeight);
    CHECK(app.codeFont!=original && app.codeFontSize==24 && xidePathEqual(app.codeFontPath,font));
    CHECK(xuiCodeEditGetFont(c->editor)==app.codeFont && xuiCodeEditGetFont(xl->editor)==app.codeFont && xuiCodeEditGetFont(closed->editor)==app.codeFont && xuiListViewGetFont(completion)==app.codeFont);
    CHECK(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor))==cVersion && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor))==xlVersion && c->dirty && xl->dirty);
    CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(xl->editor),&after)==XUI_OK && before.iCaretOffset==after.iCaretOffset && before.iAnchorOffset==after.iAnchorOffset);
    CHECK(xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(xl->editor)) && !strcmp(xuiCodeEditGetLanguage(c->editor),"c") && !strcmp(xuiCodeEditGetLanguage(xl->editor),"xlang"));
    CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(xl->editor))==XUI_OK && !strcmp(xuiCodeEditGetText(xl->editor),""));
    CHECK(xideSettingsLoad(&loaded,settings,&error) && loaded.editorFontSize==24 && xidePathEqual(loaded.editorFont,font) && xidePathEqual(loaded.compiler,compiler));
    CHECK(xrtValueObjectGet(loaded.source,sv("future")) && xrtValueObjectGet(xrtValueObjectGet(loaded.source,sv("editor")),sv("futureEditor")));
    float cellWidth=0,cellHeight=0,padding=0; xui_vec2_t measured={0}; CHECK(xuiTerminalGetMetrics(app.panels[XIDE_TERMINAL],&cellWidth,&cellHeight,&padding)==XUI_OK && xideDpiTextMeasure(&app.proxy,app.codeFont," ",&measured)==XUI_OK);
    CHECK(fabsf(cellWidth-roundf(measured.fX))<0.01f && fabsf(cellHeight-roundf(xideEditorLineHeight(&app)))<0.01f && xuiCodeEditGetFont(app.panels[XIDE_OUTPUT])==app.codeFont);
    xide_document* fresh=xideDocumentNew(&app.documents,"xlang",&error); CHECK(fresh && xuiCodeEditGetFont(fresh->editor)==app.codeFont);
    xideWorkbenchShowHelp(&app,0); CHECK(settle(&app) && xuiCodeEditGetFont(app.helpExample)==app.codeFont);
    CHECK(xideFontViewOpen(&app) && click(&app,XIDE_FONT_RESET) && !strcmp(xuiEditGetText(xideFontViewControl(&app,XIDE_FONT_SIZE)),"16") && app.codeFontSize==24);
    CHECK(click(&app,XIDE_FONT_CANCEL) && xideFontViewOpen(&app) && settle(&app) && capture(&app,image)); CHECK(click(&app,XIDE_FONT_CANCEL));
    CHECK(xideEditorShow(&app,c)==XUI_OK && settle(&app) && xuiCodeEditSetScroll(c->editor,0,100000)==XUI_OK);
    float oldX=0,oldY=0,newX=0,newY=0; CHECK(xuiCodeEditGetScroll(c->editor,&oldX,&oldY)==XUI_OK && oldY>1000);
    CHECK(writeText(settings,"{\"format\":\"xlang-ide-settings-1\",\"external\":true}")); original=app.codeFont;
    CHECK(!xideEditorFontApply(&app,font,20,&error) && error.code==XIDE_MODEL_CONFLICT && app.codeFont==original && app.codeFontSize==24 && xuiCodeEditGetFont(c->editor)==original && !app.frameError);
    CHECK(xuiCodeEditGetScroll(c->editor,&newX,&newY)==XUI_OK && fabsf(oldX-newX)<0.01f && fabsf(oldY-newY)<0.01f);
    /* Restore an owned baseline; then verify startup loads the saved native font. */
    CHECK(xideSettingsLoad(&app.preferences,settings,&error) && xideEditorFontApply(&app,font,24,&error));
    CHECK(xgeEnd()==XGE_OK); began=0; xideAppDestroy(&app); memset(&app,0,sizeof(app)); app.hidden=1; app.noSession=1; app.maxFrames=1; app.settingsPath=settings;
    CHECK(xideAppCreate(&app)==XUI_OK && app.codeFontSize==24 && xidePathEqual(app.codeFontPath,font));
    CHECK(xgeBegin()==XGE_OK); began=1; CHECK(settle(&app));
    xui_font_metrics_t metrics={0}; CHECK(app.proxy.fontGetMetrics(&app.proxy,app.codeFont,&metrics)==XUI_OK && fabsf(metrics.fSize-24)<0.01f);
    puts("font_test passed: Unicode real fonts, native modal apply/cancel/reset boundaries, malformed/size/conflict and scroll rollback, C/xlang versions/selection/undo/languages, closed/new documents, pooled completion fonts, help/output/terminal metrics and restart preferences.");
cleanup:
    if(began) xgeEnd();
    xideAppDestroy(&app); xideSettingsClear(&loaded); xgeSetQuitRequestCallback(NULL,NULL);
    xrtFree(bytes); free(root); free(settings); free(font); free(windowsFont); free(badFont); free(image); free(compiler);
    verificationFailed=failed; xgeQuit(); return XGE_OK;
}
int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    xge_desc_t desc={0}; desc.iWidth=1280; desc.iHeight=800; desc.sTitle="xlang IDE font verification"; desc.iFlags=XGE_INIT_WINDOW; desc.iRunMode=XGE_RUN_GAME_LOOP;
    int ret=xgeInit(&desc); if(ret==XGE_OK) ret=xgeRun(verify,argv[1]); xgeUnit(); return ret==XGE_OK && started && !verificationFailed ? 0 : 1;
}
