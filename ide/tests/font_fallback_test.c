#include "app/xide_app.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include "services/ui_font.h"
#include "ui/editor_view.h"
#include "ui/font_view.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
/* Only the system font directory is isolated. Executable paths, disk reads,
   font loading, widget binding and rendering use the production implementation. */
static wchar_t privateWindows[32768];
static UINT WINAPI isolatedWindowsDirectory(LPWSTR output,UINT capacity)
{
    size_t length=wcslen(privateWindows);
    if(capacity<=length)return (UINT)length+1;
    memcpy(output,privateWindows,(length+1)*sizeof(wchar_t));return (UINT)length;
}
#define GetWindowsDirectoryW isolatedWindowsDirectory
#include "services/editor_font.c"
#undef GetWindowsDirectoryW
typedef struct fixture { const char *directory,*mode,*fontPath; HDESK desktop; int checks,failed,started; } fixture;
#define CHECK(e) do { ++f->checks; if(!(e)) { fprintf(stderr,"font_fallback_test:%d: %s; %s\n",__LINE__,#e,error.message); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* path,const char* text)
{ return xideIoWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int settle(xide_app* app)
{
    if(xuiDispatchPendingEvents(app->context)!=XUI_OK)return 0;
    xideEditorPoll(app);xideWorkbenchLayout(app);
    return xuiLayout(app->context)==XUI_OK && !app->frameError;
}
static int click(xide_app* app,int control)
{
    if(!settle(app))return 0;
    xui_rect_t r=xuiWidgetGetWorldRect(xideFontViewControl(app,control));int x=r.fX+r.fW/2,y=r.fY+r.fH/2;
    return xuiInputPointerMove(app->context,x,y,0)==XUI_OK &&
        xuiInputPointerDown(app->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK &&
        xuiInputPointerUp(app->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(app);
}
static int glyphs(xide_app* app,xui_font font)
{
    const char* text[]={"中","文","命","令","\xcd\xb8"};unsigned char pixels[5][64*48*4];
    xui_surface surface=NULL;xui_context context=NULL;xui_widget label=NULL;
    xui_surface_desc_t desc={0};desc.iWidth=64;desc.iHeight=48;desc.iKind=XUI_SURFACE_KIND_TEXTURE;
    desc.iFormat=XUI_SURFACE_FORMAT_RGBA8;desc.iFlags=XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED;
    xui_label_desc_t item={0};item.iSize=sizeof(item);item.pFont=font;item.sText="";item.iTextColor=XUI_COLOR_RGBA(0,0,0,255);
    int okay=app->proxy.surfaceCreate(&app->proxy,&surface,&desc)==XUI_OK && xuiCreate(&context)==XUI_OK &&
        xuiSetProxy(context,&app->proxy)==XUI_OK && xuiSetViewportSize(context,64,48)==XUI_OK &&
        xuiLabelCreate(context,&label,&item)==XUI_OK && xuiWidgetSetRect(label,(xui_rect_t){0,0,64,48})==XUI_OK && xuiSetRootWidget(context,label)==XUI_OK;
    for(int i=0;i<5 && okay;++i) {
        xui_rect_i_t rect={0,0,64,48};
        okay=xuiLabelSetText(label,text[i])==XUI_OK && xuiUpdate(context,0)==XUI_OK && xuiLayout(context)==XUI_OK &&
            app->proxy.surfaceClear(&app->proxy,surface,0xffffffffu)==XUI_OK && xuiRender(context,surface,&rect,1)==XUI_OK &&
            app->proxy.surfaceReadRGBA(&app->proxy,surface,pixels[i],64*4)==XUI_OK;
        int dark=0;for(size_t j=0;j<sizeof(pixels[i]);j+=4)if(pixels[i][j]<180)++dark;
        printf("Native label glyph raster: sample=%d dark=%d api=%d\n",i,dark,okay);fflush(stdout);
        if(i<4 && dark<12)okay=0;
    }
    if(okay)for(int i=0;i<4;++i)for(int j=i+1;j<5;++j)if(!memcmp(pixels[i],pixels[j],sizeof(pixels[i])))okay=0;
    if(context)xuiDestroy(context);
    if(surface)app->proxy.surfaceDestroy(&app->proxy,surface);
    return okay;
}
static int syntax(xui_widget editor,int firstKind)
{
    xui_code_token_buffer buffer=xuiCodeEditGetTokenBuffer(editor);uint32_t version=0;
    xui_code_token_t tokens[128];int count=0;
    int vr=xuiCodeTokenBufferGetVersion(buffer,&version);int tr=vr==XUI_OK ? xuiCodeTokenBufferGetTokens(buffer,version,tokens,128,&count) : -999;
    printf("Native token API: version=%d tokens=%d buffer=%d\n",vr,tr,buffer!=NULL);fflush(stdout);
    if(vr!=XUI_OK || tr!=XUI_OK)return 0;
    printf("Native syntax: language=%s version=%u tokens=%d\n",xuiCodeEditGetLanguage(editor),version,count);
    for(int i=0;i<count;++i)printf("token %d start=%d end=%d kind=%d\n",i,tokens[i].iStartOffset,tokens[i].iEndOffset,tokens[i].iKind);
    fflush(stdout);
    int first=0,comment=0,string=0;
    for(int i=0;i<count;++i) { if(tokens[i].iStartOffset==0 && tokens[i].iKind==firstKind)first=1; if(tokens[i].iKind==XUI_CODE_TOKEN_COMMENT)comment=1; if(tokens[i].iKind==XUI_CODE_TOKEN_STRING)string=1; }
    return first && comment && string;
}
static int editorSyntax(xide_app* app,xide_document* document,int firstKind)
{
    xui_rect_i_t rect={0,0,app->viewportWidth,app->viewportHeight};
    return xideEditorShow(app,document)==XUI_OK && settle(app) && xuiUpdate(app->context,0)==XUI_OK &&
        xuiRender(app->context,app->target,&rect,1)==XUI_OK && syntax(document->editor,firstKind);
}
static int capture(xide_app* app,const char* path)
{
    xui_rect_i_t rect={0,0,app->viewportWidth,app->viewportHeight};size_t length=(size_t)rect.iW*rect.iH*4;unsigned char* pixels=malloc(length);
    if(!pixels)return 0;
    int ret=xuiUpdate(app->context,0);if(ret==XUI_OK)ret=app->proxy.surfaceClear(&app->proxy,app->target,0xffffffffu);
    if(ret==XUI_OK)ret=xuiRender(app->context,app->target,&rect,1);
    if(ret==XUI_OK)ret=app->proxy.surfaceReadRGBA(&app->proxy,app->target,pixels,rect.iW*4);
    char* native=xidePathNativeUtf8(path);if(ret==XUI_OK && native)ret=xgeImageSavePNG(native,rect.iW,rect.iH,pixels,rect.iW*4);else ret=XUI_ERROR;
    free(native);free(pixels);return ret==XUI_OK;
}
static int verify(void* user)
{
    fixture* f=user;xide_app app={0};xide_model_error error={0};f->started=1;
    char *settings=NULL,*windows=NULL,*fonts=NULL,*bad=NULL,*system=NULL,*bundled=NULL,*image=NULL,*startupImage=NULL,*defaultUi=NULL,*defaultCode=NULL;
    unsigned char* original=NULL;size_t originalSize=0;int began=0;
    int portable=!strcmp(f->mode,"--portable-write") || !strcmp(f->mode,"--portable-read");
    CHECK(xideIoCreateDirectories(f->directory));
    settings=portable ? xideSettingsDefaultPath() : xidePathResolve(f->directory,"settings.json");
    windows=xidePathResolve(f->directory,"private-windows");fonts=xidePathResolve(windows,"Fonts");
    bundled=xideBundledFontPath();image=xidePathResolve(f->directory,"native-font-fallback.png");
    startupImage=xidePathResolve(f->directory,"native-font-startup.png");
    CHECK(settings && fonts && bundled && image && xideIoCreateDirectories(fonts));
    CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,windows,-1,privateWindows,32768)>0);
    if(!strcmp(f->mode,"--corrupt")) {
        const char* names[]={"msyh.ttc","msyh.ttf","consola.ttf"};
        for(int i=0;i<3;++i) { bad=xidePathResolve(fonts,names[i]);CHECK(bad && writeText(bad,"not a font"));free(bad);bad=NULL; }
    } else if(!strcmp(f->mode,"--latin")) {
        wchar_t directory[32768];CHECK(GetWindowsDirectoryW(directory,32768)>0);
        char directoryUtf8[32768];CHECK(WideCharToMultiByte(CP_UTF8,0,directory,-1,directoryUtf8,sizeof(directoryUtf8),NULL,NULL)>0);
        system=xidePathResolve(directoryUtf8,"Fonts/consola.ttf");bad=xidePathResolve(fonts,"consola.ttf");
        CHECK(system && bad && xideIoCopy(system,bad,0));free(bad);bad=NULL;
    }
    if(!portable) {
        CHECK(writeText(settings,!strcmp(f->mode,"--saved-bad") ? "{\"format\":\"xlang-ide-settings-1\",\"ui\":{\"font\":{\"path\":\"missing.otf\",\"size\":24}},\"editor\":{\"font\":{\"path\":\"missing.otf\",\"size\":24}},\"future\":7}" : "{\"format\":\"xlang-ide-settings-1\",\"future\":7}"));
        if(f->fontPath) {
            xide_settings draft={0};int saved=xideSettingsLoad(&draft,settings,&error) &&
                xideSettingsSetUiFont(&draft,f->fontPath,16,&error) && xideSettingsSetEditorFont(&draft,f->fontPath,16,&error);
            xideSettingsClear(&draft);CHECK(saved);
        }
        original=xideIoReadAllLimit(settings,256u*1024u,&originalSize);CHECK(original);
    }
    app.hidden=1;app.noSession=1;app.maxFrames=100000;app.settingsPath=settings;
    int result=xideAppCreate(&app);
    if(!strcmp(f->mode,"--no-bundle")) { CHECK(result!=XUI_OK && !app.uiFont);printf("font_fallback_test passed: %d checks; missing assets rejected, startup=%d\n",f->checks,result);goto cleanup; }
    CHECK(result==XUI_OK && app.uiFont && app.codeFont && xidePathEqual(app.uiFontPath,f->fontPath ? f->fontPath : bundled));
    CHECK(xgeBegin()==XGE_OK);began=1;CHECK(settle(&app));
    CHECK(startupImage && capture(&app,startupImage));
    CHECK(glyphs(&app,app.uiFont) && glyphs(&app,app.codeFont));
    if(!strcmp(f->mode,"--latin")) { CHECK(!xidePathEqual(app.codeFontPath,bundled)); }
    else { CHECK(xidePathEqual(app.codeFontPath,f->fontPath ? f->fontPath : bundled)); }
    if(!strcmp(f->mode,"--portable-read")) { CHECK(app.uiFontSize==20 && app.codeFontSize==24); }
    else { CHECK(app.uiFontSize==16 && app.codeFontSize==16); }
    if(original) { size_t size=0;unsigned char* disk=xideIoReadAllLimit(settings,256u*1024u,&size);int unchanged=disk && size==originalSize && !memcmp(disk,original,size);xrtFree(disk);CHECK(unchanged); }
    defaultUi=xideUiFontDefaultPath();defaultCode=xideEditorFontDefaultPath();CHECK(defaultUi && defaultCode);
    CHECK(xideFontViewOpenUi(&app) && click(&app,XIDE_FONT_RESET));
    CHECK(xidePathEqual(xuiEditGetText(xideFontViewControl(&app,XIDE_FONT_PATH)),defaultUi));CHECK(click(&app,XIDE_FONT_CANCEL));
    CHECK(xideFontViewOpen(&app) && click(&app,XIDE_FONT_RESET));
    CHECK(xidePathEqual(xuiEditGetText(xideFontViewControl(&app,XIDE_FONT_PATH)),defaultCode));CHECK(click(&app,XIDE_FONT_CANCEL));
    xide_document* c=xideDocumentNew(&app.documents,"c",&error);CHECK(c && xideEditorShow(&app,c)==XUI_OK);
    xide_document* xl=xideDocumentNew(&app.documents,"xlang",&error);CHECK(xl && xideEditorShow(&app,xl)==XUI_OK);
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"int value = 1; // 中文注释\nconst char* text = \"中文\";\n")==XUI_OK);
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(xl->editor),0,"var value as int = 2 // 中文注释\nvar text as str = \"中文\"\n")==XUI_OK);
    CHECK(editorSyntax(&app,c,XUI_CODE_TOKEN_TYPE) && editorSyntax(&app,xl,XUI_CODE_TOKEN_KEYWORD));
    uint32_t cVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)),xlVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor));
    CHECK(xideEditorFontApply(&app,bundled,24,&error) && xideUiFontApply(&app,bundled,20,&error));
    CHECK(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor))==cVersion && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor))==xlVersion);
    CHECK(glyphs(&app,app.codeFont) && editorSyntax(&app,c,XUI_CODE_TOKEN_TYPE) && editorSyntax(&app,xl,XUI_CODE_TOKEN_KEYWORD));
    xui_font_metrics_t metrics={0};CHECK(app.proxy.fontGetMetrics(&app.proxy,app.codeFont,&metrics)==XUI_OK && fabsf(metrics.fSize-24)<0.01f);
    CHECK(capture(&app,image));
    printf("font_fallback_test passed: %d checks; real CJK glyphs, C/xlang syntax, default drafts, OTF apply, mode=%s; UI=%d editor=%d\n",f->checks,f->mode,app.uiFontSize,app.codeFontSize);fflush(stdout);
cleanup:
    if(began)xgeEnd();
    xideAppDestroy(&app);xgeSetQuitRequestCallback(NULL,NULL);xgeQuit();xrtFree(original);
    free(settings);free(windows);free(fonts);free(bad);free(system);free(bundled);free(image);free(startupImage);free(defaultUi);free(defaultCode);return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
    fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;
    xge_desc_t d={0};d.iWidth=1280;d.iHeight=800;d.sTitle="Private CJK fallback verification";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(verify,f);xgeUnit();return result==XGE_OK && f->started && !f->failed ? 0 : 1;
}
int main(void)
{
    int argc=0;wchar_t** wide=CommandLineToArgvW(GetCommandLineW(),&argc);if(!wide)return 1;
    char** argv=calloc((size_t)argc,sizeof(*argv));if(!argv) { LocalFree(wide);return 1; }
    for(int i=0;i<argc;++i) {
        int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,NULL,0,NULL,NULL);
        argv[i]=size ? malloc((size_t)size) : NULL;
        if(!argv[i]) { LocalFree(wide);return 1; }
        WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,argv[i],size,NULL,NULL);
    }
    LocalFree(wide);
    if(argc!=3 && argc!=4) { return 2; }
    fixture f={0};f.directory=argv[1];f.mode=argv[2];f.fontPath=argc==4 ? argv[3] : NULL;
    wchar_t name[100];swprintf(name,100,L"XlangFontFallback-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;
    if(thread) { if(WaitForSingleObject(thread,60000)!=WAIT_OBJECT_0)ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread); }
    CloseDesktop(f.desktop);for(int i=0;i<argc;++i)free(argv[i]);free(argv);return (int)code;
}
