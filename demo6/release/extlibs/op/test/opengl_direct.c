#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "../op_native.c"

#define TEST_GL_COLOR_BUFFER_BIT 0x00004000u
#define TEST_GL_SCISSOR_TEST     0x0C11u
#define TEST_GL_BACK             0x0405u
#define TEST_GL_READ_BUFFER      0x0C02u
#define TEST_GL_PACK_ALIGNMENT   0x0D05u
#define TEST_GL_PACK_ROW_LENGTH  0x0D02u
#define TEST_GL_PACK_SKIP_ROWS   0x0D03u
#define TEST_GL_PACK_SKIP_PIXELS 0x0D04u
#define TEST_GL_PIXEL_PACK_BUFFER 0x88EBu
#define TEST_GL_READ_FRAMEBUFFER 0x8CA8u

typedef HGLRC (WINAPI *test_wgl_create)(HDC);
typedef BOOL (WINAPI *test_wgl_current)(HDC, HGLRC);
typedef BOOL (WINAPI *test_wgl_delete)(HGLRC);
typedef void (WINAPI *test_gl_clear_color)(float, float, float, float);
typedef void (WINAPI *test_gl_clear)(unsigned int);
typedef void (WINAPI *test_gl_viewport)(int, int, int, int);
typedef void (WINAPI *test_gl_scissor)(int, int, int, int);
typedef void (WINAPI *test_gl_toggle)(unsigned int);
typedef void (WINAPI *test_gl_get_integer)(unsigned int, int *);
typedef void (WINAPI *test_gl_pixel_store)(unsigned int, int);
typedef PROC (WINAPI *test_wgl_get_proc)(LPCSTR);
typedef void (WINAPI *test_gl_gen_objects)(int, unsigned int *);
typedef void (WINAPI *test_gl_bind_object)(unsigned int, unsigned int);
typedef void (WINAPI *test_gl_delete_objects)(int, const unsigned int *);

int main(void)
{
    HMODULE gl = LoadLibraryW(L"opengl32.dll");
    test_wgl_create create_context;
    test_wgl_current make_current;
    test_wgl_delete delete_context;
    test_gl_clear_color clear_color;
    test_gl_clear clear;
    test_gl_viewport viewport;
    test_gl_scissor scissor;
    test_gl_toggle enable, disable;
    test_gl_get_integer get_integer;
    test_gl_pixel_store pixel_store;
    test_wgl_get_proc get_proc;
    WNDCLASSW cls = {0};
    PIXELFORMATDESCRIPTOR format = {0};
    HWND window = NULL;
    HDC dc = NULL;
    HGLRC context = NULL;
    op_context *op = NULL;
    char *top = NULL, *bottom = NULL;
    int pixel_format, read_buffer = 0, pack_alignment = 0;
    int row_length = 0, skip_rows = 0, skip_pixels = 0;
    int major = 0, minor = 0, tested_pbo = 0, tested_fbo = 0;
    int stage = 1, ok = 0;
    if (!gl) goto done;
#define TEST_LOAD(type, name) (type)GetProcAddress(gl, name)
    create_context = TEST_LOAD(test_wgl_create, "wglCreateContext");
    make_current = TEST_LOAD(test_wgl_current, "wglMakeCurrent");
    delete_context = TEST_LOAD(test_wgl_delete, "wglDeleteContext");
    clear_color = TEST_LOAD(test_gl_clear_color, "glClearColor");
    clear = TEST_LOAD(test_gl_clear, "glClear");
    viewport = TEST_LOAD(test_gl_viewport, "glViewport");
    scissor = TEST_LOAD(test_gl_scissor, "glScissor");
    enable = TEST_LOAD(test_gl_toggle, "glEnable");
    disable = TEST_LOAD(test_gl_toggle, "glDisable");
    get_integer = TEST_LOAD(test_gl_get_integer, "glGetIntegerv");
    pixel_store = TEST_LOAD(test_gl_pixel_store, "glPixelStorei");
    get_proc = TEST_LOAD(test_wgl_get_proc, "wglGetProcAddress");
    if (!create_context || !make_current || !delete_context || !clear_color ||
        !clear || !viewport || !scissor || !enable || !disable ||
        !get_integer || !pixel_store || !get_proc) goto done;
    cls.lpfnWndProc = DefWindowProcW;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"op_direct_gl_test";
    cls.style = CS_OWNDC;
    if (!RegisterClassW(&cls)) goto done;
    window = CreateWindowExW(WS_EX_TOOLWINDOW, cls.lpszClassName, L"",
                             WS_POPUP, -32000, -32000, 8, 8,
                             NULL, NULL, cls.hInstance, NULL);
    if (!window) goto done;
    dc = GetDC(window);
    if (!dc) goto done;
    format.nSize = sizeof(format);
    format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA;
    format.cColorBits = 32;
    format.iLayerType = PFD_MAIN_PLANE;
    pixel_format = ChoosePixelFormat(dc, &format);
    if (!pixel_format || !SetPixelFormat(dc, pixel_format, &format)) goto done;
    context = create_context(dc);
    if (!context || !make_current(dc, context)) goto done;
    ShowWindow(window, SW_SHOWNOACTIVATE);
    stage = 2;
    viewport(0, 0, 8, 8);
    clear_color(1.0f, 0.0f, 0.0f, 1.0f);
    clear(TEST_GL_COLOR_BUFFER_BIT);
    enable(TEST_GL_SCISSOR_TEST);
    scissor(0, 0, 8, 4);
    clear_color(0.0f, 0.0f, 1.0f, 1.0f);
    clear(TEST_GL_COLOR_BUFFER_BIT);
    disable(TEST_GL_SCISSOR_TEST);
    if (!SwapBuffers(dc)) goto done;
    op = xop_create();
    if (!op || !xop_bind_window(op, (long long)(intptr_t)window,
                               "opengl", "normal", "normal", 0)) goto done;
    stage = 3;
    pixel_store(TEST_GL_PACK_ALIGNMENT, 8);
    pixel_store(TEST_GL_PACK_ROW_LENGTH, 16);
    pixel_store(TEST_GL_PACK_SKIP_ROWS, 1);
    pixel_store(TEST_GL_PACK_SKIP_PIXELS, 2);
    top = xop_get_color(op, 0, 0);
    bottom = xop_get_color(op, 0, 7);
    if (!top || !bottom || op->mem_dc || op->mem_bmp) goto done;
    get_integer(TEST_GL_READ_BUFFER, &read_buffer);
    get_integer(TEST_GL_PACK_ALIGNMENT, &pack_alignment);
    get_integer(TEST_GL_PACK_ROW_LENGTH, &row_length);
    get_integer(TEST_GL_PACK_SKIP_ROWS, &skip_rows);
    get_integer(TEST_GL_PACK_SKIP_PIXELS, &skip_pixels);
    if (strcmp(top, "FF0000") != 0 || strcmp(bottom, "0000FF") != 0 ||
        read_buffer != TEST_GL_BACK || pack_alignment != 8 ||
        row_length != 16 || skip_rows != 1 || skip_pixels != 2) goto done;
    {
        const char *version = (const char *)p_glGetString(OP_GL_VERSION);
        if (!version || sscanf(version, "%d.%d", &major, &minor) != 2) goto done;
    }
    stage = 4;
    {
        test_gl_gen_objects gen = (test_gl_gen_objects)get_proc("glGenBuffers");
        test_gl_bind_object bind = (test_gl_bind_object)get_proc("glBindBuffer");
        test_gl_delete_objects remove = (test_gl_delete_objects)get_proc("glDeleteBuffers");
        unsigned int buffer = 0;
        if (gen && bind && remove && (uintptr_t)gen > 4u &&
            (uintptr_t)bind > 4u && (uintptr_t)remove > 4u &&
            (uintptr_t)gen != UINTPTR_MAX && (uintptr_t)bind != UINTPTR_MAX &&
            (uintptr_t)remove != UINTPTR_MAX) {
            gen(1, &buffer);
            if (!buffer) goto done;
            bind(TEST_GL_PIXEL_PACK_BUFFER, buffer);
            {
                char *invalid = xop_get_color(op, 0, 0);
                int rejected = invalid == NULL;
                xrtFree(invalid);
                bind(TEST_GL_PIXEL_PACK_BUFFER, 0);
                remove(1, &buffer);
                if (!rejected) goto done;
            }
            tested_pbo = 1;
        }
    }
    if ((major > 2 || (major == 2 && minor >= 1)) && !tested_pbo) goto done;
    stage = 5;
    {
        test_gl_gen_objects gen = (test_gl_gen_objects)get_proc("glGenFramebuffers");
        test_gl_bind_object bind = (test_gl_bind_object)get_proc("glBindFramebuffer");
        test_gl_delete_objects remove = (test_gl_delete_objects)get_proc("glDeleteFramebuffers");
        unsigned int framebuffer = 0;
        if (gen && bind && remove && (uintptr_t)gen > 4u &&
            (uintptr_t)bind > 4u && (uintptr_t)remove > 4u &&
            (uintptr_t)gen != UINTPTR_MAX && (uintptr_t)bind != UINTPTR_MAX &&
            (uintptr_t)remove != UINTPTR_MAX) {
            gen(1, &framebuffer);
            if (!framebuffer) goto done;
            bind(TEST_GL_READ_FRAMEBUFFER, framebuffer);
            {
                char *invalid = xop_get_color(op, 0, 0);
                int rejected = invalid == NULL;
                xrtFree(invalid);
                bind(TEST_GL_READ_FRAMEBUFFER, 0);
                remove(1, &framebuffer);
                if (!rejected) goto done;
            }
            tested_fbo = 1;
        }
    }
    if (major >= 3 && !tested_fbo) goto done;
    ok = 1;
done:
    if (!ok) fprintf(stderr, "op direct GL stage %d: top=%s bottom=%s read=%d pack=%d row=%d skip=%d,%d version=%d.%d pbo=%d fbo=%d\n",
                     stage, top ? top : "(null)", bottom ? bottom : "(null)",
                     read_buffer, pack_alignment, row_length, skip_rows, skip_pixels,
                     major, minor, tested_pbo, tested_fbo);
    xrtFree(top);
    xrtFree(bottom);
    xop_destroy(op);
    if (context) { make_current(NULL, NULL); delete_context(context); }
    if (dc) ReleaseDC(window, dc);
    if (window) DestroyWindow(window);
    if (cls.lpszClassName) UnregisterClassW(cls.lpszClassName, cls.hInstance);
    if (gl) FreeLibrary(gl);
    if (ok) printf("op direct GL: ok (GL %d.%d, PBO %d, FBO %d)\n",
                   major, minor, tested_pbo, tested_fbo);
    return ok ? 0 : 1;
}
