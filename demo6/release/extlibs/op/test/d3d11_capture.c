#define INITGUID
#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>
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

typedef LONG (*status_proc)(void);

static status_proc status_symbol(HMODULE hook, const char *name)
{
    FARPROC raw = GetProcAddress(hook, name);
    status_proc proc = NULL;
    if (sizeof(raw) == sizeof(proc)) memcpy(&proc, &raw, sizeof(proc));
    return proc;
}

int main(int argc, char **argv)
{
    char name[64];
    HANDLE mapping = NULL;
    unsigned char *shared = NULL;
    HMODULE hook = NULL;
    HWND window = NULL;
    IDXGISwapChain *swap = NULL;
    ID3D11Device *device = NULL;
    ID3D11DeviceContext *context = NULL;
    ID3D11Texture2D *back = NULL;
    ID3D11RenderTargetView *view = NULL;
    unsigned char *pixels = NULL;
    DXGI_SWAP_CHAIN_DESC desc;
    D3D_FEATURE_LEVEL feature = D3D_FEATURE_LEVEL_11_0;
    const float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    HRESULT hr;
    unsigned int width = 0u, height = 0u;
    int status = 1;
    int i;

    if (argc != 2) return 2;
    snprintf(name, sizeof(name), "op_hook_v2_%lu", (unsigned long)GetCurrentProcessId());
    mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                 0, OP_HOOK_MAP_SIZE, name);
    if (!mapping) { fprintf(stderr, "mapping: %lu\n", GetLastError()); goto done; }
    shared = (unsigned char *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS,
                                           0, 0, OP_HOOK_MAP_SIZE);
    if (!shared) { fprintf(stderr, "map view: %lu\n", GetLastError()); goto done; }
    memset(shared, 0, sizeof(op_hook_header));

    hook = LoadLibraryA(argv[1]);
    if (!hook) { fprintf(stderr, "load hook: %lu\n", GetLastError()); goto done; }
    window = CreateWindowExW(0, L"STATIC", L"op capture test", WS_POPUP,
                             0, 0, FRAME_W, FRAME_H, NULL, NULL, NULL, NULL);
    if (!window) { fprintf(stderr, "window: %lu\n", GetLastError()); goto done; }

    memset(&desc, 0, sizeof(desc));
    desc.BufferDesc.Width = FRAME_W;
    desc.BufferDesc.Height = FRAME_H;
    desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1u;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1u;
    desc.OutputWindow = window;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, &feature, 1u, D3D11_SDK_VERSION,
            &desc, &swap, &device, NULL, &context);
    if (FAILED(hr)) { fprintf(stderr, "device/swap: 0x%08lx\n", (unsigned long)hr); goto done; }
    hr = swap->lpVtbl->GetBuffer(swap, 0u, &IID_ID3D11Texture2D, (void **)&back);
    if (FAILED(hr)) { fprintf(stderr, "backbuffer: 0x%08lx\n", (unsigned long)hr); goto done; }
    hr = device->lpVtbl->CreateRenderTargetView(device, (ID3D11Resource *)back,
                                                 NULL, &view);
    if (FAILED(hr)) { fprintf(stderr, "render target: 0x%08lx\n", (unsigned long)hr); goto done; }
    pixels = (unsigned char *)malloc(FRAME_W * FRAME_H * 4u);
    if (!pixels) goto done;

    for (i = 0; i < 100; ++i) {
        context->lpVtbl->ClearRenderTargetView(context, view, red);
        hr = swap->lpVtbl->Present(swap, 0u, 0u);
        if (FAILED(hr)) { fprintf(stderr, "present: 0x%08lx\n", (unsigned long)hr); goto done; }
        if (op_hook_snapshot(shared, pixels, FRAME_W * FRAME_H * 4u,
                             &width, &height)) {
            size_t last = FRAME_W * FRAME_H * 4u - 4u;
            if (width != FRAME_W || height != FRAME_H ||
                pixels[0] > 30u || pixels[1] > 30u || pixels[2] < 200u ||
                pixels[last] > 30u || pixels[last + 1u] > 30u ||
                pixels[last + 2u] < 200u) {
                fprintf(stderr, "wrong frame: %ux%u BGRA %u,%u,%u,%u\n",
                        width, height, pixels[0], pixels[1], pixels[2], pixels[3]);
                goto done;
            }
            puts("op D3D11 real capture: PASS");
            status = 0;
            break;
        }
        Sleep(20u);
    }
    if (status) fprintf(stderr, "no hook frame: magic=0x%x counter=%u\n",
                        ((op_hook_header *)shared)->magic,
                        ((op_hook_header *)shared)->frame_counter);
    if (status && hook) {
        status_proc get_status = status_symbol(hook, "op_d3d11_hook_status");
        status_proc get_error = status_symbol(hook, "op_d3d11_hook_error");
        status_proc get_calls = status_symbol(hook, "op_d3d11_hook_calls");
        status_proc get_stage = status_symbol(hook, "op_d3d11_capture_stage");
        if (get_status && get_error && get_calls && get_stage)
            fprintf(stderr, "hook init: status=%ld error=0x%08lx calls=%ld stage=%ld\n",
                    (long)get_status(), (unsigned long)get_error(),
                    (long)get_calls(), (long)get_stage());
    }

done:
    free(pixels);
    if (view) view->lpVtbl->Release(view);
    if (back) back->lpVtbl->Release(back);
    if (swap) swap->lpVtbl->Release(swap);
    if (context) context->lpVtbl->Release(context);
    if (device) device->lpVtbl->Release(device);
    if (window) DestroyWindow(window);
    /* 进程退出卸载 hook；不要在回调仍可能执行时主动 FreeLibrary。 */
    if (shared) UnmapViewOfFile(shared);
    if (mapping) CloseHandle(mapping);
    return status;
}
