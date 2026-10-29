#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../hooks/op_hook_shared.h"

#ifndef FRAME_W
#define FRAME_W 64u
#endif
#ifndef FRAME_H
#define FRAME_H 64u
#endif

int main(int argc, char **argv)
{
    WNDCLASSW window_class = { 0 };
    PIXELFORMATDESCRIPTOR pixel_format = { 0 };
    char map_name[64];
    HANDLE mapping = NULL;
    unsigned char *shared = NULL;
    unsigned char *pixels = NULL;
    HMODULE hook = NULL;
    HWND window = NULL;
    HDC dc = NULL;
    HGLRC gl = NULL;
    unsigned int width = 0u, height = 0u;
    int format;
    int result = 1;
    int i;

    if (argc != 2) return 2;
    snprintf(map_name, sizeof(map_name), "op_hook_v2_%lu", (unsigned long)GetCurrentProcessId());
    mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                 0, OP_HOOK_MAP_SIZE, map_name);
    if (!mapping) goto done;
    shared = (unsigned char *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS,
                                           0, 0, OP_HOOK_MAP_SIZE);
    if (!shared) goto done;
    memset(shared, 0, sizeof(op_hook_header));
    hook = LoadLibraryA(argv[1]);
    if (!hook) { fprintf(stderr, "load hook: %lu\n", GetLastError()); goto done; }

    window_class.lpfnWndProc = DefWindowProcW;
    window_class.hInstance = GetModuleHandleW(NULL);
    window_class.lpszClassName = L"op wgl probe";
    window_class.style = CS_OWNDC;
    if (!RegisterClassW(&window_class)) goto done;
    window = CreateWindowExW(0, window_class.lpszClassName, L"op wgl test",
                             WS_POPUP, 0, 0, FRAME_W, FRAME_H,
                             NULL, NULL, window_class.hInstance, NULL);
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
    glViewport(0, 0, FRAME_W, FRAME_H);
    pixels = (unsigned char *)malloc((size_t)FRAME_W * FRAME_H * 4u);
    if (!pixels) goto done;

    for (i = 0; i < 100; ++i) {
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (!SwapBuffers(dc)) { fputs("SwapBuffers failed\n", stderr); goto done; }
        if (op_hook_snapshot(shared, pixels, (size_t)FRAME_W * FRAME_H * 4u,
                             &width, &height)) {
            size_t last = (size_t)FRAME_W * FRAME_H * 4u - 4u;
            if (width != FRAME_W || height != FRAME_H ||
                pixels[0] > 30u || pixels[1] > 30u || pixels[2] < 200u ||
                pixels[last] > 30u || pixels[last + 1u] > 30u || pixels[last + 2u] < 200u) {
                fprintf(stderr, "wrong WGL frame: %ux%u BGRA %u,%u,%u,%u\n",
                        width, height, pixels[0], pixels[1], pixels[2], pixels[3]);
                goto done;
            }
            puts("op WGL real capture: PASS");
            result = 0;
            break;
        }
        Sleep(20u);
    }
    if (result) fputs("no WGL hook frame\n", stderr);
done:
    free(pixels);
    if (gl) { wglMakeCurrent(NULL, NULL); wglDeleteContext(gl); }
    if (dc) ReleaseDC(window, dc);
    if (window) DestroyWindow(window);
    if (hook) FreeLibrary(hook);
    if (shared) UnmapViewOfFile(shared);
    if (mapping) CloseHandle(mapping);
    return result;
}
