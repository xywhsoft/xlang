/* winui_native.c - C companion for the winui Win32 GUI verification tool.
 *
 * Wraps a subset of the Win32 API (User32) into plain C functions that the
 * xlang side (winui.xl) reaches through `extern` declarations and four
 * native-backed classes (Window / Label / Button / Edit).
 *
 * The C side also installs a custom WndProc that, on WM_COMMAND from a
 * registered button, calls back into the xlang export `winui_on_button_click`
 * -- this exercises the full C -> xlang -> C round trip while the message
 * loop is running inside a C function.
 *
 * IMPORTANT type mapping (matches cmix_native.c / xl cgen --emit-header):
 *   xlang `int`    -> C int64_t / long long   (64-bit, default integer)
 *   xlang `string` -> C const char*            (UTF-8, xlang owns its copy)
 *   xlang `point`  -> C void*                  (HWND / opaque handle)
 *
 * Build / run together with winui.xl:
 *   xl run   winui.xl winui_native.c -- --headless
 *   xl build winui.xl winui_native.c -o winui.exe --link User32 --link Gdi32
 */

#include <windows.h>
#include <string.h>

/* First control-id reserved for buttons the WndProc should react to. */
#define WINUI_BTN_ID_BASE 1000

/* Static scratch buffer for text returns. xlang copies the result into its
 * own reference-counted string immediately after the call, so a single
 * static buffer is safe for this single-threaded GUI tool. */
static char g_text_buf[4096];

/* Remember the first edit control so the xlang click handler can read it
 * without the xlang side having to track handles manually. */
static HWND g_edit_hwnd = NULL;

/* Forward declaration: the xlang export winui_on_button_click is defined in
 * the generated module.c. We declare it here with `long long` (== int64_t)
 * so the C WndProc can call back into xlang during the message loop. */
extern long long winui_on_button_click(long long buttonId);

/* ---- Custom WndProc ---------------------------------------------------- */
static LRESULT CALLBACK winui_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch ( msg ) {
    case WM_COMMAND:
        if ( HIWORD(wParam) == BN_CLICKED ) {
            int id = LOWORD(wParam);
            if ( id >= WINUI_BTN_ID_BASE ) {
                /* Round trip into xlang. The handler reads the edit text and
                 * shows a MessageBox (GUI mode) or just prints (headless). */
                winui_on_button_click((long long)id);
            }
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* ---- 1. Window create -------------------------------------------------- */
void* winui_window_create(const char* title, long long w, long long h)
{
    WNDCLASSA wc;
    HINSTANCE hInst;
    HWND hwnd;
    static int registered = 0;

    hInst = GetModuleHandleA(NULL);
    if ( !registered ) {
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = winui_wndproc;
        wc.hInstance = hInst;
        wc.lpszClassName = "WinUIWindow";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassA(&wc);
        registered = 1;
    }

    hwnd = CreateWindowA("WinUIWindow",
                         title ? title : "",
                         WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         (int)w, (int)h,
                         NULL, NULL, hInst, NULL);
    return (void*)hwnd;
}

/* ---- 2. Window destroy ------------------------------------------------- */
void winui_window_destroy(void* hwnd)
{
    if ( hwnd != NULL ) {
        DestroyWindow((HWND)hwnd);
    }
}

/* ---- 3. Window show ---------------------------------------------------- */
void winui_window_show(void* hwnd)
{
    if ( hwnd != NULL ) {
        ShowWindow((HWND)hwnd, SW_SHOW);
        UpdateWindow((HWND)hwnd);
    }
}

/* ---- 4. MessageBox ----------------------------------------------------- */
long long winui_message_box(void* hwnd, const char* text, const char* title, long long flags)
{
    return (long long)MessageBoxA((HWND)hwnd,
                                  text ? text : "",
                                  title ? title : "",
                                  (UINT)flags);
}

/* ---- 5. Message loop --------------------------------------------------- */
long long winui_message_loop(void)
{
    MSG msg;
    while ( GetMessageA(&msg, NULL, 0, 0) > 0 ) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (long long)msg.wParam;
}

/* ---- Helper: create a child control with the default GUI font ---------- */
static void* create_child(const char* klass, const char* text, void* parent,
                          long long x, long long y, long long w, long long h,
                          long long style, long long id)
{
    HWND hParent = (HWND)parent;
    HWND ctl = CreateWindowExA(0,
                               klass,
                               text ? text : "",
                               WS_CHILD | WS_VISIBLE | (DWORD)style,
                               (int)x, (int)y, (int)w, (int)h,
                               hParent,
                               (HMENU)(ULONG_PTR)id,
                               GetModuleHandleA(NULL),
                               NULL);
    if ( ctl != NULL ) {
        SendMessageA(ctl, WM_SETFONT,
                     (WPARAM)GetStockObject(DEFAULT_GUI_FONT),
                     MAKELPARAM(TRUE, 0));
    }
    return (void*)ctl;
}

/* ---- 6. Label ---------------------------------------------------------- */
void* winui_label_create(void* parent, const char* text,
                         long long x, long long y, long long w, long long h)
{
    return create_child("STATIC", text, parent, x, y, w, h, SS_LEFT, 0);
}

/* ---- 7. Button --------------------------------------------------------- */
/* Extra `id` param: buttons pass their id as the HMENU so WM_COMMAND can
 * identify them. id >= 1000 triggers the xlang callback in the WndProc. */
void* winui_button_create(void* parent, const char* text,
                          long long x, long long y, long long w, long long h,
                          long long id)
{
    return create_child("BUTTON", text, parent, x, y, w, h, BS_PUSHBUTTON, id);
}

/* ---- 8. Edit ----------------------------------------------------------- */
void* winui_edit_create(void* parent, const char* text,
                        long long x, long long y, long long w, long long h)
{
    HWND ctl = (HWND)create_child("EDIT", text, parent, x, y, w, h,
                                  ES_LEFT | ES_AUTOHSCROLL, 0);
    if ( ctl != NULL && g_edit_hwnd == NULL ) {
        g_edit_hwnd = ctl;  /* remember first edit for the click handler */
    }
    return (void*)ctl;
}

/* ---- 9. Get text (returns const char*; xlang copies) ------------------- */
const char* winui_get_text(void* hwnd_ctrl)
{
    int len;
    g_text_buf[0] = 0;
    if ( hwnd_ctrl == NULL ) {
        return g_text_buf;
    }
    len = GetWindowTextLengthA((HWND)hwnd_ctrl);
    if ( len < 0 ) {
        len = 0;
    }
    if ( len >= (int)sizeof(g_text_buf) ) {
        len = (int)sizeof(g_text_buf) - 1;
    }
    GetWindowTextA((HWND)hwnd_ctrl, g_text_buf, len + 1);
    g_text_buf[len] = 0;
    return g_text_buf;
}

/* ---- 10. Set text ------------------------------------------------------ */
void winui_set_text(void* hwnd_ctrl, const char* text)
{
    if ( hwnd_ctrl != NULL ) {
        SetWindowTextA((HWND)hwnd_ctrl, text ? text : "");
    }
}

/* ---- 11. Sleep --------------------------------------------------------- */
void winui_sleep(long long ms)
{
    Sleep((DWORD)ms);
}
