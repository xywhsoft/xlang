#include <d3d9.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    wchar_t title[96];
    int blue = argc > 1 && strcmp(argv[1], "blue") == 0;
    wsprintfW(title, L"op d3d9 remote %lu", (unsigned long)GetCurrentProcessId());
    HWND window = CreateWindowExW(0, L"STATIC", title,
        WS_POPUP, 0, 0, 64, 64, NULL, NULL, NULL, NULL);
    IDirect3D9* d3d = NULL;
    IDirect3DDevice9* device = NULL;
    D3DPRESENT_PARAMETERS params;
    DWORD started = GetTickCount();
    int result = 1;
    if (window == NULL) goto done;
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (d3d == NULL) goto done;
    memset(&params, 0, sizeof(params));
    params.BackBufferWidth = 64u;
    params.BackBufferHeight = 64u;
    params.BackBufferFormat = D3DFMT_X8R8G8B8;
    params.BackBufferCount = 1u;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow = window;
    params.Windowed = TRUE;
    params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (FAILED(d3d->lpVtbl->CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
            window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &device))) goto done;

    // 子进程只渲染固定帧；由父进程负责注入、读取和结束信号。
    while (IsWindow(window) && GetTickCount() - started < 15000u) {
        MSG message;
        while (PeekMessageW(&message, NULL, 0u, 0u, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!IsWindow(window)) break;
        if (FAILED(device->lpVtbl->Clear(device, 0u, NULL, D3DCLEAR_TARGET,
                blue ? D3DCOLOR_XRGB(0, 0, 255) : D3DCOLOR_XRGB(255, 0, 0),
                1.0f, 0u)) ||
            FAILED(device->lpVtbl->Present(device, NULL, NULL, NULL, NULL))) goto done;
        Sleep(16u);
    }
    result = 0;
done:
    if (device != NULL) device->lpVtbl->Release(device);
    if (d3d != NULL) d3d->lpVtbl->Release(d3d);
    if (window != NULL && IsWindow(window)) DestroyWindow(window);
    if (result != 0) fputs("op D3D9 remote target: failed\n", stderr);
    return result;
}
