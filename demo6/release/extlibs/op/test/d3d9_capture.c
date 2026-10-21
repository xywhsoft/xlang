#include <d3d9.h>
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
typedef void *(*vtable_proc)(void);

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
    unsigned char *pixels = NULL;
    HMODULE hook = NULL;
    HWND window = NULL;
    IDirect3D9 *d3d = NULL;
    IDirect3DDevice9 *device = NULL;
    D3DPRESENT_PARAMETERS pp;
    status_proc get_status = NULL;
    status_proc get_error = NULL;
    status_proc get_calls = NULL;
    unsigned int width = 0u, height = 0u;
    HRESULT hr;
    int result = 1;
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
    get_status = status_symbol(hook, "op_d3d9_hook_status");
    get_error = status_symbol(hook, "op_d3d9_hook_error");
    get_calls = status_symbol(hook, "op_d3d9_hook_calls");
    if (!get_status || !get_error || !get_calls) {
        fputs("missing hook diagnostics\n", stderr);
        goto done;
    }
    for (i = 0; i < 200 && get_status() == 0; ++i) Sleep(10u);
    if (get_status() != 1) {
        fprintf(stderr, "hook init: status=%ld error=0x%08lx\n",
                (long)get_status(), (unsigned long)get_error());
        goto done;
    }

    window = CreateWindowExW(0, L"STATIC", L"op d3d9 test", WS_POPUP,
                             0, 0, FRAME_W, FRAME_H, NULL, NULL, NULL, NULL);
    if (!window) { fprintf(stderr, "window: %lu\n", GetLastError()); goto done; }
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { fputs("Direct3DCreate9 failed\n", stderr); goto done; }
    memset(&pp, 0, sizeof(pp));
    pp.BackBufferWidth = FRAME_W;
    pp.BackBufferHeight = FRAME_H;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1u;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    hr = d3d->lpVtbl->CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    if (FAILED(hr)) { fprintf(stderr, "device: 0x%08lx\n", (unsigned long)hr); goto done; }
    pixels = (unsigned char *)malloc(FRAME_W * FRAME_H * 4u);
    if (!pixels) goto done;

    for (i = 0; i < 100; ++i) {
        hr = device->lpVtbl->Clear(device, 0u, NULL, D3DCLEAR_TARGET,
                                    D3DCOLOR_XRGB(255, 0, 0), 1.0f, 0u);
        if (FAILED(hr)) { fprintf(stderr, "clear: 0x%08lx\n", (unsigned long)hr); goto done; }
        hr = device->lpVtbl->Present(device, NULL, NULL, NULL, NULL);
        if (FAILED(hr)) { fprintf(stderr, "present: 0x%08lx\n", (unsigned long)hr); goto done; }
        if (op_hook_snapshot(shared, pixels, FRAME_W * FRAME_H * 4u,
                             &width, &height)) {
            size_t last = FRAME_W * FRAME_H * 4u - 4u;
            if (width != FRAME_W || height != FRAME_H ||
                pixels[0] > 30u || pixels[1] > 30u || pixels[2] < 200u ||
                pixels[last] > 30u || pixels[last + 1u] > 30u || pixels[last + 2u] < 200u) {
                fprintf(stderr, "wrong frame: %ux%u BGRA %u,%u,%u,%u\n",
                        width, height, pixels[0], pixels[1], pixels[2], pixels[3]);
                goto done;
            }
            puts("op D3D9 real capture: PASS");
            result = 0;
            break;
        }
        Sleep(20u);
    }
    if (result) {
        FARPROC raw = GetProcAddress(hook, "op_d3d9_probe_vtable");
        vtable_proc probed = NULL;
        FARPROC present_raw = GetProcAddress(hook, "op_d3d9_probe_present");
        vtable_proc probe_present = NULL;
        if (sizeof(raw) == sizeof(probed)) memcpy(&probed, &raw, sizeof(probed));
        if (sizeof(present_raw) == sizeof(probe_present))
            memcpy(&probe_present, &present_raw, sizeof(probe_present));
        fprintf(stderr, "no hook frame: magic=0x%x counter=%u status=%ld error=0x%08lx calls=%ld\n",
                ((op_hook_header *)shared)->magic,
                ((op_hook_header *)shared)->frame_counter,
                (long)get_status(), (unsigned long)get_error(), (long)get_calls());
        fprintf(stderr, "probe vtable=%p probe Present=%p target vtable=%p target Present=%p\n",
                probed ? probed() : NULL, probe_present ? probe_present() : NULL,
                (void *)device->lpVtbl, (void *)device->lpVtbl->Present);
    }

done:
    free(pixels);
    if (device) device->lpVtbl->Release(device);
    if (d3d) d3d->lpVtbl->Release(d3d);
    if (window) DestroyWindow(window);
    /* Hook 由进程退出卸载，避免测试期间卸载已被修改的 vtable。 */
    if (shared) UnmapViewOfFile(shared);
    if (mapping) CloseHandle(mapping);
    return result;
}
