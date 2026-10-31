#include "ui/template_view.h"
#include "ui/file_changes_view.h"
#include "services/editor_font.h"
#include "ui/catalog_view.h"
#include "ui/font_view.h"
#include "ui/recovery_view.h"
#include "platform/path.h"
#include "platform/startup_alias.h"
#include "platform/file_io.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static int fail(xide_model_error* error,int code,const char* message)
{
    if(error) { error->code=code; snprintf(error->message,sizeof(error->message),"%s",message); } return 0;
}
char* xideWindowsFontPath(const char* filename)
{
    wchar_t directory[32768]; UINT length=GetWindowsDirectoryW(directory,32768); if(!length || length>=32768) return NULL;
    int bytes=WideCharToMultiByte(CP_UTF8,0,directory,-1,NULL,0,NULL,NULL); char* text=bytes ? malloc((size_t)bytes) : NULL;
    if(!text) return NULL;
    WideCharToMultiByte(CP_UTF8,0,directory,-1,text,bytes,NULL,NULL);
    char *fonts=xidePathResolve(text,"Fonts"),*path=fonts ? xidePathResolve(fonts,filename) : NULL; free(text); free(fonts); return path;
}
static char* bundledFontPath(const char* relative)
{
    wchar_t binary[32768]; DWORD length=GetModuleFileNameW(NULL,binary,32768);
    if(!length || length>=32768) return NULL;
    const wchar_t* resource=xideStartupResourceModule(binary);
    int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,resource,-1,NULL,0,NULL,NULL);
    char* text=count ? malloc((size_t)count) : NULL;
    if(!text) return NULL;
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,resource,-1,text,count,NULL,NULL)) { free(text); return NULL; }
    char* directory=xidePathDirectory(text); free(text);
    char* path=directory ? xidePathResolve(directory,relative) : NULL;
    free(directory); return path;
}
char* xideBundledFontPath(void)
{
    return bundledFontPath("data/fonts/NotoSansMonoCJKsc-Regular.otf");
}
char* xideEditorFontDefaultPath(void)
{
    // 默认字形固定随包提供，不受机器安装的系统字体影响。
    return bundledFontPath("data/fonts/Consolas-with-Yahei_Nerd_Font.ttf");
}
static uint32_t big32(const unsigned char* bytes)
{
    return (uint32_t)bytes[0]<<24 | (uint32_t)bytes[1]<<16 | (uint32_t)bytes[2]<<8 | bytes[3];
}
static int container(const unsigned char* bytes,size_t size)
{
    if(size<12) return 0;
    size_t offset=0;
    if(!memcmp(bytes,"ttcf",4)) {
        uint32_t count=big32(bytes+8); if(!count || count>(size-12)/4) return 0;
        offset=big32(bytes+12); if(offset>size-12) return 0;
    }
    const unsigned char* face=bytes+offset;
    if(big32(face)!=0x00010000u && memcmp(face,"true",4) && memcmp(face,"OTTO",4)) return 0;
    unsigned tables=(unsigned)face[4]<<8 | face[5]; if(!tables || tables>(size-offset-12)/16) return 0;
    for(unsigned i=0;i<tables;++i) {
        const unsigned char* table=face+12+i*16; size_t start=big32(table+8),length=big32(table+12);
        if(start>size || length>size-start) return 0;
    }
    return 1;
}
int xideFontLoad(xide_app* app,const char* path,int size,xui_font* font,xide_model_error* error)
{
    if(!path || size<8 || size>40 || !xidePathExists(path) || (!xidePathHasExtension(path,".ttf") && !xidePathHasExtension(path,".ttc") && !xidePathHasExtension(path,".otf")))
        return fail(error,XIDE_MODEL_INVALID,"Choose an existing TTF/TTC/OTF font and an integer size from 8 to 40.");
    size_t count=0; unsigned char* bytes=xideIoReadAllLimit(path,64u*1024u*1024u,&count);
    if(!bytes || !container(bytes,count)) { xrtFree(bytes); return fail(error,XIDE_MODEL_INVALID,"The selected file is unreadable or has an invalid font container."); }
    char* fallbacks[]={xideWindowsFontPath("msyh.ttc"),xideWindowsFontPath("msyh.ttf"),xideBundledFontPath()};
    xgeFontFallbackClear();
    for(size_t i=0;i<3;++i) {
        if(!fallbacks[i] || xidePathEqual(path,fallbacks[i])) continue;
        size_t length=0; unsigned char* data=xideIoReadAllLimit(fallbacks[i],64u*1024u*1024u,&length);
        int loaded=data && container(data,length) && xgeFontFallbackSetMemory(data,(int)length,(float)size)==XGE_OK;
        xrtFree(data); if(loaded) break;
    }
    int result=app->proxy.fontLoadMemory(&app->proxy,font,bytes,(int)count,(float)size,XUI_FONT_FORMAT_TTF);
    xgeFontFallbackClear(); for(size_t i=0;i<3;++i) free(fallbacks[i]); xrtFree(bytes);
    xui_font_metrics_t metrics={0}; xui_vec2_t measured={0};
    if(result!=XUI_OK || !*font || app->proxy.fontGetMetrics(&app->proxy,*font,&metrics)!=XUI_OK ||
       !isfinite(metrics.fLineHeight) || metrics.fLineHeight<=0 || xideDpiTextMeasure(&app->proxy,*font," ",&measured)!=XUI_OK || !isfinite(measured.fX) || measured.fX<=0) {
        if(*font) app->proxy.fontDestroy(&app->proxy,*font);
        *font=NULL; return fail(error,XIDE_MODEL_INVALID,"The selected file cannot be loaded as a usable font.");
    }
    return 1;
}
float xideEditorLineHeight(xide_app* app)
{
    xui_font_metrics_t metrics={0};
    if(app->codeFont && app->proxy.fontGetMetrics && app->proxy.fontGetMetrics(&app->proxy,app->codeFont,&metrics)==XUI_OK && isfinite(metrics.fLineHeight) && metrics.fLineHeight>18)
        return metrics.fLineHeight;
    return 18;
}
static int completionFont(xide_app* app,xui_widget widget,xui_font font)
{
    if(!widget) return XUI_OK;
    if(xuiWidgetGetType(widget)==xuiListViewGetType(app->context)) {
        int result=xuiListViewSetFont(widget,font); if(result!=XUI_OK) return result;
        xui_font_metrics_t metrics={0}; app->proxy.fontGetMetrics(&app->proxy,font,&metrics);
        result=xuiListViewSetMetrics(widget,metrics.fLineHeight+6,4,1); if(result!=XUI_OK) return result;
    }
    for(xui_widget child=xuiWidgetGetFirstChild(widget);child;child=xuiWidgetGetNextSibling(child)) {
        int result=completionFont(app,child,font); if(result!=XUI_OK) return result;
    }
    return XUI_OK;
}
int xideCodeWidgetSetFont(xide_app* app,xui_widget widget,xui_font font)
{
    if(!widget) return XUI_OK;
    int result=xuiCodeEditSetFont(widget,font); if(result!=XUI_OK) return result;
    /* The pinned SDK does not rebind its pooled completion list in SetFont. */
    return completionFont(app,xuiCodeEditGetCompletionPopupWidget(widget),font);
}
int xideEditorFontStart(xide_app* app)
{
    xide_model_error error={0}; const char* saved=app->preferences.editorFont;
    int size=app->preferences.editorFontSize ? app->preferences.editorFontSize : 16;
    char* path=saved ? xidePathResolve(NULL,saved) : xideEditorFontDefaultPath();
    if(!xideFontLoad(app,path,size,&app->codeFont,&error)) {
        fprintf(stderr,"editor font: %s; trying default and bundled fonts.\n",error.message); free(path); path=xideEditorFontDefaultPath(); size=16;
        if(!xideFontLoad(app,path,size,&app->codeFont,&error)) {
            free(path); path=xideBundledFontPath();
            if(!xideFontLoad(app,path,size,&app->codeFont,&error)) { free(path); path=NULL; app->codeFont=app->uiFont; }
        }
    }
    app->codeFontPath=path; app->codeFontSize=size; return XUI_OK;
}
static int install(xide_app* app,xui_font font)
{
    app->codeFont=font; app->documents.font=font;
    int result=XUI_OK;
    for(size_t i=0;i<app->documents.count && result==XUI_OK;++i) result=xideCodeWidgetSetFont(app,app->documents.items[i]->editor,font);
    for(int i=0;i<2 && result==XUI_OK;++i) result=xideCodeWidgetSetFont(app,app->editors[i],font);
    if(result==XUI_OK) result=xideCatalogViewCodeFont(app);
    if(result==XUI_OK) result=xideFontViewCodeFont(app);
    if(result==XUI_OK) result=xideRecoveryViewCodeFont(app);
    if(result==XUI_OK) result=xideFileChangesViewCodeFont(app);
    if(result==XUI_OK) result=xideTemplateViewCodeFont(app);
    if(result==XUI_OK && app->panels[XIDE_OUTPUT]) result=xideCodeWidgetSetFont(app,app->panels[XIDE_OUTPUT],font);
    if(result==XUI_OK && app->panels[XIDE_TERMINAL]) {
        xui_vec2_t cell={0}; xideDpiTextMeasure(&app->proxy,font," ",&cell);
        result=xuiTerminalSetFont(app->panels[XIDE_TERMINAL],font);
        if(result==XUI_OK) result=xuiTerminalSetMetrics(app->panels[XIDE_TERMINAL],cell.fX,xideEditorLineHeight(app),4);
    }
    return result;
}
int xideEditorFontApply(xide_app* app,const char* path,int size,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!app || !app->context || !app->proxy.fontLoadMemory) return fail(error,XIDE_MODEL_INVALID,"A native workbench is required to change fonts.");
    char* resolved=xidePathResolve(NULL,path); xui_font font=NULL;
    if(!resolved) return fail(error,XIDE_MODEL_INVALID,"Invalid UTF-8 font path.");
    if(!xideFontLoad(app,resolved,size,&font,error)) { free(resolved); return 0; }
    typedef struct font_scroll { xui_widget widget; float x,y; } font_scroll;
    size_t count=app->documents.count+8;
    font_scroll* scroll=calloc(count,sizeof(*scroll));
    if(!scroll) { app->proxy.fontDestroy(&app->proxy,font); free(resolved); return fail(error,XIDE_MODEL_IO,"Cannot retain editor positions while changing fonts."); }
    for(size_t i=0;i<app->documents.count;++i) scroll[i].widget=app->documents.items[i]->editor;
    scroll[count-8].widget=xideTemplateViewControl(app,XIDE_TEMPLATE_PREVIEW);
    scroll[count-7].widget=app->editors[0]; scroll[count-6].widget=app->editors[1]; scroll[count-5].widget=app->helpExample;
    scroll[count-4].widget=xideFontViewControl(app,XIDE_FONT_PREVIEW); scroll[count-3].widget=xideRecoveryViewControl(app,XIDE_RECOVERY_CODE);
    scroll[count-2].widget=xideFileChangesViewControl(app,XIDE_CHANGES_LOCAL); scroll[count-1].widget=xideFileChangesViewControl(app,XIDE_CHANGES_DISK);
    for(size_t i=0;i<count;++i) if(scroll[i].widget) xuiCodeEditGetScroll(scroll[i].widget,&scroll[i].x,&scroll[i].y);
    xui_font previous=app->codeFont;
    int result=install(app,font);
    int saved=result==XUI_OK && xideSettingsSetEditorFont(&app->preferences,resolved,size,error);
    if(!saved) {
        int restored=install(app,previous);
        if(restored!=XUI_OK) app->frameError=restored;
        for(size_t i=0;i<count;++i) if(scroll[i].widget) xuiCodeEditSetScroll(scroll[i].widget,scroll[i].x,scroll[i].y);
        free(scroll);
        app->proxy.fontDestroy(&app->proxy,font); free(resolved);
        if(result!=XUI_OK) return fail(error,XIDE_MODEL_INVALID,"Cannot bind the new font to the workbench.");
        return 0;
    }
    free(scroll);
    /* All owned editors, closed documents and pooled completion lists rebound. */
    if(previous && previous!=app->uiFont) app->proxy.fontDestroy(&app->proxy,previous);
    free(app->codeFontPath); app->codeFontPath=resolved; app->codeFontSize=size;
    xgeRenderRequest(); return 1;
}
