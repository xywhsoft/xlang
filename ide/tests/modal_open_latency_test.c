#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/about_view.h"
#include "ui/file_dialog.h"
#include "ui/message_box.h"
#include "ui/dialog.h"
#include <windows.h>
#include <stdio.h>

/* Real workbench on an owned desktop. Invalidation counts are assertions;
 * wall-clock measurements are evidence, not machine-dependent deadlines.
 * No IDE-side batching hides XGE registration bugs. */
static LARGE_INTEGER frequency;
static int started, failed;

static double milliseconds(void)
{
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return 1000.0 * (double)value.QuadPart / (double)frequency.QuadPart;
}

static int frame(xide_app* app, const char* name, int index)
{
    double begin = milliseconds();
    int status = xideAppFrame(app);
    xui_render_stats_t stats = {0};
    xui_layout_stats_t layout = {0};
    stats.iSize = sizeof(stats);
    layout.iSize = sizeof(layout);
    xuiGetRenderStats(app->context, &stats);
    xuiGetLayoutStats(app->context, &layout);
    printf("FRAME,%s,%d,%.6f,%d,%d,%d,%d,%d,%llu,%d\n",
           name, index, milliseconds() - begin, status,
           stats.iUpdatedWidgets, stats.iUpdatedCaches,
           stats.iPrepareVisitedWidgets, stats.iRenderNodesUpdated,
           (unsigned long long)stats.iDirtyPropagationSteps, layout.iPassCount);
    fflush(stdout);
    if (status) failed = 1;
    return status;
}

static int recordOpen(xide_app* app, const char* name, int round,
                      double begin, int opened, uint32_t before,
                      uint32_t allowedChanges)
{
    uint32_t changes = xuiStyleGetGeneration(app->context) - before;
    printf("OPEN,%s,%d,%.6f,%d,%u\n",
           name, round, milliseconds() - begin, opened, changes);
    fflush(stdout);
    if (!opened || changes > allowedChanges) {
        fprintf(stderr, "%s round %d: opened=%d, style changes=%u, allowed=%u\n",
                name, round, opened, changes, allowedChanges);
        failed = 1;
        return 0;
    }
    for (int i = 0; i < 5; ++i)
        if (frame(app, name, round * 5 + i)) return 0;
    return 1;
}

static int verify(void* raw)
{
    if (started) return 0;
    started = 1;
    const char* folder = raw;
    char settings[4096];
    snprintf(settings, sizeof(settings), "%s/settings.json", folder);
    xide_app app = {0};
    app.hidden = 1;
    app.noSession = 1;
    app.disableAutomaticDiagnostics = 1;
    app.settingsPath = settings;
    double begin = milliseconds();
    int status = xideAppCreate(&app);
    printf("INIT,%.6f,%d\n", milliseconds() - begin, status);
    if (status) { failed = 1; goto cleanup; }
    for (int i = 0; i < 5; ++i)
        if (frame(&app, "baseline", i)) goto cleanup;
    app.hidden = 0;

    for (int round = 0; round < 3; ++round) {
        uint32_t before = xuiStyleGetGeneration(app.context);
        begin = milliseconds();
        int opened = xideAboutViewOpen(&app);
        if (!recordOpen(&app, "about", round, begin, opened, before, 0)) goto cleanup;
        xideDialogSetOpen(xideAboutViewWindow(&app), 0);
        if (frame(&app, "closed-about", round)) goto cleanup;

        before = xuiStyleGetGeneration(app.context);
        begin = milliseconds();
        xideMessageShow(&app, "性能诊断", "这是消息框性能诊断。\nThis is a short diagnostic message.",
                        XUI_MSGBOX_ICON_INFO);
        opened = xideMessageCurrent(&app) != NULL;
        if (!recordOpen(&app, "msgbox", round, begin, opened, before, 1)) goto cleanup;
        xideMessageDestroy(&app);
        if (frame(&app, "closed-msgbox", round)) goto cleanup;

        before = xuiStyleGetGeneration(app.context);
        begin = milliseconds();
        opened = xideFileDialogChoose(&app, NULL, XUI_FILE_DIALOG_MODE_OPEN_FILE,
                                      "文件性能诊断", "*", folder, NULL, NULL, NULL);
        if (!recordOpen(&app, "file", round, begin, opened, before, 1)) goto cleanup;
        xideFileDialogDestroy(&app);
        if (frame(&app, "closed-file", round)) goto cleanup;
    }

    {
        uint32_t before = xuiStyleGetGeneration(app.context);
        begin = milliseconds();
        xui_widget_type cached = xuiWidgetFindType(app.context, "window");
        printf("PROBE,find-existing-type,%.6f,%u,%d\n", milliseconds() - begin,
               xuiStyleGetGeneration(app.context) - before, cached != NULL);
        begin = milliseconds();
        xui_widget_type queried = xuiWindowGetType(app.context);
        uint32_t changes = xuiStyleGetGeneration(app.context) - before;
        printf("PROBE,get-existing-type,%.6f,%u,%d\n", milliseconds() - begin,
               changes, queried == cached);
        if (!cached || queried != cached || changes) failed = 1;
    }
cleanup:
    xideAppDestroy(&app);
    xgeQuit();
    return 0;
}

typedef struct fixture {
    const char* folder;
    HDESK desktop;
} fixture;

static DWORD WINAPI worker(void* raw)
{
    fixture* test = raw;
    if (!SetThreadDesktop(test->desktop)) return 1;
    xge_desc_t desc = {0};
    desc.iWidth = 1280;
    desc.iHeight = 900;
    desc.sTitle = "IDE modal registration regression";
    desc.iFlags = XGE_INIT_WINDOW;
    int status = xgeInit(&desc);
    if (!status) status = xgeRun(verify, (void*)test->folder);
    xgeUnit();
    return status || failed || !started;
}

int main(int argc, char** argv)
{
    if (argc != 2 || !xrtDirCreateAll(argv[1])) return 2;
    QueryPerformanceFrequency(&frequency);
    wchar_t name[80];
    swprintf(name, 80, L"XlangModalRegression-%lu", GetCurrentProcessId());
    fixture test = {.folder = argv[1]};
    test.desktop = CreateDesktopW(name, NULL, NULL, 0,
        DESKTOP_CREATEWINDOW | DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS | DESKTOP_ENUMERATE, NULL);
    if (!test.desktop) return 1;
    HANDLE thread = CreateThread(NULL, 0, worker, &test, 0, NULL);
    DWORD result = 1;
    if (thread) {
        /* Never destroy a live UI thread's desktop on timeout. */
        if (WaitForSingleObject(thread, 180000) != WAIT_OBJECT_0) ExitProcess(1);
        GetExitCodeThread(thread, &result);
        CloseHandle(thread);
    }
    CloseDesktop(test.desktop);
    if (!result)
        puts("modal_open_latency_test passed: unchanged About generation, one MsgBox construction "
             "transaction, reuse and existing-type lookup; timing has no machine-dependent deadline");
    return (int)result;
}
