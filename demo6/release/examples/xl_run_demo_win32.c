#include "include/xl.h"

#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdint.h>
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
    int oldFd[2];
    HANDLE oldHandle[2];
    int handleWasCrt[2];
    int redirectStarted;
    FILE* file;
    WCHAR path[MAX_PATH];
} CaptureState;

static const char* g_xlangSample =
    "println(\"hello from xlang\")\r\n"
    "int value = 40 + 2\r\n"
    "println(value)\r\n";

static const char* g_cSample =
    "#include <stdio.h>\r\n"
    "int main(int argc, char** argv) {\r\n"
    "    printf(\"hello from C\\n\");\r\n"
    "    printf(\"%d\\n\", 40 + 2);\r\n"
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
    size_t read=size>0?fread(text,1,(size_t)size,file):0;
    text[read] = '\0';
    return text;
}

static char* capture_end(CaptureState* cap);

static int capture_begin(CaptureState* cap)
{
    WCHAR tempDir[MAX_PATH];
    FILE* streams[]={stdout,stderr};
    DWORD ids[]={STD_OUTPUT_HANDLE,STD_ERROR_HANDLE};
    UINT got;

    memset(cap, 0, sizeof(*cap));
    for(int i=0;i<2;++i){cap->oldFd[i]=-1;cap->oldHandle[i]=GetStdHandle(ids[i]);}
    got = GetTempPathW(MAX_PATH, tempDir);
    if ( got == 0 || got >= MAX_PATH ) {
        return 0;
    }
    if ( GetTempFileNameW(tempDir, L"xld", 0, cap->path) == 0 ) {
        return 0;
    }
    cap->file = _wfopen(cap->path, L"w+b");
    if ( cap->file == NULL ) {
        DeleteFileW(cap->path);
        cap->path[0] = '\0';
        return 0;
    }
    fflush(stdout);
    fflush(stderr);
    for(int i=0;i<2;++i) {
        int fd=_fileno(streams[i]);
        if(fd<0)continue;
        cap->handleWasCrt[i]=cap->oldHandle[i]==(HANDLE)_get_osfhandle(fd);
        cap->oldFd[i]=_dup(fd);
        if(cap->oldFd[i]<0)goto failed;
    }
    /* GUI processes may start with disconnected CRT streams. Both CRT and
     * Win32 output paths must target the capture file. */
    cap->redirectStarted=1;
    for(int i=0;i<2;++i) {
        if(_fileno(streams[i])<0 && !freopen("NUL","w",streams[i]))goto failed;
        if(_dup2(_fileno(cap->file),_fileno(streams[i]))!=0)goto failed;
        if(!SetStdHandle(ids[i],(HANDLE)_get_osfhandle(_fileno(cap->file))))goto failed;
    }
    return 1;
failed:
    {char* ignored=capture_end(cap);free(ignored);}
    return 0;
}

static char* capture_end(CaptureState* cap)
{
    char* text;
    FILE* streams[]={stdout,stderr};
    DWORD ids[]={STD_OUTPUT_HANDLE,STD_ERROR_HANDLE};

    fflush(stdout);
    fflush(stderr);
    for(int i=0;i<2;++i) {
        if(cap->redirectStarted) {
            if(cap->oldFd[i]>=0)_dup2(cap->oldFd[i],_fileno(streams[i]));
            else (void)freopen("NUL","w",streams[i]);
            HANDLE restored=cap->handleWasCrt[i]?(HANDLE)_get_osfhandle(_fileno(streams[i])):cap->oldHandle[i];
            (void)SetStdHandle(ids[i],restored);
        }
        if(cap->oldFd[i]>=0){_close(cap->oldFd[i]);cap->oldFd[i]=-1;}
    }
    text = cap->file != NULL ? read_capture_file(cap->file) : dup_text("");
    if ( cap->file != NULL ) {
        fclose(cap->file);
        cap->file = NULL;
    }
    if ( cap->path[0] != '\0' ) {
        DeleteFileW(cap->path);
        cap->path[0] = '\0';
    }
    return text;
}

static char* read_edit_text(HWND edit)
{
    int len=GetWindowTextLengthW(edit);
    WCHAR* wide=(WCHAR*)malloc(((size_t)len+1)*sizeof(*wide));
    if(!wide)return NULL;
    GetWindowTextW(edit,wide,len+1);
    int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,NULL,0,NULL,NULL);
    char* text=bytes>0?(char*)malloc((size_t)bytes):NULL;
    if(text && !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,text,bytes,NULL,NULL)){free(text);text=NULL;}
    free(wide);
    return text;
}

static int set_edit_utf8(HWND edit,const char* text)
{
    if(!text)text="";
    int units=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,NULL,0);
    WCHAR* wide=units>0?(WCHAR*)malloc((size_t)units*sizeof(*wide)):NULL;
    if(!wide)return 0;
    int ok=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,wide,units)>0 && SetWindowTextW(edit,wide);
    free(wide);return ok;
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
    options.tempDir = "build\\test\\xl_run_demo";
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
    if(!captureOk) {
        if(outError)*outError=dup_text("cannot capture stdout/stderr");
        xlDestroyContext(ctx);return 0;
    }
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
    if(!set_edit_utf8(g_output,text))SetWindowTextW(g_output,L"output is not valid UTF-8 or memory is exhausted");
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
    const struct {
        int isC;
        const char* source;
        int succeeds;
        const char* output;
        const char* error;
    } cases[] = {
        {0, g_xlangSample, 1, "hello from xlang", ""},
        {1, g_cSample, 1, "hello from C\n42", ""},
        {1, "int main(void){ invalid C token; }", 0, "", "undeclared"},
        {1, g_cSample, 1, "42", ""},
        {0, "assert false, \"expected GUI error\"\n", 0, "", "expected GUI error"},
        {0, "println(\"UTF-8 \xe9\xaa\x8c\xe6\x94\xb6\")\nprintln(42)\n", 1,
            "UTF-8 \xe9\xaa\x8c\xe6\x94\xb6", ""},
        {1, "#include <stdio.h>\nint main(void){fputs(\"stderr captured\",stderr);return 0;}", 1,
            "stderr captured", ""}
    };
    for ( int repeat = 0; repeat < 3; ++repeat ) {
        for ( size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i ) {
            char* captured = NULL;
            char* error = NULL;
            int ok = run_source_capture(cases[i].isC, cases[i].source, &captured, &error);
            int matches = !!ok == cases[i].succeeds &&
                text_contains(captured, cases[i].output) && text_contains(error, cases[i].error);
            free(captured);
            free(error);
            if ( !matches ) return (int)i + 1;
        }
    }
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
        g_modeXlang = CreateWindowA("BUTTON", "&XLang", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON,
                                   0, 0, 0, 0, hwnd, (HMENU)IDC_MODE_XLANG, NULL, NULL);
        g_modeC = CreateWindowA("BUTTON", "&C", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                0, 0, 0, 0, hwnd, (HMENU)IDC_MODE_C, NULL, NULL);
        g_run = CreateWindowA("BUTTON", "&Run", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                              0, 0, 0, 0, hwnd, (HMENU)IDC_RUN, NULL, NULL);
        g_sampleXlang = CreateWindowA("BUTTON", "XLang sample", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      0, 0, 0, 0, hwnd, (HMENU)IDC_SAMPLE_XLANG, NULL, NULL);
        g_sampleC = CreateWindowA("BUTTON", "C sample", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  0, 0, 0, 0, hwnd, (HMENU)IDC_SAMPLE_C, NULL, NULL);
        g_input = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
                                     ES_LEFT | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                                 0, 0, 0, 0, hwnd, (HMENU)IDC_INPUT, NULL, NULL);
        g_output = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
                                      ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                                  0, 0, 0, 0, hwnd, (HMENU)IDC_OUTPUT, NULL, NULL);
        CheckRadioButton(hwnd,IDC_MODE_XLANG,IDC_MODE_C,IDC_MODE_XLANG);
        set_edit_utf8(g_input,g_xlangSample);
        layout(hwnd);
        return 0;
    case WM_SIZE:
        layout(hwnd);
        return 0;
    case WM_COMMAND:
        switch ( LOWORD(wParam) ) {
        case IDC_MODE_XLANG:
        case IDC_MODE_C:
            CheckRadioButton(hwnd,IDC_MODE_XLANG,IDC_MODE_C,LOWORD(wParam));
            SetFocus(g_input);
            return 0;
        case IDC_RUN:
            run_current_source(hwnd);
            return 0;
        case IDC_SAMPLE_XLANG:
            CheckRadioButton(hwnd,IDC_MODE_XLANG,IDC_MODE_C,IDC_MODE_XLANG);
            set_edit_utf8(g_input, g_xlangSample);
            return 0;
        case IDC_SAMPLE_C:
            CheckRadioButton(hwnd,IDC_MODE_XLANG,IDC_MODE_C,IDC_MODE_C);
            set_edit_utf8(g_input, g_cSample);
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
    while ( GetMessageW(&msg, NULL, 0, 0) > 0 ) {
        if(msg.message==WM_SYSKEYDOWN && (msg.wParam=='C' || msg.wParam=='X' || msg.wParam=='R')) {
            SendMessageW(hwnd,WM_COMMAND,msg.wParam=='C'?IDC_MODE_C:msg.wParam=='X'?IDC_MODE_XLANG:IDC_RUN,0);
            continue;
        }
        if(msg.message==WM_KEYDOWN && (msg.wParam==VK_F5 ||
            (msg.wParam==VK_RETURN && GetKeyState(VK_CONTROL)<0))) {
            run_current_source(hwnd);continue;
        }
        if(IsDialogMessageW(hwnd,&msg))continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

#ifndef XL6_GUI_ENTRY_ONLY
int main(void)
{
    return run_self_test();
}
#endif
