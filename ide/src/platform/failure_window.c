#include "platform/failure_window.h"
#include "app/xide_app.h"
#include "services/editor_font.h"
#include "services/ui_font.h"
#include "theme/theme.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
typedef struct failure_window { xide_app app;xui_msgbox box;xui_widget details;int initialized,painted;const wchar_t* text;unsigned char* pixels; } failure_window;
static void dismissed(xui_msgbox box,int result,void* user) { xgeQuit(); }
static int frame(void* user) {
    failure_window* f=user;xide_app* a=&f->app;
    if(!f->initialized) {
        a->proxy=xuiProxyXge();a->viewportWidth=xgeGetWidth();a->viewportHeight=xgeGetHeight();
        int r=xuiCreate(&a->context);if(r!=XUI_OK) return r;
        xuiSetProxy(a->context,&a->proxy);xuiInputViewport(a->context,a->viewportWidth,a->viewportHeight);xuiSetLanguage(a->context,XUI_LANGUAGE_ZH);ideThemeInit(a->context,1);
        r=xuiWidgetCreate(a->context,&a->root);if(r!=XUI_OK) return r;
        xuiSetRootWidget(a->context,a->root);xuiWidgetSetRect(a->root,(xui_rect_t){0,0,a->viewportWidth,a->viewportHeight});
        char* font=xideUiFontDefaultPath();xide_model_error error={0};int loaded=font && xideFontLoad(a,font,16,&a->uiFont,&error);free(font);if(!loaded) return XUI_ERROR;
        xuiSetDefaultFont(a->context,a->uiFont);
        xui_surface_desc_t surface={0};surface.iKind=XUI_SURFACE_KIND_TEXTURE;surface.iWidth=a->viewportWidth;surface.iHeight=a->viewportHeight;surface.iFormat=XUI_SURFACE_FORMAT_RGBA8;surface.iFlags=XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED;
        r=a->proxy.surfaceCreate(&a->proxy,&a->target,&surface);if(r!=XUI_OK) return r;
        f->pixels=malloc((size_t)a->viewportWidth*(size_t)a->viewportHeight*4);if(!f->pixels) return XUI_ERROR_OUT_OF_MEMORY;
        int n=WideCharToMultiByte(CP_UTF8,0,f->text,-1,NULL,0,NULL,NULL);char* text=n ? malloc((size_t)n) : NULL;if(!text) return XUI_ERROR_OUT_OF_MEMORY;
        WideCharToMultiByte(CP_UTF8,0,f->text,-1,text,n,NULL,NULL);
        xui_msgbox_desc_t desc={0};desc.iSize=sizeof(desc);desc.sTitle="xlang IDE 运行失败";desc.sMessage="";desc.pFont=a->uiFont;desc.iType=XUI_MSGBOX_ICON_ERROR;desc.iButtons=XUI_MSGBOX_BUTTON_OK;desc.bModal=1;desc.bHasModal=1;
        r=xuiMsgBoxCreate(a->context,&f->box,&desc);if(r!=XUI_OK) { free(text);return r; }
        xui_msgbox_metrics_t metrics={0};metrics.iSize=sizeof(metrics);xuiMsgBoxGetMetrics(f->box,&metrics);metrics.fMinWidth=660;metrics.fMaxWidth=700;metrics.fMaxWidthRatio=.92f;metrics.fMinMessageHeight=360;xuiMsgBoxSetMetrics(f->box,&metrics);
        xui_text_edit_desc_t details={0};details.iSize=sizeof(details);details.pFont=a->uiFont;details.sText=text;details.bReadonly=1;details.bWordWrap=1;details.iMaxLength=160000;
        r=xuiTextEditCreate(a->context,&f->details,&details);free(text);if(r!=XUI_OK) return r;
        xuiWidgetSetAccessibleName(f->details,"错误原因、恢复状态与诊断文件");xuiWidgetAddChild(xuiMsgBoxGetContentWidget(f->box),f->details);
        xuiMsgBoxSetPresetButtonTitle(f->box,XUI_MSGBOX_BUTTON_TITLE_OK,"关闭");xuiMsgBoxSetResult(f->box,dismissed,f);xuiMsgBoxSetOpen(f->box,1);
        /* A bounded, read-only diagnostic inspection point for owned native
           launch tests. Rendering and interaction remain entirely XUI. */
        f->initialized=1;
    }
    int r=xgeBegin();if(r!=XGE_OK) return r;
    r=xuiProxyXgePumpInput(a->context);if(r==XUI_OK) r=xuiDispatchPendingEvents(a->context);if(r==XUI_OK) r=xuiLayout(a->context);if(r==XUI_OK) r=xuiUpdate(a->context,xgeGetDelta());
    if(r==XUI_OK) { xuiWidgetSetRect(f->details,xuiMsgBoxGetMessageRect(f->box));r=xuiLayout(a->context); }
    xui_rect_i_t all={0,0,a->viewportWidth,a->viewportHeight};
    if(r==XUI_OK) r=a->proxy.surfaceClear(&a->proxy,a->target,ideThemeColor(1,IDE_BG));
    if(r==XUI_OK) r=xuiRender(a->context,a->target,&all,1);
    if(r==XUI_OK) { xgeClear(ideThemeColor(1,IDE_BG));xui_rect_t rect={0,0,a->viewportWidth,a->viewportHeight};r=a->proxy.surfaceDraw(&a->proxy,a->target,rect,rect,0xffffffffu,XUI_SURFACE_DRAW_SCREEN_SPACE); }
    HWND window=(HWND)xgePlatformNativeHandle();
    RemovePropW(window,L"XlangFailureFrame");
    if(r==XUI_OK && a->proxy.surfaceReadRGBA(&a->proxy,a->target,f->pixels,a->viewportWidth*4)==XUI_OK) f->painted=1;
    int end=xgeEnd();
    if(r==XUI_OK && end==XGE_OK && f->painted) {
        SetPropW(window,L"XlangFailureText",(HANDLE)f->text);
        SetPropW(window,L"XlangFailureFrameWidth",(HANDLE)(INT_PTR)a->viewportWidth);
        SetPropW(window,L"XlangFailureFrameHeight",(HANDLE)(INT_PTR)a->viewportHeight);
        SetPropW(window,L"XlangFailureFrame",f->pixels);
    }
    return r==XUI_OK ? end : r;
}
static LRESULT CALLBACK emergencyProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_CLOSE || (message==WM_COMMAND && LOWORD(w)==IDOK)) { DestroyWindow(window);return 0; }
    return DefWindowProcW(window,message,w,l);
}
static void emergency(const wchar_t* text) {
    /* The graphics/font subsystem itself may be unavailable. Keep a visible
       emergency report without trying to reenter the failed renderer. */
    WNDCLASSW type={0};type.lpfnWndProc=emergencyProc;type.hInstance=GetModuleHandleW(NULL);type.lpszClassName=L"XlangFailureEmergency";type.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);type.hCursor=LoadCursorW(NULL,MAKEINTRESOURCEW(32512));RegisterClassW(&type);
    HWND window=CreateWindowExW(WS_EX_DLGMODALFRAME,type.lpszClassName,L"xlang IDE 运行失败",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,760,600,NULL,NULL,type.hInstance,NULL);
    if(!window) return;
    CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",text,WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,16,16,710,490,window,(HMENU)(INT_PTR)65535,type.hInstance,NULL);
    CreateWindowExW(0,L"BUTTON",L"关闭",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,626,520,100,30,window,(HMENU)(INT_PTR)IDOK,type.hInstance,NULL);
    ShowWindow(window,SW_SHOW);UpdateWindow(window);MSG message;
    while(IsWindow(window) && GetMessageW(&message,NULL,0,0)>0) { if(!IsDialogMessageW(window,&message)) { TranslateMessage(&message);DispatchMessageW(&message); } }
    if(IsWindow(window)) DestroyWindow(window);
    UnregisterClassW(type.lpszClassName,type.hInstance);
}
void xideFailureWindowShow(const wchar_t* text) {
    failure_window f={0};f.text=text;
    xge_desc_t desc={0};desc.iWidth=760;desc.iHeight=640;desc.sTitle="xlang IDE 运行失败";desc.iFlags=XGE_INIT_WINDOW|XGE_INIT_ON_DEMAND;desc.iRunMode=XGE_RUN_GAME_LOOP;desc.iTargetFPS=30;
    xgeSetQuitRequestCallback(NULL,NULL);int r=xgeInit(&desc);
    if(r==XGE_OK) { xgeRenderRequest();r=xgeRun(frame,&f); }
    if(xgePlatformNativeHandle()) {
        HWND window=(HWND)xgePlatformNativeHandle();RemovePropW(window,L"XlangFailureText");RemovePropW(window,L"XlangFailureFrame");
        RemovePropW(window,L"XlangFailureFrameWidth");RemovePropW(window,L"XlangFailureFrameHeight");
    }
    if(f.box) xuiMsgBoxDestroy(f.box);
    if(f.app.context) xuiDestroy(f.app.context);
    if(f.app.uiFont) f.app.proxy.fontDestroy(&f.app.proxy,f.app.uiFont);
    if(f.app.target) f.app.proxy.surfaceDestroy(&f.app.proxy,f.app.target);
    free(f.pixels);
    xgeUnit();if(r!=XGE_OK) { fprintf(stderr,"XUI failure report unavailable: %d\n",r);emergency(text); }
}
