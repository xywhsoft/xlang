#include "app/xide_app.h"
#include "theme/theme.h"
#include "platform/startup_alias.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shellapi.h>
#include <math.h>
static int parseArguments(xide_app* app, int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help")) {
            puts("xlang IDE development workbench\n  --size WIDTH HEIGHT  initial window size\n  --dpi-scale N  diagnostic render scale (0.5-8)\n  --frames N  bounded render smoke test\n  --hidden    hide this program's smoke-test window\n  --screenshot PATH.png  capture the final bounded frame\n  --workspace PATH.xlw  open a workspace\n  --project PATH.xlp    open a standalone project\n  --open-file PATH  open an editable UTF-8 file\n  --select-project ID   initially selected project\n  --demo xlang|c  initially selected syntax example\n  --catalog PATH.json  use an explicit command catalog\n  --command ID         initially selected command help\n  --help-chapter N      initially scroll to a help chapter (0-based)\n  --command-search TEXT filter command catalog\n  --compiler PATH.exe  select xlang toolchain\n  --c-server PATH.exe  select clangd for C completion\n  --settings PATH.json  use an explicit preferences file\n  --session-root PATH  isolate session and recovery storage\n  --diagnostic-root PATH  isolate fatal error reports\n  --no-session         disable session restore and persistence\n  --configuration ID   select project build configuration\n  --font-settings open code font preferences\n  --ui-font-settings open interface font preferences\n  --recovery-manager open session and recovery manager\n  --edit-configurations open selected project configuration editor\n  --task build|run|test     explicitly start a project task\n  --wait-task          wait up to 30s before bounded render exits\n  --theme classic-light|graphite-dark|warm-white|ocean"); return 1;
        }
        if (!strcmp(argv[i], "--hidden")) app->hidden = 1;
        else if(!strcmp(argv[i],"--dpi-scale") && i+1<argc) { char* end; float scale=strtof(argv[++i],&end); if(*end || !isfinite(scale) || scale<0.5f || scale>8) return -1; app->dpiOverride=scale; }
        else if (!strcmp(argv[i], "--size") && i + 2 < argc) { char* end; long width=strtol(argv[++i],&end,10); if(*end || width<640 || width>7680) return -1; long height=strtol(argv[++i],&end,10); if(*end || height<480 || height>4320) return -1; app->viewportWidth=(int)width; app->viewportHeight=(int)height; app->windowSizeExplicit=1; }
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            char* end; long frames = strtol(argv[++i], &end, 10);
            if (*end || frames < 1 || frames > 10000) return -1;
            app->maxFrames = (int)frames;
        } else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) app->screenshot = argv[++i];
        else if (!strcmp(argv[i], "--demo") && i + 1 < argc) {
            const char* demo = argv[++i];
            if (!strcmp(demo, "c")) app->initialEditor = 1;
            else if (strcmp(demo, "xlang")) return -1;
        } else if (!strcmp(argv[i], "--diagnostic-root") && i + 1 < argc) app->diagnosticRoot=argv[++i];
        else if (!strcmp(argv[i], "--workspace") && i + 1 < argc) app->workspacePath=argv[++i];
        else if (!strcmp(argv[i], "--project") && i + 1 < argc) app->projectPath=argv[++i];
        else if ((!strcmp(argv[i], "--open-file") || !strcmp(argv[i], "--preview-file")) && i + 1 < argc) app->initialFile=argv[++i];
        else if (!strcmp(argv[i], "--select-project") && i + 1 < argc) app->initialSelectedProject=argv[++i];
        else if (!strcmp(argv[i], "--catalog") && i + 1 < argc) app->catalogPath=argv[++i];
        else if (!strcmp(argv[i], "--command") && i + 1 < argc) app->initialCommand=argv[++i];
        else if (!strcmp(argv[i], "--command-search") && i + 1 < argc) app->initialCommandSearch=argv[++i];
        else if (!strcmp(argv[i], "--help-chapter") && i + 1 < argc) { char* end; long chapter=strtol(argv[++i],&end,10); if(*end || chapter<0 || chapter>4096) return -1; app->initialChapter=(int)chapter; app->initialChapterRequested=1; }
        else if (!strcmp(argv[i], "--c-server") && i + 1 < argc) app->cServerPath=argv[++i];
        else if (!strcmp(argv[i], "--compiler") && i + 1 < argc) app->compilerPath=argv[++i];
        else if (!strcmp(argv[i], "--session-root") && i + 1 < argc) app->sessionRoot=argv[++i];
        else if (!strcmp(argv[i], "--no-session")) app->noSession=1;
        else if (!strcmp(argv[i], "--settings") && i + 1 < argc) app->settingsPath=argv[++i];
        else if (!strcmp(argv[i], "--configuration") && i + 1 < argc) app->configurationId=argv[++i];
        else if (!strcmp(argv[i], "--recovery-manager")) app->initialRecovery=1;
        else if (!strcmp(argv[i], "--ui-font-settings")) app->initialUiFontSettings=1;
        else if (!strcmp(argv[i], "--font-settings")) app->initialFontSettings=1;
        else if (!strcmp(argv[i], "--edit-configurations")) app->initialConfigurations=1;
        else if (!strcmp(argv[i], "--wait-task")) app->waitTask=1;
        else if (!strcmp(argv[i], "--task") && i + 1 < argc) { const char* task=argv[++i]; if(!strcmp(task,"build")) app->initialTask=XIDE_ACTION_BUILD; else if(!strcmp(task,"run")) app->initialTask=XIDE_ACTION_RUN; else if(!strcmp(task,"test")) app->initialTask=XIDE_ACTION_TEST; else return -1; }
        else if (!strcmp(argv[i], "--theme") && i + 1 < argc) {
            int theme = ideThemeFind(argv[++i]); if (theme < 0) return -1; app->theme = theme; app->themeExplicit=1;
        } else return -1;
    }
    if ((app->hidden || app->screenshot) && app->maxFrames == 0) return -1;
    if(app->workspacePath && app->projectPath) return -1;
    if(app->waitTask && (!app->initialTask || !app->maxFrames)) return -1;
    return 0;
}
static char** utf8Arguments(int* count)
{
    wchar_t** wide=CommandLineToArgvW(GetCommandLineW(),count); if(!wide) return NULL;
    char** args=calloc((size_t)*count,sizeof(*args));
    for(int i=0;args && i<*count;++i) {
        int size=WideCharToMultiByte(CP_UTF8,0,wide[i],-1,NULL,0,NULL,NULL); args[i]=size ? malloc((size_t)size) : NULL;
        if(!args[i]) { for(int j=0;j<i;++j) free(args[j]); free(args); args=NULL; break; }
        WideCharToMultiByte(CP_UTF8,0,wide[i],-1,args[i],size,NULL,NULL);
    }
    LocalFree(wide); return args;
}
int main(void)
{
    xide_app app = {0};int argc=0,automated=0,exitCode=0;char** argv=utf8Arguments(&argc);
    if(!argv) { xideFailureSet(&app.failure,XIDE_FAILURE_ARGUMENTS,"Win32",ERROR_NOT_ENOUGH_MEMORY,"Unable to read UTF-8 startup arguments.","utf8Arguments"); exitCode=1;goto report; }
    /* Detect automation and its isolated log directory even when parsing stops
       at an earlier invalid argument. No dialog is allowed to stall a gate. */
    for(int i=1;i<argc;++i) {
        if(!strcmp(argv[i],"--hidden") || !strcmp(argv[i],"--frames"))automated=1;
        if(!strcmp(argv[i],"--diagnostic-root") && i+1<argc)app.diagnosticRoot=argv[++i];
    }
    int arguments = parseArguments(&app, argc, argv);
    if(arguments!=0) {
        if(arguments<0) { xideFailureSet(&app.failure,XIDE_FAILURE_ARGUMENTS,"arguments",2,"Invalid arguments; use --help.","parseArguments"); fputs("Invalid arguments; use --help.\n",stderr); exitCode=2; }
    } else {
        int aliasExit=0;unsigned long aliasError=0;int alias=xideStartupAliasMaybe(&aliasExit,&aliasError);
        if(alias>0) {exitCode=aliasExit;goto report;}
        if(alias<0) {xideFailureSet(&app.failure,XIDE_FAILURE_WINDOW,"Win32",(int)aliasError,"Unable to prepare a verified short startup path for the IDE.","startupAlias");exitCode=1;goto report;}
        xge_desc_t desc = {0}; desc.iWidth = app.viewportWidth ? app.viewportWidth : 1280; desc.iHeight = app.viewportHeight ? app.viewportHeight : 800; desc.sTitle = "xlang IDE";
        desc.iFlags = XGE_INIT_WINDOW | XGE_INIT_VSYNC | XGE_INIT_HIGHDPI;
        app.highDpi=1;
        if (!app.maxFrames) desc.iFlags |= XGE_INIT_ON_DEMAND;
        desc.iRunMode = XGE_RUN_GAME_LOOP; desc.iTargetFPS = app.maxFrames ? 60 : 30;
        int result = xgeInit(&desc);
        if(result!=XGE_OK)xideFailureSet(&app.failure,XIDE_FAILURE_WINDOW,"XGE",result,"Unable to initialize the window and graphics environment.","xgeInit");
        if (result == XGE_OK) { xgeRenderRequest(); result = xgeRun(xideAppFrame, &app); }
        if (app.frameError) result = app.frameError;
        if (result == XGE_OK && (app.frames == 0 || (app.maxFrames && app.frames < app.maxFrames))) result = XGE_ERROR;
        if (result != XGE_OK) {
            xideFailureSet(&app.failure,app.frames ? XIDE_FAILURE_RENDER : XIDE_FAILURE_STARTUP,"XGE",result,"The IDE stopped before rendering completed.","xgeRun");
            xideAppFailureRecovery(&app);fprintf(stderr, "xlang IDE startup/render failed: %d\n", result);
        }
        xideAppDestroy(&app); xgeSetQuitRequestCallback(NULL,NULL); xgeUnit(); exitCode=result==XGE_OK && !app.taskExit ? 0 : 1;
    }
report:
    if(app.failure.kind) {
        if(app.noSession)xideFailureRecovery(&app.failure,XIDE_FAILURE_RECOVERY_DISABLED,NULL);
        fprintf(stderr,"xlang IDE failure: stage=%d domain=%s code=%d recovery=%d detail=%s\n",app.failure.kind,app.failure.domain,app.failure.code,app.failure.recovery,app.failure.detail);
        xide_failure_report diagnostic={0};
        if(!automated || app.diagnosticRoot) {
            if(xideFailureWrite(&app.failure,app.diagnosticRoot,app.frames,&diagnostic))fprintf(stderr,"xlang IDE diagnostic: %s\n",diagnostic.path);
            else fprintf(stderr,"xlang IDE diagnostic could not be written: %lu\n",diagnostic.error);
        }
#ifdef XIDE_GUI
        if(!automated)xideFailureShow(&app.failure,&diagnostic);
#endif
    }
    for(int i=0;argv && i<argc;++i) free(argv[i]);
    free(argv);
    return exitCode;
}
