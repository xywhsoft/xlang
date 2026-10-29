#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    WNDCLASSW window_class = { 0 };
    PIXELFORMATDESCRIPTOR pixel_format = { 0 };
    wchar_t title[96];
    HWND window = NULL;
    HDC dc = NULL;
    HGLRC gl = NULL;
    DWORD started = GetTickCount();
    int blue = argc > 1 && strcmp(argv[1], "blue") == 0;
    int format;
    int result = 1;

    wsprintfW(title, L"op wgl remote %lu", (unsigned long)GetCurrentProcessId());
    window_class.lpfnWndProc = DefWindowProcW;
    window_class.hInstance = GetModuleHandleW(NULL);
    window_class.lpszClassName = L"op wgl remote class";
    window_class.style = CS_OWNDC;
    if (!RegisterClassW(&window_class)) goto done;
    window = CreateWindowExW(0, window_class.lpszClassName, title,
                             WS_POPUP, 0, 0, 64, 64, NULL, NULL,
                             window_class.hInstance, NULL);
    if (!window) goto done;
    dc = GetDC(window);
    if (!dc) goto done;
    pixel_format.nSize = sizeof(pixel_format);
    pixel_format.nVersion = 1;
    pixel_format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixel_format.iPixelType = PFD_TYPE_RGBA;
    pixel_format.cColorBits = 32;
    pixel_format.iLayerType = PFD_MAIN_PLANE;
    format = ChoosePixelFormat(dc, &pixel_format);
    if (format == 0 || !SetPixelFormat(dc, format, &pixel_format)) goto done;
    gl = wglCreateContext(dc);
    if (!gl || !wglMakeCurrent(dc, gl)) goto done;
    glViewport(0, 0, 64, 64);

    // 目标进程只负责持续渲染，注入和验证都由父进程完成。
    while (IsWindow(window) && GetTickCount() - started < 15000u) {
        MSG message;
        while (PeekMessageW(&message, NULL, 0u, 0u, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!IsWindow(window)) break;
        glClearColor(blue ? 0.0f : 1.0f, 0.0f, blue ? 1.0f : 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (!SwapBuffers(dc)) goto done;
        Sleep(16u);
    }
    result = 0;
done:
    if (gl) { wglMakeCurrent(NULL, NULL); wglDeleteContext(gl); }
    if (dc) ReleaseDC(window, dc);
    if (window) DestroyWindow(window);
    if (result != 0) fputs("op WGL remote target: failed\n", stderr);
    return result;
}
