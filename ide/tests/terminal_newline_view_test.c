#include "app/xide_app.h"
#include "ui/task_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include "platform/settings.h"
#include "theme/theme.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "terminal_newline_view_test:%d: %s (%s / %s)\n", __LINE__, #x, error.message, info.error); failed=1; goto cleanup; } } while (0)
static int writeText(const char* path,const char* text)
{ return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }

int main(int argc,char** argv)
{
    int failed=0,checks=0,column=0,row=0,finished=0;
    xide_app app={0};xide_settings preferences={0};xide_model_error error={0};xide_task_info info={0};
    char *root=NULL,*project=NULL,*entry=NULL,*settings=NULL;
    CHECK(argc==3);
    root=xidePathResolve(argv[1],"real-pipe-newline");CHECK(root && xrtDirCreateAll(root));
    project=xidePathResolve(root,"test.xlp");entry=xidePathResolve(root,"main.xl");settings=xidePathResolve(root,"settings.json");CHECK(project && entry && settings);
    CHECK(writeText(project,"{\"format\":\"xlang-project-1\",\"kind\":\"exe\",\"entry\":\"main.xl\"}"));
    CHECK(writeText(entry,"print(\"Hello, World!\")\nprintln(\"xlang\", 6, true)\nfunc main(...) as int\n print(\"程序启动\")\n return 0\nend\n"));
    CHECK(writeText(settings,"{\"format\":\"xlang-ide-settings-1\"}"));
    CHECK(xideSettingsLoad(&preferences,settings,&error) && xideSettingsSetCompiler(&preferences,argv[2],&error));
    app.settingsPath=settings;app.hidden=1;app.viewportWidth=1280;app.viewportHeight=800;
    CHECK(xuiCreate(&app.context)==XUI_OK && ideThemeInit(app.context,IDE_THEME_CLASSIC)==XUI_OK && xuiInputViewport(app.context,1280,800)==XUI_OK && xideWorkbenchCreate(&app)==XUI_OK);
    CHECK(xuiTerminalGetNewlineMode(app.panels[XIDE_TERMINAL])==XUI_TERMINAL_NEWLINE_TEXT);
    CHECK(xideEditorOpen(&app,entry,&error));
    xideDispatchAction(&app,XIDE_ACTION_RUN_FILE);
    xide_task* task=xideTaskViewSelected(&app);CHECK(task);
    ULONGLONG start=GetTickCount64();
    do {
        xideTaskViewPoll(&app);xuiDispatchPendingEvents(app.context);xideTaskInfo(task,&info);
        if(info.phase==XIDE_TASK_DONE && xideTaskViewSettled(&app)) { finished=1;break; }
        Sleep(5);
    } while(GetTickCount64()-start<60000);
    CHECK(finished && info.state==XIDE_TASK_SUCCEEDED && info.ran);
    CHECK(xuiTerminalFlush(app.panels[XIDE_TERMINAL])==XUI_OK);
    CHECK(xuiTerminalGetCursor(app.panels[XIDE_TERMINAL],&column,&row)==XUI_OK && column==8 && row==1);
    xui_terminal_cell_t cell={0};
    CHECK(xuiTerminalGetCell(app.panels[XIDE_TERMINAL],0,1,&cell)==XUI_OK && cell.iCodepoint==0x7a0b);
    char output[1024]={0};uint64_t cursor=0,dropped=0;
    size_t count=xideTaskReadTerminal(task,&cursor,output,sizeof(output)-1,&dropped);
    CHECK(count && !dropped && !strcmp(output,"Hello, World!xlang 6 true\n程序启动"));
    CHECK(strchr(output,'\r')==NULL);
    printf("terminal_newline_view_test passed: %d checks; actual xlang build/run, raw LF pipe preserved, Chinese starts at column 0, cursor=(8,1).\n",checks);
cleanup:
    xideAppDestroy(&app);xideSettingsClear(&preferences);free(root);free(project);free(entry);free(settings);
    return failed;
}
