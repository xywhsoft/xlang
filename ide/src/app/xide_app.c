#include "ui/import_view.h"
#include "ui/template_view.h"
#include "ui/rename_view.h"
#include "ui/file_changes_view.h"
#include "app/xide_app.h"
#include "services/file_sync.h"
#include "services/file_rebind.h"
#include "theme/theme.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "ui/configuration_view.h"
#include "ui/references_view.h"
#include "ui/resource_view.h"
#include "ui/toolchain_view.h"
#include "ui/language_view.h"
#include "ui/c_language_view.h"
#include "ui/catalog_view.h"
#include "ui/completion_view.h"
#include "ui/c_completion_view.h"
#include "services/session.h"
#include "services/editor_font.h"
#include "services/ui_font.h"
#include "ui/font_view.h"
#include "ui/goto_view.h"
#include "ui/recovery_view.h"
#include "platform/path.h"
#include "platform/window_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <math.h>
#define TRY_DOMAIN(call,domain_) do { int result_ = (call); if (result_ != 0) { xideFailureSet(&app->failure,app->frames ? XIDE_FAILURE_RENDER : XIDE_FAILURE_STARTUP,domain_,result_,"The interface could not create or render a required resource.",#call); fprintf(stderr, "workbench call failed at %s:%d: %s -> %d\n", __FILE__, __LINE__, #call, result_); return result_; } } while (0)
#define TRY(call) TRY_DOMAIN(call,"XUI")
#define TRY_XGE(call) TRY_DOMAIN(call,"XGE")
static void reportError(xui_context context, const xui_error_info_t* error, void* user)
{
    (void)context; xide_app* app=user;
    if(app->maxFrames) { app->frameError=error->iCode; xideFailureSet(&app->failure,XIDE_FAILURE_RENDER,"XUI",error->iCode,error->sMessage,error->sOperation); }
    int panel=-1; for(xui_widget widget=error->pWidget;widget && panel<0;widget=xuiWidgetGetParent(widget)) for(int i=0;i<XIDE_WINDOW_COUNT;++i) if(app->panels[i]==widget) { panel=i; break; }
    xui_rect_t rect=error->pWidget ? xuiWidgetGetWorldRect(error->pWidget) : (xui_rect_t){0};
    const char* type=error->pWidget ? xuiWidgetTypeGetName(xuiWidgetGetType(error->pWidget)) : "context";
    fprintf(stderr, "XUI %d: %s: %s (panel=%d, widget=%s, rect=%d,%d,%d,%d, cache=%d)\n", error->iCode, error->sOperation, error->sMessage,panel,type,rect.fX,rect.fY,rect.fW,rect.fH,error->pWidget ? xuiWidgetGetCachePolicy(error->pWidget).iPolicy : -1);
}
static int createTarget(xide_app* app, int width, int height)
{
    xui_surface_desc_t desc = {0};
    desc.iKind = XUI_SURFACE_KIND_TEXTURE; desc.iWidth = width; desc.iHeight = height;
    desc.iFormat = XUI_SURFACE_FORMAT_RGBA8;
    desc.iFlags = XUI_SURFACE_USAGE_TARGET | XUI_SURFACE_ALPHA_PREMULTIPLIED;
    xui_surface target = NULL;
    TRY(app->proxy.surfaceCreate(&app->proxy, &target, &desc));
    if (app->target) app->proxy.surfaceDestroy(&app->proxy, app->target);
    app->target = target; return XUI_OK;
}
static int quitRequest(void* user)
{
    xide_app* app=user; if(xideActionsHasModal(app)) return 0;
    xideMessageBeginQuit(app);
    if(!xideWorkspaceQuitRequest(app)) { xideMessageEndOperation(app);return 0; }
    xideMessageEndOperation(app);
    xide_model_error error={0};
    if(!xideSessionSave(app,1,&error)) fprintf(stderr,"session on exit: %s\n",error.message);
    return 1;
}
void xideAppLoadSettings(xide_app* app)
{
    if(app->preferencesInitialized) return;
    app->preferencesInitialized=1;
    char* path=app->settingsPath ? xidePathResolve(NULL,app->settingsPath) : app->hidden ? NULL : xideSettingsDefaultPath();
    if(path) { xide_model_error error={0}; if(!xideSettingsLoad(&app->preferences,path,&error)) fprintf(stderr,"preferences: %s\n",error.message); free(path); }
}
static float effectiveScale(xide_app* app)
{
    if(app->dpiOverride>0) return app->dpiOverride;
    /* The SDK resize ratio uses rounded integer window dimensions. Native DPI
       stays constant across those resizes; low-DPI test hosts keep the SDK ratio. */
    if(app->highDpi && xgePlatformNativeHandle()) return xideWindowDpi(xgePlatformNativeHandle())/96.0f;
    xge_platform_runtime_t runtime={0};
    return xgePlatformRuntimeGet(&runtime)==XGE_OK && isfinite(runtime.fDpiScale) && runtime.fDpiScale>=0.5f && runtime.fDpiScale<=8 ? runtime.fDpiScale : 1;
}
int xideAppCreate(xide_app* app)
{
    xideAppLoadSettings(app);
    if (app->hidden) ShowWindow((HWND)xgePlatformNativeHandle(), SW_HIDE);
    app->dpiScale=effectiveScale(app);
    app->viewportWidth=(int)lroundf(xgeGetWidth()/app->dpiScale); app->viewportHeight=(int)lroundf(xgeGetHeight()/app->dpiScale);
    TRY(xideDpiProxyCreate(&app->dpiProxy,&app->proxy,app->dpiScale));
    TRY(xuiSetDefaultLanguage(XUI_LANGUAGE_ZH));
    TRY(xuiCreate(&app->context)); TRY(xuiSetProxy(app->context, &app->proxy));
    TRY(xuiSetLanguage(app->context, XUI_LANGUAGE_ZH)); TRY(xuiSetErrorCallback(app->context, reportError, app));
    TRY(xuiInputViewport(app->context,app->viewportWidth,app->viewportHeight));
    TRY(xuiInputDpi(app->context,app->dpiScale));
    TRY(ideThemeInit(app->context, app->theme));
    TRY(createTarget(app, app->viewportWidth, app->viewportHeight));
    TRY(xideUiFontStart(app));
    TRY(xideEditorFontStart(app));
    TRY(xuiSetDefaultFont(app->context, app->uiFont));
    TRY(xideWorkbenchCreate(app)); TRY(xideUiFontInitialize(app));
    /* Draw composition inline; the operating system retains candidate UI. */
    int result=xgeImeSetMode(XGE_IME_MODE_COMPOSITION);
    if(result==XGE_ERROR_UNSUPPORTED) result=xgeImeSetMode(XGE_IME_MODE_NATIVE);
    if(result!=XGE_OK) { xideFailureSet(&app->failure,XIDE_FAILURE_STARTUP,"XGE",result,"Unable to initialize the text input interface.","xgeImeSetMode"); return result; }
    xgeSetQuitRequestCallback(quitRequest,app);
    if((app->workspacePath || app->projectPath) && (app->noSession || (app->hidden && !app->sessionRoot))) {
        xide_model_error error={0};
        if(!xideWorkspaceOpen(app,app->workspacePath ? app->workspacePath : app->projectPath,app->projectPath!=NULL,&error)) {
            xideFailureSet(&app->failure,XIDE_FAILURE_WORKSPACE,"model",error.code,error.message,"xideWorkspaceOpen");
            fprintf(stderr,"workspace startup failed: %s\n",error.message); return XUI_ERROR;
        }
    }
    if(!xideSessionStart(app)) { xideFailureSet(&app->failure,XIDE_FAILURE_STARTUP,"XUI",XUI_ERROR_OUT_OF_MEMORY,"Unable to initialize the session and recovery interface.","xideSessionStart"); return XUI_ERROR_OUT_OF_MEMORY; }
    if(app->initialSelectedProject) {
        if(!xideWorkspaceViewSelectProject(app,app->initialSelectedProject)) {
            xideFailureSet(&app->failure,XIDE_FAILURE_WORKSPACE,"XUI",XUI_ERROR,"Selected project ID not found.","xideWorkspaceViewSelectProject");
            fputs("Selected project ID not found.\n",stderr); return XUI_ERROR;
        }
    }
    if (app->initialEditor) xideDispatchAction(app, XIDE_ACTION_C);
    if(app->initialFile && !xideWorkspaceOpenFile(app,app->initialFile)) { xideFailureSet(&app->failure,XIDE_FAILURE_DOCUMENT,"XUI",XUI_ERROR,app->initialFile,"xideWorkspaceOpenFile"); return XUI_ERROR; }
    if(app->initialCommandSearch && xideCatalogViewSearch(app,app->initialCommandSearch)!=XUI_OK) { xideFailureSet(&app->failure,XIDE_FAILURE_COMMAND,"XUI",XUI_ERROR,"Unable to filter the command catalog.","xideCatalogViewSearch"); return XUI_ERROR; }
    if(app->initialCommand) { const xide_command* command=xideCatalogFind(&app->catalog,app->initialCommand); if(!command) { xideFailureSet(&app->failure,XIDE_FAILURE_COMMAND,"XUI",XUI_ERROR,app->initialCommand,"xideCatalogFind"); return XUI_ERROR; } xideWorkbenchShowHelp(app,(int)(command-app->catalog.entries)); }
    if(app->initialTask) { app->taskStartMs=GetTickCount64(); xideDispatchAction(app,app->initialTask); }
    if(app->initialConfigurations && !xideConfigurationViewOpen(app)) return XUI_ERROR;
    if(app->initialFontSettings && !xideFontViewOpen(app)) return XUI_ERROR;
    if(app->initialRecovery && !xideRecoveryViewOpen(app)) return XUI_ERROR;
    if(app->initialUiFontSettings && !xideFontViewOpenUi(app)) return XUI_ERROR;
    xide_model_error syncError={0}; if(!xideFileSyncStart(app,&syncError)) { xideFailureSet(&app->failure,XIDE_FAILURE_STARTUP,"model",syncError.code,syncError.message,"xideFileSyncStart"); fprintf(stderr,"file monitor startup: %s\n",syncError.message); return XUI_ERROR; }
    return xideWorkbenchApplyTheme(app, app->theme);
}
void xideAppDestroy(xide_app* app)
{
    xideCLanguageViewStop(app); xideCCompletionViewStop(app); xideCompletionViewStop(app); xideLanguageViewStop(app);
    xideToolchainViewStop(app);
    if(app->context) xideFileDialogDestroy(app);
    xideFileSyncStop(app);
    xideFileRebindDestroy(app->fileRebind); app->fileRebind=NULL; xrtValueRelease(app->fileRebindReading); app->fileRebindReading=NULL;
    xideRecoveryViewStop(app);
    xideSettingsClear(&app->preferences); free(app->codeFontPath); app->codeFontPath=NULL; free(app->uiFontPath); app->uiFontPath=NULL;
    xideSessionDestroy(app); free(app->projectSessionPath); app->projectSessionPath=NULL;
    xideTasksDestroy(app->tasks); app->tasks=NULL;
    xideImportViewDestroy(app);
    xuiCodeFindScopeDestroy(app->editorFindScope); app->editorFindScope=NULL;
    if (app->context) { xideMessageDestroy(app); xideDialogDestroyAll(app); xideDocumentsClear(&app->documents); xuiDestroy(app->context); }
    xideActionsDestroy(app);
    xideCatalogViewDestroy(app); xideCatalogClear(&app->catalog);
    xideCLanguageViewDestroy(app); xideCCompletionViewDestroy(app); xideCompletionViewDestroy(app); xideLanguageViewDestroy(app); xideToolchainViewDestroy(app); xideFileChangesViewDestroy(app); xideRecoveryViewDestroy(app); xideFontViewDestroy(app); xideGotoViewDestroy(app); xideTemplateViewDestroy(app); xideRenameViewDestroy(app); xideResourceViewDestroy(app); xideReferencesViewDestroy(app); xideConfigurationViewDestroy(app); xideTaskViewDestroy(app); xideWorkspaceViewDestroy(app); xideWorkspaceClear(&app->workspaceModel); free(app->selectedProject); app->selectedProject=NULL;
    if (app->codeFont && app->codeFont != app->uiFont) app->proxy.fontDestroy(&app->proxy, app->codeFont);
    if (app->uiFont) app->proxy.fontDestroy(&app->proxy, app->uiFont);
    if (app->target) app->proxy.surfaceDestroy(&app->proxy, app->target);
    xideDpiProxyDestroy(app->dpiProxy); app->dpiProxy=NULL;
    app->context = NULL; app->target = NULL; app->codeFont = app->uiFont = NULL;
}
int xideAppCapture(xide_app* app,const char* path)
{
    int width=0,height=0; int result=xideDpiSurfaceReadPixels(app->dpiProxy,app->target,NULL,0,&width,&height); if(result!=XUI_OK) return result;
    size_t size=(size_t)width*(size_t)height*4; unsigned char* pixels=malloc(size); if(!pixels) return XUI_ERROR_OUT_OF_MEMORY;
    result=xideDpiSurfaceReadPixels(app->dpiProxy,app->target,pixels,width*4,&width,&height);
    if(result==XUI_OK) {
        result=xgeImageSavePNG(path,width,height,pixels,width*4);
        if(result!=XGE_OK)xideFailureSet(&app->failure,XIDE_FAILURE_RENDER,"XGE",result,"Unable to save the requested rendered image.","xgeImageSavePNG");
    }
    free(pixels); return result;
}
static int frame(void* user)
{
    xide_app* app = user;
    if (!app->context) TRY(xideAppCreate(app));
    TRY_XGE(xgeBegin());
    float scale=effectiveScale(app);
    if(scale!=app->dpiScale) {
        /* Scale and viewport/font changes invalidate these bitmaps. Release them
           before recreating resources so old layouts do not amplify the peak. */
        TRY(xuiPurgeCaches(app->context,0));
        TRY(xideDpiProxySetScale(app->dpiProxy,scale)); app->dpiScale=scale;
        TRY(xuiInputDpi(app->context,scale));
    }
    int width=(int)lroundf(xgeGetWidth()/scale),height=(int)lroundf(xgeGetHeight()/scale);
    if (width != app->viewportWidth || height != app->viewportHeight) {
        /* Release old-layout caches before allocating the new viewport target. */
        TRY(xuiPurgeCaches(app->context,0));
        TRY(createTarget(app, width, height));
        app->viewportWidth = width; app->viewportHeight = height;
        TRY(xuiInputViewport(app->context, (float)width, (float)height));
    }
    TRY(xidePlatformHandleInput(app)); TRY(xuiDispatchPendingEvents(app->context)); xideFileDialogPoll(app); xideMessagePoll(app); xideEditorPoll(app); xideFileSyncPoll(app); xideFileChangesViewPoll(app); xideTaskViewPoll(app); xideLanguageViewPoll(app); xideCLanguageViewPoll(app); xideToolchainViewPoll(app); xideRecoveryViewPoll(app);
    xideWorkbenchLayout(app); TRY(xuiLayout(app->context)); xideCatalogViewLayout(app); TRY(xuiLayout(app->context)); TRY(xuiTerminalFit(app->panels[XIDE_TERMINAL]));
    if(app->initialChapterRequested && app->frames==0) {
        if(!xideHelpJump(xideCatalogHelpDocument(app),(size_t)app->initialChapter)) return XUI_ERROR_INVALID_ARGUMENT;
        app->initialChapterRequested=0;
    }
    xideCatalogViewAfterLayout(app); TRY(xuiLayout(app->context));
    xideSessionAfterLayout(app); xideSessionPoll(app);
    TRY(xuiUpdate(app->context, xgeGetDelta()));
    TRY(xideCatalogViewPrepareRender(app));
    TRY(app->proxy.surfaceClear(&app->proxy, app->target, ideThemeColor(app->theme, IDE_BG)));
    xui_rect_i_t full = {0, 0, width, height};
    TRY(xuiRender(app->context, app->target, &full, 1));
    xgeClear(ideThemeColor(app->theme, IDE_BG));
    TRY(xideDpiSurfacePresent(app->dpiProxy,app->target,xgeGetWidth(),xgeGetHeight()));
    ++app->frames;
    int taskDone=(!app->waitTask || xideTaskViewSettled(app)) && !xideRecoveryViewBusy(app) && xideFileSyncSettled(app);
    if(app->waitTask && app->taskStartMs && GetTickCount64()-app->taskStartMs>30000) { app->taskExit=1; taskDone=1; }
    if (app->maxFrames > 0 && app->frames >= app->maxFrames && taskDone && app->screenshot) TRY(xideAppCapture(app,app->screenshot));
    TRY_XGE(xgeEnd());
    if (app->maxFrames > 0 && app->frames >= app->maxFrames && taskDone) {
    if(app->initialTask) { xide_task_info info={0}; xideTaskInfo(xideTaskViewSelected(app),&info); if(info.state!=XIDE_TASK_SUCCEEDED) app->taskExit=1; printf("Task #%llu: state=%d built=%d ran=%d code=%d tested=%d kind=%d %s\n",(unsigned long long)info.id,info.state,info.built,info.ran,info.exit.Code,info.tested,info.kind,info.error); }
        printf("xlang IDE: rendered %d frames; windows=%d panes=%d; C and xlang registered\n", app->frames,
               xuiDockPanelGetWindowCount(app->dock), xuiDockPanelGetPaneCount(app->dock));
        if(app->sessionRoot && !app->noSession) { xide_model_error error={0}; if(!xideSessionSave(app,0,&error)) { fprintf(stderr,"session snapshot failed: %s\n",error.message); app->frameError=XUI_ERROR; } }
        xgeQuit();
    }
    return XGE_OK;
}
int xideAppFrame(void* user)
{
    xide_app* app = user;
    int result = frame(user);
    if (result != XGE_OK) {
        app->frameError = result;
        /* A handled frame failure bypasses the normal quit callback. Preserve
           edits still inside the periodic checkpoint debounce before teardown. */
        xideFailureSet(&app->failure,app->frames ? XIDE_FAILURE_RENDER : XIDE_FAILURE_STARTUP,"XGE",result,"Unable to continue the IDE interface.","xideAppFrame");
        xideAppFailureRecovery(app);
        xgeQuit();
    }
    return result;
}
void xideAppFailureRecovery(xide_app* app)
{
    if(app->failure.recovery!=XIDE_FAILURE_RECOVERY_UNAVAILABLE)return;
    if(app->noSession) { xideFailureRecovery(&app->failure,XIDE_FAILURE_RECOVERY_DISABLED,NULL);return; }
    if(xideSessionCanAttemptCheckpoint(app)) {
        xide_model_error error={0};
        if(xideSessionSave(app,0,&error))xideFailureRecovery(&app->failure,XIDE_FAILURE_RECOVERY_SAVED,NULL);
        else { xideFailureRecovery(&app->failure,XIDE_FAILURE_RECOVERY_FAILED,error.message);fprintf(stderr,"recovery checkpoint after frame failure: %s\n",error.message); }
    }
}
