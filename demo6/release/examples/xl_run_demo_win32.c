#include "include/xl.h"

#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define IDC_MODE_XLANG 1001
#define IDC_MODE_C 1002
#define IDC_RUN 1003
#define IDC_SAMPLE_XLANG 1004
#define IDC_SAMPLE_C 1005
#define IDC_INPUT 1006
#define IDC_OUTPUT 1007

static HWND g_input;
static HWND g_output;
static HWND g_modeXlang;
static HWND g_modeC;
static HWND g_run;
static HWND g_sampleXlang;
static HWND g_sampleC;

typedef struct CaptureState {
    int oldStdout;
    int oldStderr;
    FILE* file;
    char path[MAX_PATH];
} CaptureState;

static const char* g_xlangSample =
    "println(\"hello from xlang\")\r\n"
    "int value = 40 + 2\r\n"
    "println(value)\r\n";

static const char* g_cSample =
    "#include <stdio.h>\r\n"
    "int main(int argc, char** argv) {\r\n"
    "    printf(\"hello from C\\\\n\");\r\n"
    "    printf(\"%d\\\\n\", 40 + 2);\r\n"
    "    return 0;\r\n"
    "}\r\n";

static char* dup_text(const char* text)
{
    size_t len;
    char* copy;

    if ( text == NULL ) {
        text = "";
    }
    len = strlen(text);
    copy = (char*)malloc(len + 1);
    if ( copy != NULL ) {
        memcpy(copy, text, len + 1);
    }
    return copy;
}

static char* read_capture_file(FILE* file)
{
    long size;
    char* text;

    fflush(file);
    if ( fseek(file, 0, SEEK_END) != 0 ) {
        return dup_text("");
    }
    size = ftell(file);
    if ( size < 0 ) {
        return dup_text("");
    }
    if ( fseek(file, 0, SEEK_SET) != 0 ) {
        return dup_text("");
    }
    text = (char*)malloc((size_t)size + 1);
    if ( text == NULL ) {
        return NULL;
    }
    if ( size > 0 ) {
        fread(text, 1, (size_t)size, file);
    }
    text[size] = '\0';
    return text;
}

static int capture_begin(CaptureState* cap)
{
    char tempDir[MAX_PATH];
    UINT got;

    memset(cap, 0, sizeof(*cap));
    cap->oldStdout = -1;
    cap->oldStderr = -1;
    got = GetTempPathA(sizeof(tempDir), tempDir);
    if ( got == 0 || got >= sizeof(tempDir) ) {
        return 0;
    }
    if ( GetTempFileNameA(tempDir, "xld", 0, cap->path) == 0 ) {
        return 0;
    }
    cap->file = fopen(cap->path, "w+b");
    if ( cap->file == NULL ) {
        DeleteFileA(cap->path);
        cap->path[0] = '\0';
        return 0;
    }
    fflush(stdout);
    fflush(stderr);
    cap->oldStdout = _dup(_fileno(stdout));
    cap->oldStderr = _dup(_fileno(stderr));
    if ( cap->oldStdout < 0 || cap->oldStderr < 0 ||
         _dup2(_fileno(cap->file), _fileno(stdout)) != 0 ||
         _dup2(_fileno(cap->file), _fileno(stderr)) != 0 ) {
        return 0;
    }
    return 1;
}

static char* capture_end(CaptureState* cap)
{
    char* text;

    fflush(stdout);
    fflush(stderr);
    if ( cap->oldStdout >= 0 ) {
        _dup2(cap->oldStdout, _fileno(stdout));
        _close(cap->oldStdout);
        cap->oldStdout = -1;
    }
    if ( cap->oldStderr >= 0 ) {
        _dup2(cap->oldStderr, _fileno(stderr));
        _close(cap->oldStderr);
        cap->oldStderr = -1;
    }
    text = cap->file != NULL ? read_capture_file(cap->file) : dup_text("");
    if ( cap->file != NULL ) {
        fclose(cap->file);
        cap->file = NULL;
    }
    if ( cap->path[0] != '\0' ) {
        DeleteFileA(cap->path);
        cap->path[0] = '\0';
    }
    return text;
}

static char* read_edit_text(HWND edit)
{
    int len;
    char* text;

    len = GetWindowTextLengthA(edit);
    text = (char*)malloc((size_t)len + 1);
    if ( text == NULL ) {
        return NULL;
    }
    GetWindowTextA(edit, text, len + 1);
    return text;
}

static int text_contains(const char* text, const char* needle)
{
    return text != NULL && needle != NULL && strstr(text, needle) != NULL;
}

static int run_source_capture(int isC, const char* source, char** outCaptured, char** outError)
{
    XL_ContextOptions options;
    XL_Context* ctx;
    CaptureState cap;
    int captureOk;
    int ok;

    if ( outCaptured != NULL ) {
        *outCaptured = NULL;
    }
    if ( outError != NULL ) {
        *outError = NULL;
    }
    options = xlDefaultContextOptions();
    options.workDir = ".";
    options.tempDir = "release\\tmp\\xl_run_demo";
    ctx = xlCreateContext(&options);
    if ( ctx == NULL ) {
        if ( outCaptured != NULL ) {
            *outCaptured = dup_text("");
        }
        if ( outError != NULL ) {
            *outError = dup_text("cannot create xl context");
        }
        return 0;
    }

    captureOk = capture_begin(&cap);
    ok = isC ? xlRunCSourceString(ctx, source, 0, NULL) : xlRunScriptString(ctx, source);
    if ( outCaptured != NULL ) {
        *outCaptured = captureOk ? capture_end(&cap) : dup_text("(stdout capture unavailable)\r\n");
    } else if ( captureOk ) {
        char* ignored = capture_end(&cap);
        free(ignored);
    }
    if ( outError != NULL ) {
        *outError = dup_text(xlGetLastError(ctx));
    }
    xlDestroyContext(ctx);
    return ok;
}

static void set_output_text(int ok, const char* captured, const char* error)
{
    const char* status;
    const char* body;
    const char* err;
    size_t len;
    char* text;

    status = ok ? "status: ok\r\n" : "status: failed\r\n";
    body = captured != NULL && captured[0] != '\0' ? captured : "(no stdout/stderr)\r\n";
    err = error != NULL && error[0] != '\0' ? error : "";
    len = strlen(status) + strlen(body) + strlen(err) + 64;
    text = (char*)malloc(len);
    if ( text == NULL ) {
        SetWindowTextA(g_output, "out of memory");
        return;
    }
    if ( err[0] != '\0' ) {
        snprintf(text, len, "%s\r\n%s\r\nerror:\r\n%s\r\n", status, body, err);
    } else {
        snprintf(text, len, "%s\r\n%s", status, body);
    }
    SetWindowTextA(g_output, text);
    free(text);
}

static void run_current_source(HWND hwnd)
{
    char* source;
    char* captured;
    char* error;
    int ok;

    source = read_edit_text(g_input);
    if ( source == NULL ) {
        SetWindowTextA(g_output, "cannot read input");
        return;
    }
    captured = NULL;
    error = NULL;
    ok = run_source_capture(IsDlgButtonChecked(hwnd, IDC_MODE_C) == BST_CHECKED, source, &captured, &error);
    set_output_text(ok, captured, error);
    free(captured);
    free(error);
    free(source);
}

static int run_self_test(void)
{
    const char* xlangSource =
        "println(\"gui xlang ok\")\r\n"
        "println(21 + 21)\r\n";
    const char* cSource =
        "#include <stdio.h>\r\n"
        "int main(int argc, char** argv) {\r\n"
        "    printf(\"gui C ok\\n\");\r\n"
        "    printf(\"%d\\n\", 21 + 21);\r\n"
        "    return 0;\r\n"
        "}\r\n";
    char* captured;
    char* error;
    int ok;

    captured = NULL;
    error = NULL;
    ok = run_source_capture(0, xlangSource, &captured, &error);
    if ( !ok || !text_contains(captured, "gui xlang ok") || !text_contains(captured, "42") ) {
        free(captured);
        free(error);
        return 1;
    }
    free(captured);
    free(error);

    captured = NULL;
    error = NULL;
    ok = run_source_capture(1, cSource, &captured, &error);
    if ( !ok || !text_contains(captured, "gui C ok") || !text_contains(captured, "42") ) {
        free(captured);
        free(error);
        return 2;
    }
    free(captured);
    free(error);
    return 0;
}

static void layout(HWND hwnd)
{
    RECT rc;
    int width;
    int height;
    int gap;
    int top;
    int editTop;
    int editHeight;
    int leftWidth;
    int rightWidth;

    GetClientRect(hwnd, &rc);
    width = rc.right - rc.left;
    height = rc.bottom - rc.top;
    gap = 8;
    top = 8;
    editTop = 48;
    editHeight = height - editTop - gap;
    leftWidth = (width - gap * 3) / 2;
    rightWidth = width - leftWidth - gap * 3;

    MoveWindow(g_modeXlang, gap, top, 72, 26, TRUE);
    MoveWindow(g_modeC, gap + 78, top, 48, 26, TRUE);
    MoveWindow(g_run, gap + 134, top, 80, 28, TRUE);
    MoveWindow(g_sampleXlang, gap + 224, top, 104, 28, TRUE);
    MoveWindow(g_sampleC, gap + 336, top, 88, 28, TRUE);
    MoveWindow(g_input, gap, editTop, leftWidth, editHeight, TRUE);
    MoveWindow(g_output, gap * 2 + leftWidth, editTop, rightWidth, editHeight, TRUE);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch ( msg ) {
    case WM_CREATE:
        g_modeXlang = CreateWindowA("BUTTON", "XLang", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                   0, 0, 0, 0, hwnd, (HMENU)IDC_MODE_XLANG, NULL, NULL);
        g_modeC = CreateWindowA("BUTTON", "C", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                0, 0, 0, 0, hwnd, (HMENU)IDC_MODE_C, NULL, NULL);
        g_run = CreateWindowA("BUTTON", "Run", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              0, 0, 0, 0, hwnd, (HMENU)IDC_RUN, NULL, NULL);
        g_sampleXlang = CreateWindowA("BUTTON", "XLang sample", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      0, 0, 0, 0, hwnd, (HMENU)IDC_SAMPLE_XLANG, NULL, NULL);
        g_sampleC = CreateWindowA("BUTTON", "C sample", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  0, 0, 0, 0, hwnd, (HMENU)IDC_SAMPLE_C, NULL, NULL);
        g_input = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_xlangSample,
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                     ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                                 0, 0, 0, 0, hwnd, (HMENU)IDC_INPUT, NULL, NULL);
        g_output = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                      ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                                  0, 0, 0, 0, hwnd, (HMENU)IDC_OUTPUT, NULL, NULL);
        CheckDlgButton(hwnd, IDC_MODE_XLANG, BST_CHECKED);
        layout(hwnd);
        return 0;
    case WM_SIZE:
        layout(hwnd);
        return 0;
    case WM_COMMAND:
        switch ( LOWORD(wParam) ) {
        case IDC_RUN:
            run_current_source(hwnd);
            return 0;
        case IDC_SAMPLE_XLANG:
            CheckDlgButton(hwnd, IDC_MODE_XLANG, BST_CHECKED);
            SetWindowTextA(g_input, g_xlangSample);
            return 0;
        case IDC_SAMPLE_C:
            CheckDlgButton(hwnd, IDC_MODE_C, BST_CHECKED);
            SetWindowTextA(g_input, g_cSample);
            return 0;
        default:
            break;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, int showCmd)
{
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;

    (void)prevInstance;
    if ( cmdLine != NULL && strstr(cmdLine, "--self-test") != NULL ) {
        return run_self_test();
    }
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = "XLRunDemoWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if ( !RegisterClassA(&wc) ) {
        return 1;
    }
    hwnd = CreateWindowA("XLRunDemoWindow",
                         "xlang dynamic run demo",
                         WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         CW_USEDEFAULT,
                         CW_USEDEFAULT,
                         960,
                         640,
                         NULL,
                         NULL,
                         instance,
                         NULL);
    if ( hwnd == NULL ) {
        return 1;
    }
    ShowWindow(hwnd, showCmd);
    UpdateWindow(hwnd);
    while ( GetMessageA(&msg, NULL, 0, 0) > 0 ) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}

#ifndef XL6_GUI_ENTRY_ONLY
int main(void)
{
    return run_self_test();
}
#endif
