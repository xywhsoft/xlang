#include "ui/toolchain_view.h"
#include "platform/dpi_proxy.h"
#include "ui/resource_view.h"
#include "ui/import_view.h"
#include "ui/template_view.h"
#include "ui/rename_view.h"
#include "ui/references_view.h"
#include "ui/file_changes_view.h"
#include "ui/font_view.h"
#include "ui/goto_view.h"
#include "ui/recovery_view.h"
#include "services/ui_font.h"
#include "ui/configuration_view.h"
#include "services/editor_font.h"
#include "lang/languages.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define TRY(call) do { int result_=(call); if(result_!=XUI_OK) return result_; } while(0)
struct xide_font_view { xui_widget window,content,message,title,detail,hint,controls[XIDE_FONT_CONTROL_COUNT]; xide_ui_layout layout; float scale; int ui; };
static void fontSelected(xide_app* a,const char* path,void* user)
{ if(path && a->fontView) xuiEditSetText(a->fontView->controls[XIDE_FONT_PATH],path); }
static void clicked(xui_widget widget,void* user)
{
    xide_app* app=user; xide_font_view* view=app->fontView;
    if(widget==view->controls[XIDE_FONT_CANCEL]) xideDialogSetOpen(view->window,0);
    else if(widget==view->controls[XIDE_FONT_RESET]) {
        char* path=view->ui ? xideUiFontDefaultPath() : xideEditorFontDefaultPath(); if(path) { xuiEditSetText(view->controls[XIDE_FONT_PATH],path); free(path); }
        xuiEditSetText(view->controls[XIDE_FONT_SIZE],"16"); xuiLabelSetText(view->message,"默认值已填入；点击应用后保存。");
    } else if(widget==view->controls[XIDE_FONT_BROWSE]) {
        xideFileDialogChoose(app,view->window,XUI_FILE_DIALOG_MODE_OPEN_FILE,view->ui ? "选择界面字体" : "选择代码字体","字体|*.ttf;*.ttc;*.otf",xuiEditGetText(view->controls[XIDE_FONT_PATH]),NULL,fontSelected,app);
    } else if(widget==view->controls[XIDE_FONT_APPLY]) {
        const char* text=xuiEditGetText(view->controls[XIDE_FONT_SIZE]); char* end; long size=strtol(text,&end,10); xide_model_error error={0};
        if(!*text || *end || size<8 || size>40) xuiLabelSetText(view->message,"字号必须是 8 到 40 之间的整数。");
        else if(view->ui ? xideUiFontApply(app,xuiEditGetText(view->controls[XIDE_FONT_PATH]),(int)size,&error) : xideEditorFontApply(app,xuiEditGetText(view->controls[XIDE_FONT_PATH]),(int)size,&error)) xideDialogSetOpen(view->window,0);
        else xuiLabelSetText(view->message,error.message);
    }
    xgeRenderRequest();
}
static int label(xide_app* app,const char* text,xui_rect_t rect,xui_widget* out)
{
    xui_label_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont; desc.sText=text; desc.iWrapMode=XUI_TEXT_WRAP_WORD;
    xui_widget widget=NULL; TRY(xuiLabelCreate(app->context,&widget,&desc)); TRY(xuiWidgetSetRect(widget,rect)); TRY(xuiWidgetAddChild(app->fontView->content,widget)); if(out) *out=widget; return XUI_OK;
}
static int input(xide_app* app,int control,const char* name,xui_rect_t rect)
{
    xui_input_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont; xui_widget* widget=&app->fontView->controls[control];
    TRY(xuiInputCreate(app->context,widget,&desc)); TRY(xuiWidgetSetRect(*widget,rect)); TRY(xuiWidgetSetAccessibleName(*widget,name)); return xuiWidgetAddChild(app->fontView->content,*widget);
}
static int button(xide_app* app,int control,const char* name,xui_rect_t rect)
{
    xui_button_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont; desc.sText=name; xui_widget* widget=&app->fontView->controls[control];
    TRY(xuiButtonCreate(app->context,widget,&desc)); TRY(xuiWidgetSetRect(*widget,rect)); TRY(xuiButtonSetClick(*widget,clicked,app)); return xuiWidgetAddChild(app->fontView->content,*widget);
}
static void windowClosed(xui_widget widget,void* user) { xide_app* a=user; clicked(a->fontView->controls[XIDE_FONT_CANCEL],a); }
int xideFontViewCreate(xide_app* app)
{
    xide_font_view* view=calloc(1,sizeof(*view)); if(!view) return XUI_ERROR_OUT_OF_MEMORY; app->fontView=view;
    TRY(xideDialogCreate(app,&view->window,"代码字体 · C 与 xlang 共用",576,400,14)); TRY(xideDialogSetCancel(view->window,windowClosed,app)); view->content=xideDialogGetContentWidget(view->window);
    TRY(xuiWidgetAddChild(app->root,view->window)); TRY(xuiWidgetSetLayoutType(view->content,XUI_LAYOUT_MANUAL));
    TRY(label(app,"代码字体 · C 与 xlang 共用",(xui_rect_t){0,0,576,28},&view->title));
    TRY(label(app,"TTF / TTC / OTF 文件（建议使用等宽字体）",(xui_rect_t){0,38,576,24},&view->detail));
    TRY(input(app,XIDE_FONT_PATH,"代码字体文件",(xui_rect_t){0,66,478,32})); TRY(button(app,XIDE_FONT_BROWSE,"浏览…",(xui_rect_t){490,66,86,32}));
    TRY(label(app,"字号（DIP，8–40）",(xui_rect_t){0,108,276,24},NULL)); TRY(input(app,XIDE_FONT_SIZE,"代码字号",(xui_rect_t){286,106,90,32}));
    TRY(button(app,XIDE_FONT_RESET,"填入默认值",(xui_rect_t){400,106,176,32}));
    TRY(label(app,"当前字体效果；应用后同步编辑器、帮助范例、输出和终端。",(xui_rect_t){0,152,576,28},&view->hint));
    xui_code_edit_desc_t code={0}; code.iSize=sizeof(code); code.pFont=app->codeFont; code.bReadonly=1; code.bShowLineNumbers=1;
    code.sText="// 中文 / C / xlang / 0123456789\nfunc main(...) as int\n    return 0\nend\n";
    TRY(xideDpiCodeEditCreate(app->context,&view->controls[XIDE_FONT_PREVIEW],&code)); TRY(xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(view->controls[XIDE_FONT_PREVIEW])));
    TRY(xideConfigureEditorLanguage(view->controls[XIDE_FONT_PREVIEW],"xlang")); TRY(xuiWidgetSetRect(view->controls[XIDE_FONT_PREVIEW],(xui_rect_t){0,184,576,114})); TRY(xuiWidgetAddChild(view->content,view->controls[XIDE_FONT_PREVIEW]));
    TRY(label(app,"",(xui_rect_t){0,308,576,44},&view->message));
    TRY(button(app,XIDE_FONT_APPLY,"应用并保存",(xui_rect_t){332,360,128,32})); TRY(button(app,XIDE_FONT_CANCEL,"取消",(xui_rect_t){476,360,100,32})); return xideUiLayoutCapture(&view->layout,view->content);
}
static int openFont(xide_app* app,int ui)
{
    xide_font_view* view=app->fontView; if(!view || xideGotoViewIsOpen(app) || xideToolchainViewIsOpen(app) || xideResourceViewIsOpen(app) || xideImportViewIsOpen(app) || xideRenameViewIsOpen(app) || xideTemplateViewIsOpen(app) || xideReferencesViewIsOpen(app) || xideConfigurationViewIsOpen(app) || xideRecoveryViewIsOpen(app) || xideFileChangesViewIsOpen(app)) return 0;
    view->ui=ui;
    xuiLabelSetText(view->title,ui ? "界面字体" : "代码字体 · C 与 xlang 共用"); xuiWindowSetTitle(view->window,ui ? "界面字体" : "代码字体 · C 与 xlang 共用");
    xuiLabelSetText(view->detail,ui ? "TTF / TTC / OTF 文件（保留中文字体回退）" : "TTF / TTC / OTF 文件（建议使用等宽字体）");
    xuiLabelSetText(view->hint,ui ? "当前界面字体效果；代码字体单独设置。" : "当前字体效果；应用后同步编辑器、帮助范例、输出和终端。");
    xuiWidgetSetAccessibleName(view->controls[XIDE_FONT_PATH],ui ? "界面字体文件" : "代码字体文件");
    xuiWidgetSetAccessibleName(view->controls[XIDE_FONT_SIZE],ui ? "界面字号" : "代码字号");
    xuiCodeEditSetText(view->controls[XIDE_FONT_PREVIEW],ui ? "界面字体预览 / User interface\n文件  项目  编辑  视图\n工作空间 · 项目属性 · 命令帮助\n中文 ABC 0123456789\n" : "// 中文 / C / xlang / 0123456789\nfunc main(...) as int\n    return 0\nend\n");
    xideConfigureEditorLanguage(view->controls[XIDE_FONT_PREVIEW],ui ? "text" : "xlang");
    xideCodeWidgetSetFont(app,view->controls[XIDE_FONT_PREVIEW],ui ? app->uiFont : app->codeFont);
    int current=ui ? app->uiFontSize : app->codeFontSize;
    char size[32]; snprintf(size,sizeof(size),"%d",current ? current : 16);
    xuiEditSetText(view->controls[XIDE_FONT_PATH],(ui ? app->uiFontPath : app->codeFontPath) ? (ui ? app->uiFontPath : app->codeFontPath) : ""); xuiEditSetText(view->controls[XIDE_FONT_SIZE],size); xuiLabelSetText(view->message,"");
    xideFontViewLayout(app); xideDialogSetOpen(view->window,1); xuiSetFocusWidget(app->context,view->controls[XIDE_FONT_PATH]); xgeRenderRequest(); return 1;
}
int xideFontViewOpen(xide_app* app) { return openFont(app,0); }
int xideFontViewOpenUi(xide_app* app) { return openFont(app,1); }
int xideFontViewIsOpen(xide_app* app) { return app->fontView && xideDialogIsOpen(app->fontView->window); }
void xideFontViewLayout(xide_app* app)
{
    if(!app->fontView) return;
    int width=app->viewportWidth-48,height=app->viewportHeight-48; if(width<200) width=200; if(height<200) height=200;
    xideDialogSetMaxSize(app->fontView->window,width,height);
    int contentWidth=xideUiHeight(app,576),contentHeight=xideUiHeight(app,400);
    float scale=xideUiFontScale(app);
    if(app->fontView->scale!=scale) { xideUiLayoutApply(app,&app->fontView->layout,1); xideDialogSetContentSize(app->fontView->window,contentWidth,contentHeight); app->fontView->scale=scale; }

}
int xideFontViewCodeFont(xide_app* app) { return app->fontView ? xideCodeWidgetSetFont(app,app->fontView->controls[XIDE_FONT_PREVIEW],app->fontView->ui ? app->uiFont : app->codeFont) : XUI_OK; }
int xideFontViewUiFont(xide_app* app) { return app->fontView && app->fontView->ui ? xideCodeWidgetSetFont(app,app->fontView->controls[XIDE_FONT_PREVIEW],app->uiFont) : XUI_OK; }
xui_widget xideFontViewControl(xide_app* app,int control) { return app->fontView && control>=0 && control<XIDE_FONT_CONTROL_COUNT ? app->fontView->controls[control] : NULL; }
void xideFontViewDestroy(xide_app* app) { free(app->fontView); app->fontView=NULL; }

xui_widget xideFontViewWindow(xide_app* app) { return app->fontView ? app->fontView->window : NULL; }
