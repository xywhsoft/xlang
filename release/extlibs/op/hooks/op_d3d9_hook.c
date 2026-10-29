/*
 * op_d3d9_hook.c - D3D9 Present Hook DLL
 * Injected into target process, hooks the shared IDirect3DDevice9::Present function.
 * Captures back buffer to shared memory after each Present call.
 * Build with MinGW GCC and the vendored MinHook sources.
 */
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include "op_hook_shared.h"
#include "third_party/minhook/include/MinHook.h"

/* ------------------------------------------------------------------ */
/* Minimal D3D9 vtable declarations                                    */
/* ------------------------------------------------------------------ */

typedef struct IDirect3DDevice9 IDirect3DDevice9;
typedef struct IDirect3DSurface9 IDirect3DSurface9;

typedef struct {
    unsigned int Format;
    unsigned int Type;
    unsigned int Usage;
    unsigned int Pool;
    unsigned int MultiSampleType;
    unsigned int MultiSampleQuality;
    unsigned int Width;
    unsigned int Height;
} D3DSURFACE_DESC;

typedef struct {
    int Pitch;
    void *pBits;
} D3DLOCKED_RECT;

#define D3DPOOL_SYSTEMMEM 2
#define D3DFMT_A8R8G8B8  21
#define D3DFMT_X8R8G8B8  22
#define D3DLOCK_READONLY  0x10

/* IDirect3DSurface9 vtable (IUnknown=3 + IDirect3DResource9=8 + Surface=4) */
typedef struct IDirect3DSurface9Vtbl {
    HRESULT (WINAPI *QueryInterface)(IDirect3DSurface9*, const void*, void**);
    ULONG   (WINAPI *AddRef)(IDirect3DSurface9*);
    ULONG   (WINAPI *Release)(IDirect3DSurface9*);
    /* IDirect3DResource9 */
    void *m[8];
    /* IDirect3DSurface9 */
    void *GetContainer;
    HRESULT (WINAPI *GetDesc)(IDirect3DSurface9*, D3DSURFACE_DESC*);
    HRESULT (WINAPI *LockRect)(IDirect3DSurface9*, D3DLOCKED_RECT*, const void*, unsigned int);
    HRESULT (WINAPI *UnlockRect)(IDirect3DSurface9*);
} IDirect3DSurface9Vtbl;
struct IDirect3DSurface9 { const IDirect3DSurface9Vtbl *lpVtbl; };

/* IDirect3DDevice9 vtable - Present is at index 17 */
typedef struct IDirect3DDevice9Vtbl {
    HRESULT (WINAPI *QueryInterface)(IDirect3DDevice9*, const void*, void**);
    ULONG   (WINAPI *AddRef)(IDirect3DDevice9*);
    ULONG   (WINAPI *Release)(IDirect3DDevice9*);
    /* slots 3-16 */
    void *m[14];
    /* slot 17: Present */
    HRESULT (WINAPI *Present)(IDirect3DDevice9*, const void*, const void*, void*, const void*);
    /* slot 18: GetBackBuffer */
    HRESULT (WINAPI *GetBackBuffer)(IDirect3DDevice9*, unsigned int, unsigned int, unsigned int, IDirect3DSurface9**);
    void *before_readback[13]; /* slots 19-31 */
    HRESULT (WINAPI *GetRenderTargetData)(IDirect3DDevice9*, IDirect3DSurface9*, IDirect3DSurface9*);
    void *before_surface[3]; /* slots 33-35 */
    HRESULT (WINAPI *CreateOffscreenPlainSurface)(IDirect3DDevice9*, unsigned int, unsigned int,
                                                   unsigned int, unsigned int, IDirect3DSurface9**, void*);
} IDirect3DDevice9Vtbl;
struct IDirect3DDevice9 { const IDirect3DDevice9Vtbl *lpVtbl; };

typedef char op_d3d9_present_slot_check[(offsetof(IDirect3DDevice9Vtbl, Present) / sizeof(void*) == 17u) ? 1 : -1];
typedef char op_d3d9_backbuffer_slot_check[(offsetof(IDirect3DDevice9Vtbl, GetBackBuffer) / sizeof(void*) == 18u) ? 1 : -1];
typedef char op_d3d9_readback_slot_check[(offsetof(IDirect3DDevice9Vtbl, GetRenderTargetData) / sizeof(void*) == 32u) ? 1 : -1];
typedef char op_d3d9_offscreen_slot_check[(offsetof(IDirect3DDevice9Vtbl, CreateOffscreenPlainSurface) / sizeof(void*) == 36u) ? 1 : -1];
typedef char op_d3d9_surface_desc_slot_check[(offsetof(IDirect3DSurface9Vtbl, GetDesc) / sizeof(void*) == 12u) ? 1 : -1];
typedef char op_d3d9_surface_lock_slot_check[(offsetof(IDirect3DSurface9Vtbl, LockRect) / sizeof(void*) == 13u) ? 1 : -1];

/* ------------------------------------------------------------------ */
/* Hook state                                                          */
/* ------------------------------------------------------------------ */

static void *g_shared_mem = NULL;
static unsigned int g_map_size = OP_HOOK_MAP_SIZE;
static CRITICAL_SECTION g_capture_lock;
static IDirect3DDevice9 *g_capture_device = NULL;
static IDirect3DSurface9 *g_readback = NULL;
static unsigned int g_readback_w = 0, g_readback_h = 0, g_readback_format = 0;
static volatile LONG g_hook_status = 0;
static volatile LONG g_hook_error = 0;
static volatile LONG g_hook_calls = 0;
static void *g_probe_vtable = NULL;
static void *g_target_present = NULL;

__declspec(dllexport) LONG op_d3d9_hook_status(void) {
    return InterlockedCompareExchange(&g_hook_status, 0, 0);
}

__declspec(dllexport) LONG op_d3d9_hook_error(void) {
    return InterlockedCompareExchange(&g_hook_error, 0, 0);
}

__declspec(dllexport) LONG op_d3d9_hook_calls(void) {
    return InterlockedCompareExchange(&g_hook_calls, 0, 0);
}

__declspec(dllexport) void *op_d3d9_probe_vtable(void) {
    return g_probe_vtable;
}

/* Original Present function pointer */
typedef HRESULT (WINAPI *PFN_Present)(IDirect3DDevice9*, const void*, const void*, void*, const void*);
static PFN_Present g_orig_present = NULL;

__declspec(dllexport) void *op_d3d9_probe_present(void) {
    return g_target_present;
}

/* ------------------------------------------------------------------ */
/* Hooked Present                                                      */
/* ------------------------------------------------------------------ */

static HRESULT WINAPI hook_present(IDirect3DDevice9 *dev, const void *src,
                                   const void *dst, void *wnd, const void *dirty) {
    InterlockedIncrement(&g_hook_calls);
    if (g_shared_mem && dev) {
        EnterCriticalSection(&g_capture_lock);
        IDirect3DSurface9 *backbuf = NULL;
        if (g_capture_device != dev) {
            if (g_readback) { g_readback->lpVtbl->Release(g_readback); g_readback = NULL; }
            g_capture_device = dev;
            g_readback_w = g_readback_h = g_readback_format = 0;
        }
        /* GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO=0, &surface) */
        HRESULT hr = dev->lpVtbl->GetBackBuffer(dev, 0, 0, 0, &backbuf);
        if (SUCCEEDED(hr) && backbuf) {
            D3DSURFACE_DESC desc;
            hr = backbuf->lpVtbl->GetDesc(backbuf, &desc);
            if (SUCCEEDED(hr) &&
                (desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_X8R8G8B8) &&
                op_hook_frame_fits(desc.Width, desc.Height, g_map_size, sizeof(op_hook_header))) {
                /* 默认交换链后缓冲区不可锁；先复制到可读的系统内存 surface。 */
                if (!g_readback || g_readback_w != desc.Width || g_readback_h != desc.Height ||
                    g_readback_format != desc.Format) {
                    if (g_readback) { g_readback->lpVtbl->Release(g_readback); g_readback = NULL; }
                    hr = dev->lpVtbl->CreateOffscreenPlainSurface(dev, desc.Width, desc.Height,
                               desc.Format, D3DPOOL_SYSTEMMEM, &g_readback, NULL);
                    if (SUCCEEDED(hr) && g_readback) {
                        g_readback_w = desc.Width;
                        g_readback_h = desc.Height;
                        g_readback_format = desc.Format;
                    }
                }
                if (g_readback) hr = dev->lpVtbl->GetRenderTargetData(dev, backbuf, g_readback);
                if (SUCCEEDED(hr) && g_readback) {
                    D3DLOCKED_RECT lr;
                    hr = g_readback->lpVtbl->LockRect(g_readback, &lr, NULL, D3DLOCK_READONLY);
                    if (SUCCEEDED(hr)) {
                        if (lr.pBits && lr.Pitch > 0 && (unsigned int)lr.Pitch >= desc.Width * 4u) {
                            op_hook_write_frame(g_shared_mem, desc.Width, desc.Height,
                                                lr.pBits, (unsigned int)lr.Pitch);
                            InterlockedExchange(&g_hook_error, 0);
                        }
                        g_readback->lpVtbl->UnlockRect(g_readback);
                    }
                }
                if (FAILED(hr)) InterlockedExchange(&g_hook_error, (LONG)hr);
            }
            backbuf->lpVtbl->Release(backbuf);
        }
        LeaveCriticalSection(&g_capture_lock);
    }
    return g_orig_present(dev, src, dst, wnd, dirty);
}

/* ------------------------------------------------------------------ */
/* 函数级拦截：不同设备可以各有 vtable，但指向同一个 Present 实现。 */
/* ------------------------------------------------------------------ */

static int hook_present_function(IDirect3DDevice9 *dev) {
    MH_STATUS status;
    LPVOID trampoline = NULL;
    LPVOID target;
    HMODULE pinned;
    if (!dev) return 0;
    void **vtable = *(void***)dev;
    target = vtable[17];
    g_probe_vtable = vtable;
    g_target_present = target;
    status = MH_Initialize();
    if (status != MH_OK) goto failed;
    status = MH_CreateHook(target, (LPVOID)hook_present, &trampoline);
    if (status != MH_OK) goto failed;
    memcpy(&g_orig_present, &trampoline, sizeof(g_orig_present));
    status = MH_EnableHook(target);
    if (status != MH_OK) {
        MH_RemoveHook(target);
        MH_Uninitialize();
        goto failed;
    }
    /* 注入 DLL 随目标进程存活；禁止卸载仍被 Present 跳转引用的代码。 */
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCSTR)&g_hook_status, &pinned)) {
        InterlockedExchange(&g_hook_error, (LONG)GetLastError());
        MH_DisableHook(target);
        MH_RemoveHook(target);
        MH_Uninitialize();
        return 0;
    }
    return 1;
failed:
    InterlockedExchange(&g_hook_error, (LONG)status);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Find D3D9 device by scanning for d3d9.dll                           */
/* ------------------------------------------------------------------ */

static void try_find_and_hook(void) {
    /* 工作线程内创建探针设备，避免在 DllMain 的 loader lock 下加载图形运行库。 */
    HWND window = CreateWindowExW(0, L"STATIC", L"op d3d9 probe", WS_POPUP,
                                   0, 0, 1, 1, NULL, NULL, NULL, NULL);
    if (!window) { InterlockedExchange(&g_hook_status, -1); return; }
    HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
    if (!d3d9) d3d9 = LoadLibraryA("d3d9.dll");
    if (!d3d9) { InterlockedExchange(&g_hook_status, -2); goto finish; }

    typedef void* (WINAPI *PFN_Direct3DCreate9)(unsigned int);
    FARPROC symbol = GetProcAddress(d3d9, "Direct3DCreate9");
    PFN_Direct3DCreate9 pCreate = NULL;
    if (sizeof(symbol) == sizeof(pCreate)) memcpy(&pCreate, &symbol, sizeof(pCreate));
    if (!pCreate) { InterlockedExchange(&g_hook_status, -3); goto finish; }

    /* IDirect3D9 vtable: IUnknown(3) + padding(13) + CreateDevice at index 16 */
    typedef struct IDirect3D9 IDirect3D9;
    typedef struct IDirect3D9Vtbl {
        HRESULT (WINAPI *QueryInterface)(IDirect3D9*, const void*, void**);
        ULONG   (WINAPI *AddRef)(IDirect3D9*);
        ULONG   (WINAPI *Release)(IDirect3D9*);
        void *m[13];
        HRESULT (WINAPI *CreateDevice)(IDirect3D9*, unsigned int, unsigned int, void*,
                                       unsigned int, void*, IDirect3DDevice9**);
    } IDirect3D9Vtbl;
    struct IDirect3D9 { const IDirect3D9Vtbl *lpVtbl; };

    IDirect3D9 *d3d = (IDirect3D9*)pCreate(32); /* D3D_SDK_VERSION = 32 */
    if (!d3d) { InterlockedExchange(&g_hook_status, -4); goto finish; }

    /* Use a minimal presentation parameters struct */
    typedef struct {
        unsigned int BackBufferWidth;
        unsigned int BackBufferHeight;
        unsigned int BackBufferFormat;
        unsigned int BackBufferCount;
        unsigned int MultiSampleType;
        unsigned int MultiSampleQuality;
        unsigned int SwapEffect;
        void *hDeviceWindow;
        int Windowed;
        int EnableAutoDepthStencil;
        unsigned int AutoDepthStencilFormat;
        unsigned int Flags;
        unsigned int FullScreen_RefreshRateInHz;
        unsigned int PresentationInterval;
    } D3DPRESENT_PARAMETERS;

    D3DPRESENT_PARAMETERS pp;
    memset(&pp, 0, sizeof(pp));
    pp.BackBufferWidth = 1;
    pp.BackBufferHeight = 1;
    pp.BackBufferFormat = 0; /* D3DFMT_UNKNOWN */
    pp.BackBufferCount = 1;
    pp.SwapEffect = 1; /* D3DSWAPEFFECT_DISCARD */
    pp.hDeviceWindow = window;
    pp.Windowed = 1;
    pp.PresentationInterval = 0x80000000; /* D3DPRESENT_INTERVAL_IMMEDIATE */

    IDirect3DDevice9 *dev = NULL;
    HRESULT hr = d3d->lpVtbl->CreateDevice(d3d, 0, 1/*HAL*/, window,
                                            0x20/*SOFTWARE_VERTEXPROCESSING*/, &pp, &dev);
    if (SUCCEEDED(hr) && dev) {
        InterlockedExchange(&g_hook_status, hook_present_function(dev) ? 1 : -6);
        dev->lpVtbl->Release(dev);
    } else { InterlockedExchange(&g_hook_error, (LONG)hr); InterlockedExchange(&g_hook_status, -5); }
    d3d->lpVtbl->Release(d3d);
finish:
    DestroyWindow(window);
}

static DWORD WINAPI op_init_hook_thread(void *unused) {
    (void)unused;
    try_find_and_hook();
    return 0u;
}

/* ------------------------------------------------------------------ */
/* DLL Entry                                                           */
/* ------------------------------------------------------------------ */

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, void *reserved) {
    (void)hinst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        InitializeCriticalSection(&g_capture_lock);

        /* Open shared memory created by op_native.c */
        DWORD pid = GetCurrentProcessId();
        g_shared_mem = op_hook_open_shared(pid, g_map_size);

        if (g_shared_mem) {
            HANDLE thread = CreateThread(NULL, 0, op_init_hook_thread, NULL, 0, NULL);
            if (thread) CloseHandle(thread);
            else InterlockedExchange(&g_hook_status, -7);
        } else InterlockedExchange(&g_hook_status, -8);
    } else if (reason == DLL_PROCESS_DETACH && reserved == NULL) {
        if (g_readback) { g_readback->lpVtbl->Release(g_readback); g_readback = NULL; }
        if (g_shared_mem) {
            UnmapViewOfFile(g_shared_mem);
            g_shared_mem = NULL;
        }
        DeleteCriticalSection(&g_capture_lock);
    }
    return TRUE;
}
