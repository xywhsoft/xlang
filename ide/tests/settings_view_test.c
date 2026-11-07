#include "app/xide_app.h"
#include "ui/settings_view.h"
#include "ui/editor_view.h"
#include "ui/catalog_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do{if(!(e)){fprintf(stderr,"settings_view_test:%d: %s; %s\n",__LINE__,#e,xideSettingsViewMessage(&a));failed=1;goto cleanup;}}while(0)
static int started,failed;
static int settle(xide_app* a)
{xideWorkbenchLayout(a);return xuiDispatchPendingEvents(a->context)==XUI_OK && xuiLayout(a->context)==XUI_OK && !a->frameError;}
static int click(xide_app* a,int id)
{
 if(!settle(a))return 0;
 xui_rect_t r=xuiWidgetGetWorldRect(xideSettingsViewControl(a,id));int x=r.fX+r.fW/2,y=r.fY+r.fH/2;
 return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a);
}
static int scrollMode(xide_app* a,xide_document* document,int enabled)
{
 if(xideEditorShow(a,document)!=XUI_OK || !settle(a))return 0;
 int visible=-1;float width=0;
 if(xuiCodeEditGetMinimap(document->editor,&visible,&width)!=XUI_OK || visible!=enabled)return 0;
 xui_rect_t map=xuiCodeEditGetMinimapRect(document->editor);
 xui_widget bar=xuiCodeEditGetVScrollBarWidget(document->editor);
 return bar && (enabled ? map.fW>0 && !xuiWidgetGetVisible(bar) : map.fW==0 && xuiWidgetGetVisible(bar));
}
static int run(void* user)
{
 const char* directory=user;xide_app a={0};xide_settings loaded={0};xide_model_error error={0};char *settings=NULL,*image=NULL;unsigned char* before=NULL;size_t bytes=0;int began=0;started=1;
 settings=xidePathResolve(directory,"settings-center.json");image=xidePathResolve(directory,"settings-center.png");CHECK(settings && image);
 const char initial[]="{\"format\":\"xlang-ide-settings-1\",\"future\":7,\"documents\":{\"futureDocument\":11},\"editor\":{\"font\":{\"futureFont\":9}}}";
 CHECK(xrtFileWriteAtomic(settings,(xbytesview){(const unsigned char*)initial,sizeof(initial)-1}));
 a.hidden=1;a.noSession=1;a.settingsPath=settings;a.disableAutomaticDiagnostics=1;
 CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(settle(&a));
 int found=0;for(int i=0;i<xuiMenuBarGetItemCount(a.menuBar);++i){const xui_menubar_item_t* item=xuiMenuBarGetItem(a.menuBar,i);if(item && !strcmp(item->sText,"设置"))++found;CHECK(!item || strcmp(item->sText,"编辑器"));}CHECK(found==1);
 xide_document* c=xideDocumentNew(&a.documents,"c",&error);CHECK(c && xideEditorShow(&a,c)==XUI_OK);
 CHECK(xuiCodeEditSetText(c->editor,"int main(void) { return 0; }\n")==XUI_OK);
 xide_document* xl=xideDocumentNew(&a.documents,"xlang",&error);CHECK(xl && xideEditorShow(&a,xl)==XUI_OK);
 CHECK(xuiCodeEditSetText(xl->editor,"func main() as int\n    return 0\nend\n")==XUI_OK);
 uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor));
 CHECK(xuiSetFocusWidget(a.context,xl->editor)==XUI_OK);
 CHECK(xuiInputKeyDownEx(a.context,188,XUI_MOD_CTRL,NULL)==XUI_OK && xuiInputKeyUpEx(a.context,188,XUI_MOD_CTRL,NULL)==XUI_OK && settle(&a));
 CHECK(xideSettingsViewIsOpen(&a));
 CHECK(xuiEditSetText(xideSettingsViewControl(&a,XIDE_PREF_SEARCH),"字体")==XUI_OK && settle(&a));
 CHECK(xuiWidgetGetVisible(xideSettingsViewControl(&a,XIDE_PREF_DOC_PATH)) && !xuiWidgetGetVisible(xideSettingsViewControl(&a,XIDE_PREF_TAB)));
 CHECK(xuiEditSetText(xideSettingsViewControl(&a,XIDE_PREF_SEARCH),"")==XUI_OK);
 CHECK(xuiEditSetText(xideSettingsViewControl(&a,XIDE_PREF_CODE_SIZE),"18")==XUI_OK);
 CHECK(xuiEditSetText(xideSettingsViewControl(&a,XIDE_PREF_DOC_SIZE),"20")==XUI_OK);
 CHECK(xuiEditSetText(xideSettingsViewControl(&a,XIDE_PREF_TAB),"8")==XUI_OK);
 CHECK(xuiEditSetText(xideSettingsViewControl(&a,XIDE_PREF_INDENT),"2")==XUI_OK);
 xui_widget minimap=xideSettingsViewControl(&a,XIDE_PREF_MINIMAP),scroll=xuiWidgetGetParent(minimap);
 while(scroll && xuiWidgetGetType(scroll)!=xuiWidgetFindType(a.context,"scrollview"))scroll=xuiWidgetGetParent(scroll);
 CHECK(scroll && xuiScrollViewEnsureChildVisible(scroll,minimap)==XUI_OK && settle(&a));
 CHECK(click(&a,XIDE_PREF_MINIMAP) && !xuiCheckBoxGetChecked(minimap));
 CHECK(xuiCheckBoxSetChecked(xideSettingsViewControl(&a,XIDE_PREF_AUTO),0)==XUI_OK);
 CHECK(xuiCheckBoxSetChecked(xideSettingsViewControl(&a,XIDE_PREF_COMPLETION),0)==XUI_OK);
 CHECK(xuiCheckBoxSetChecked(xideSettingsViewControl(&a,XIDE_PREF_SIGNATURE),0)==XUI_OK);
 CHECK(xuiCheckBoxSetChecked(xideSettingsViewControl(&a,XIDE_PREF_WRAP),1)==XUI_OK);
 CHECK(xuiComboBoxSetSelected(xideSettingsViewControl(&a,XIDE_PREF_THEME),2)==XUI_OK);
 CHECK(xideSettingsViewApply(&a) && settle(&a));
 CHECK(a.codeFontSize==18 && a.documentFontSize==20 && a.uiFontSize==16 && a.theme==2);
 CHECK(a.documentFont!=a.uiFont && a.documentFont!=a.codeFont);
 CHECK(xuiLabelGetFont(a.helpDescription)==a.documentFont && xuiLabelGetFont(a.helpSignature)==a.codeFont);
 CHECK(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor))==version);
 CHECK(xuiCodeEditGetTabColumns(xl->editor)==8 && xuiCodeEditGetIndentColumns(c->editor)==2 && xuiCodeEditGetWordWrap(c->editor));
 int autoShow=1,minPrefix=0,maxItems=0;float completionDelay=0;CHECK(xuiCodeEditGetCompletionOptions(c->editor,&autoShow,&minPrefix,&maxItems,&completionDelay)==XUI_OK && !autoShow);
 int visible=1;float width=0;CHECK(xuiCodeEditGetMinimap(xl->editor,&visible,&width)==XUI_OK && !visible);
 CHECK(xideSettingsLoad(&loaded,settings,&error) && loaded.documentFontSize==20 && xideSettingsOptions(&loaded)->theme==2);
 CHECK(xrtValueObjectGet(loaded.source,(xstrview){"future",6}));
 before=xrtFileReadAllLimit(settings,256*1024,&bytes);CHECK(before);
 xui_font original=a.documentFont;
 CHECK(xuiEditSetText(xideSettingsViewControl(&a,XIDE_PREF_DOC_PATH),"missing-settings-font.ttf")==XUI_OK);
 CHECK(!xideSettingsViewApply(&a) && a.documentFont==original && a.documentFontSize==20 && !a.frameError);
 CHECK(click(&a,XIDE_PREF_CANCEL) && !xideSettingsViewIsOpen(&a));
 CHECK(xideSettingsViewOpen(&a,XIDE_SETTINGS_DOCUMENTS));
 CHECK(xuiEditSetText(xideSettingsViewControl(&a,XIDE_PREF_DOC_SIZE),"24")==XUI_OK && click(&a,XIDE_PREF_CANCEL));
 CHECK(a.documentFontSize==20);
 CHECK(xideSettingsViewOpen(&a,XIDE_SETTINGS_DOCUMENTS));
 const char external[]="{\"format\":\"xlang-ide-settings-1\",\"external\":true}";
 CHECK(xrtFileWriteAtomic(settings,(xbytesview){(const unsigned char*)external,sizeof(external)-1}));
 CHECK(xuiEditSetText(xideSettingsViewControl(&a,XIDE_PREF_DOC_SIZE),"22")==XUI_OK && !xideSettingsViewApply(&a));
 CHECK(a.documentFont==original && a.documentFontSize==20 && !a.frameError);
 CHECK(xrtFileWriteAtomic(settings,(xbytesview){before,bytes}));
 CHECK(xuiEditSetText(xideSettingsViewControl(&a,XIDE_PREF_DOC_SIZE),"20")==XUI_OK);
 CHECK(click(&a,XIDE_PREF_CANCEL));
 CHECK(xideSettingsViewOpen(&a,XIDE_SETTINGS_EDITOR));
 CHECK(click(&a,XIDE_PREF_DEFAULTS) && a.codeFontSize==18);
 CHECK(xideSettingsViewApply(&a) && a.codeFontSize==16 && a.documentFontSize==20);
 CHECK(click(&a,XIDE_PREF_CANCEL));
 xide_document* next=xideDocumentNew(&a.documents,"xlang",&error);CHECK(next && xideEditorShow(&a,next)==XUI_OK && xuiCodeEditGetTabColumns(next->editor)==4);
 // Long documents require a real vertical scrollbar when Minimap is off.
 char longText[8192];size_t used=0;
 for(int i=0;i<180;++i){int n=snprintf(longText+used,sizeof(longText)-used,"// line %d: 中文\n",i);CHECK(n>0 && (size_t)n<sizeof(longText)-used);used+=(size_t)n;}
 CHECK(xuiCodeEditSetText(c->editor,longText)==XUI_OK && xuiCodeEditSetText(next->editor,longText)==XUI_OK);
 uint32_t cVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)),nextVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(next->editor));
 CHECK(scrollMode(&a,c,1) && scrollMode(&a,next,1));
 CHECK(xideSettingsViewOpen(&a,XIDE_SETTINGS_EDITOR));
 CHECK(xuiEditSetText(xideSettingsViewControl(&a,XIDE_PREF_SEARCH),"传统滚动条")==XUI_OK && settle(&a));
 CHECK(xuiWidgetGetVisible(minimap) && click(&a,XIDE_PREF_MINIMAP) && !xuiCheckBoxGetChecked(minimap));
 CHECK(xuiCodeEditGetMinimap(next->editor,&visible,&width)==XUI_OK && visible); // draft only
 CHECK(click(&a,XIDE_PREF_APPLY) && click(&a,XIDE_PREF_CANCEL));
 CHECK(scrollMode(&a,c,0) && scrollMode(&a,next,0));
 xideSettingsClear(&loaded);CHECK(xideSettingsLoad(&loaded,settings,&error) && !xideSettingsOptions(&loaded)->minimap);
 xide_document* traditional=xideDocumentNew(&a.documents,"c",&error);CHECK(traditional);
 CHECK(xuiCodeEditSetText(traditional->editor,longText)==XUI_OK && scrollMode(&a,traditional,0));
 CHECK(xideSettingsViewOpen(&a,XIDE_SETTINGS_EDITOR));
 CHECK(xuiEditSetText(xideSettingsViewControl(&a,XIDE_PREF_SEARCH),"minimap")==XUI_OK && settle(&a));
 CHECK(click(&a,XIDE_PREF_MINIMAP) && click(&a,XIDE_PREF_CANCEL));
 CHECK(scrollMode(&a,c,0) && scrollMode(&a,next,0)); // Cancel preserves the applied mode.
 CHECK(xideSettingsViewOpen(&a,XIDE_SETTINGS_EDITOR) && !xuiCheckBoxGetChecked(minimap));
 CHECK(xuiCheckBoxSetChecked(minimap,1)==XUI_OK && xideSettingsViewApply(&a) && click(&a,XIDE_PREF_CANCEL));
 CHECK(scrollMode(&a,c,1) && scrollMode(&a,next,1) && scrollMode(&a,traditional,1));
 CHECK(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor))==cVersion && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(next->editor))==nextVersion);
 CHECK(xideSettingsViewOpen(&a,XIDE_SETTINGS_APPEARANCE) && settle(&a));
 xui_rect_i_t rect={0,0,a.viewportWidth,a.viewportHeight};CHECK(xuiUpdate(a.context,0)==XUI_OK && a.proxy.surfaceClear(&a.proxy,a.target,0xffffffff)==XUI_OK && xuiRender(a.context,a.target,&rect,1)==XUI_OK && xideAppCapture(&a,image)==XUI_OK);
 puts("settings_view_test passed: top-level menu/shortcut, search, atomic apply/cancel/conflict, three font roles, theme, existing/new C/xlang editors, persisted Minimap/traditional scrollbar switching and cancellation without text changes, per-page defaults, native rendering; original documents preserved.");
cleanup:
 xrtFree(before);xideSettingsClear(&loaded);if(began)xgeEnd();xideAppDestroy(&a);free(settings);free(image);xgeQuit();return XGE_OK;
}
int main(int argc,char** argv)
{if(argc!=2)return 2;xge_desc_t d={0};d.iWidth=1500;d.iHeight=1000;d.sTitle="xlang IDE settings regression";d.iFlags=XGE_INIT_WINDOW;int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,argv[1]);xgeUnit();return result==XGE_OK && started && !failed ? 0 : 1;}
