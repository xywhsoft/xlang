/*
 * op_d3d9_hook.c - D3D9 Present Hook DLL
 * Injected into target process, hooks IDirect3DDevice9::Present via vtable patch.
 * Captures back buffer to shared memory after each Present call.
 * TCC-compatible: compile with `tcc -shared -o op_d3d9_hook.dll op_d3d9_hook.c -luser32 -lgdi32`
 */
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "op_hook_shared.h"

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

#define D3DPOOL_SYSTEMMEM 0
#define D3DFMT_A8R8G8B8  21
#define D3DLOCK_READONLY  0x10

/* IDirect3DSurface9 vtable (IUnknown=3 + IDirect3DResource9=8 + Surface=4) */
typedef struct IDirect3DSurface9Vtbl {
    HRESULT (WINAPI *QueryInterface)(IDirect3DSurface9*, const void*, void**);
    ULONG   (WINAPI *AddRef)(IDirect3DSurface9*);
    ULONG   (WINAPI *Release)(IDirect3DSurface9*);
    /* IDirect3DResource9 */
    void *m[8];
    /* IDirect3DSurface9 */
    void *p1[2]; /* GetContainer, GetDesc */
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
    /* slot 18+: more methods */
    void *m2[3]; /* GetBackBuffer is at 21 */
    void *m3[2];
    HRESULT (WINAPI *GetBackBuffer)(IDirect3DDevice9*, unsigned int, unsigned int, unsigned int, IDirect3DSurface9**);
    void *m4[28]; /* up to CreateOffscreenPlainSurface at slot 52? */
    /* We'll use GetRenderTarget (slot 21+?) - actually let's use a simpler approach */
} IDirect3DDevice9Vtbl;
struct IDirect3DDevice9 { const IDirect3DDevice9Vtbl *lpVtbl; };

/* ------------------------------------------------------------------ */
/* Hook state                                                          */
/* ------------------------------------------------------------------ */

static void *g_shared_mem = NULL;
static unsigned int g_map_size = 4 * 1024 * 1024;
static IDirect3DDevice9 *g_device = NULL;

/* Original Present function pointer */
typedef HRESULT (WINAPI *PFN_Present)(IDirect3DDevice9*, const void*, const void*, void*, const void*);
static PFN_Present g_orig_present = NULL;

/* ------------------------------------------------------------------ */
/* Hooked Present                                                      */
/* ------------------------------------------------------------------ */

static HRESULT WINAPI hook_present(IDirect3DDevice9 *dev, const void *src,
                                   const void *dst, void *wnd, const void *dirty) {
    /* Capture back buffer before presenting */
    if (g_shared_mem && dev) {
        IDirect3DSurface9 *backbuf = NULL;
        /* GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO=0, &surface) */
        HRESULT hr = dev->lpVtbl->GetBackBuffer(dev, 0, 0, 0, &backbuf);
        if (SUCCEEDED(hr) && backbuf) {
            D3DLOCKED_RECT lr;
            hr = backbuf->lpVtbl->LockRect(backbuf, &lr, NULL, D3DLOCK_READONLY);
            if (SUCCEEDED(hr)) {
                /* Get surface dimensions via desc - use pitch heuristic */
                /* We'll read width from shared mem header if set, otherwise guess */
                op_hook_header *hdr = (op_hook_header *)g_shared_mem;
                unsigned int w = hdr->width;
                unsigned int h = hdr->height;
                if (w == 0 || h == 0) {
                    /* First frame: try to get dimensions from client rect */
                    RECT rc;
                    if (wnd && IsWindow((HWND)wnd)) {
                        GetClientRect((HWND)wnd, &rc);
                        w = rc.right - rc.left;
                        h = rc.bottom - rc.top;
                    }
                    if (w == 0 || h == 0) { w = 800; h = 600; }
                }
                if (w * h * 4 + sizeof(op_hook_header) <= g_map_size) {
                    op_hook_write_frame(g_shared_mem, w, h, lr.pBits, lr.Pitch);
                }
                backbuf->lpVtbl->UnlockRect(backbuf);
            }
            backbuf->lpVtbl->Release(backbuf);
        }
    }
    return g_orig_present(dev, src, dst, wnd, dirty);
}

/* ------------------------------------------------------------------ */
/* Vtable patching                                                     */
/* ------------------------------------------------------------------ */

static int patch_device_vtable(IDirect3DDevice9 *dev) {
    if (!dev) return 0;
    /* Present is at vtable index 17 */
    void **vtable = *(void***)dev;
    g_orig_present = (PFN_Present)vtable[17];

    DWORD old_prot;
    if (!VirtualProtect(&vtable[17], sizeof(void*), PAGE_READWRITE, &old_prot))
        return 0;
    vtable[17] = (void*)hook_present;
    VirtualProtect(&vtable[17], sizeof(void*), old_prot, &old_prot);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Find D3D9 device by scanning for d3d9.dll                           */
/* ------------------------------------------------------------------ */

static void try_find_and_hook(void) {
    /* Create a temporary D3D9 device to get the vtable address */
    HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
    if (!d3d9) d3d9 = LoadLibraryA("d3d9.dll");
    if (!d3d9) return;

    typedef void* (WINAPI *PFN_Direct3DCreate9)(unsigned int);
    PFN_Direct3DCreate9 pCreate = (PFN_Direct3DCreate9)GetProcAddress(d3d9, "Direct3DCreate9");
    if (!pCreate) return;

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
    if (!d3d) return;

    /* Create a dummy device to get vtable */
    typedef struct {
        unsigned int AdapterOrdinal;
        unsigned int DeviceType;
        void *hFocusWindow;
        unsigned int BehaviorFlags;
        void *pPresentationParameters;
        IDirect3DDevice9 **ppReturnedDeviceInterface;
    } dummy_params;

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
    pp.hDeviceWindow = GetDesktopWindow();
    pp.Windowed = 1;
    pp.PresentationInterval = 0x80000000; /* D3DPRESENT_INTERVAL_IMMEDIATE */

    IDirect3DDevice9 *dev = NULL;
    HRESULT hr = d3d->lpVtbl->CreateDevice(d3d, 0, 1/*HAL*/, GetDesktopWindow(),
                                            0x20/*MIXED_VERTEXPROCESSING*/, &pp, &dev);
    if (SUCCEEDED(hr) && dev) {
        patch_device_vtable(dev);
        g_device = dev;
        /* Don't release - we need the vtable patched */
    }
    d3d->lpVtbl->Release(d3d);
}

/* ------------------------------------------------------------------ */
/* DLL Entry                                                           */
/* ------------------------------------------------------------------ */

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, void *reserved) {
    (void)hinst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);

        /* Open shared memory created by op_native.c */
        DWORD pid = GetCurrentProcessId();
        g_shared_mem = op_hook_open_shared(pid, g_map_size);

        /* Find and hook D3D9 device */
        try_find_and_hook();
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_shared_mem) {
            UnmapViewOfFile(g_shared_mem);
            g_shared_mem = NULL;
        }
    }
    return TRUE;
}
