/*
 * op_d3d11_hook.c - D3D11 SwapChain::Present Hook DLL
 * Injected into target process, hooks IDXGISwapChain::Present via vtable patch.
 * Captures back buffer to shared memory after each Present call.
 * TCC-compatible: compile with `tcc -shared -o op_d3d11_hook.dll op_d3d11_hook.c -luser32 -lgdi32 -ld3d11 -ldxgi`
 */
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include "op_hook_shared.h"

/* ------------------------------------------------------------------ */
/* Minimal COM/D3D11 declarations for TCC                              */
/* ------------------------------------------------------------------ */

typedef struct { unsigned long Data1; unsigned short Data2, Data3; unsigned char Data4[8]; } HGUID;
static const HGUID IID_ID3D11Texture2D = {0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};
static const HGUID IID_ID3D11Device = {0xdb6f6ddb,0xac77,0x4e88,{0x82,0x53,0x81,0x9d,0xf9,0xbb,0xf1,0x40}};

typedef struct {
    unsigned int Width, Height, MipLevels, ArraySize, Format;
    struct { unsigned int Count, Quality; } SampleDesc;
    unsigned int Usage, BindFlags, CPUAccessFlags, MiscFlags;
} TEX2D_DESC;

typedef struct {
    void *pData;
    unsigned int RowPitch, DepthPitch;
} MAPPED_SUB;
typedef char op_mapped_pointer_offset_check[(offsetof(MAPPED_SUB, pData) == 0u) ? 1 : -1];
typedef char op_mapped_size_check[(sizeof(MAPPED_SUB) == 16u) ? 1 : -1];

/* IDXGISwapChain vtable: Present is at index 8 */
typedef struct IDXGISwapChain IDXGISwapChain;
typedef struct IDXGISwapChainVtbl {
    HRESULT (WINAPI *QueryInterface)(IDXGISwapChain*, const void*, void**);
    ULONG   (WINAPI *AddRef)(IDXGISwapChain*);
    ULONG   (WINAPI *Release)(IDXGISwapChain*);
    /* IDXGIObject: 4 */
    void *o[4];
    /* IDXGIDeviceSubObject: GetDevice (slot 7) */
    HRESULT (WINAPI *GetDevice)(IDXGISwapChain*, const void*, void**);
    /* IDXGISwapChain::Present (index 8) */
    HRESULT (WINAPI *Present)(IDXGISwapChain*, unsigned int, unsigned int);
    /* GetBuffer (index 9) */
    HRESULT (WINAPI *GetBuffer)(IDXGISwapChain*, unsigned int, const void*, void**);
    void *rest[6];
} IDXGISwapChainVtbl;
struct IDXGISwapChain { const IDXGISwapChainVtbl *lpVtbl; };

/* ID3D11Texture2D - GetDesc at index 10 */
typedef struct ID3D11Texture2D ID3D11Texture2D;
typedef struct ID3D11Texture2DVtbl {
    HRESULT (WINAPI *QueryInterface)(ID3D11Texture2D*, const void*, void**);
    ULONG   (WINAPI *AddRef)(ID3D11Texture2D*);
    ULONG   (WINAPI *Release)(ID3D11Texture2D*);
    void *child[4]; /* ID3D11DeviceChild */
    void *res[3];   /* ID3D11Resource */
    void (WINAPI *GetDesc)(ID3D11Texture2D*, TEX2D_DESC*);
} ID3D11Texture2DVtbl;
struct ID3D11Texture2D { const ID3D11Texture2DVtbl *lpVtbl; };

/* ID3D11Device - CreateTexture2D at index 5, GetImmediateContext at index 40 */
typedef struct ID3D11Device ID3D11Device;
typedef struct ID3D11DeviceVtbl {
    HRESULT (WINAPI *QueryInterface)(ID3D11Device*, const void*, void**);
    ULONG   (WINAPI *AddRef)(ID3D11Device*);
    ULONG   (WINAPI *Release)(ID3D11Device*);
    void *m1[2]; /* CreateBuffer, CreateTexture1D */
    HRESULT (WINAPI *CreateTexture2D)(ID3D11Device*, const TEX2D_DESC*, const void*, ID3D11Texture2D**);
    void *m2[34];
    void (WINAPI *GetImmediateContext)(ID3D11Device*, void**);
} ID3D11DeviceVtbl;
struct ID3D11Device { const ID3D11DeviceVtbl *lpVtbl; };

/* ID3D11DeviceContext - CopyResource at 47, Map at 14, Unmap at 15 */
typedef struct ID3D11DeviceContext ID3D11DeviceContext;
typedef struct ID3D11DeviceContextVtbl {
    HRESULT (WINAPI *QueryInterface)(ID3D11DeviceContext*, const void*, void**);
    ULONG   (WINAPI *AddRef)(ID3D11DeviceContext*);
    ULONG   (WINAPI *Release)(ID3D11DeviceContext*);
    void *child[4];
    void *pre_map[7]; /* slots 7-13 */
    HRESULT (WINAPI *Map)(ID3D11DeviceContext*, void*, unsigned int, unsigned int, unsigned int, MAPPED_SUB*);
    void (WINAPI *Unmap)(ID3D11DeviceContext*, void*, unsigned int);
    void *mid[31]; /* slots 16-46 */
    void (WINAPI *CopyResource)(ID3D11DeviceContext*, void*, void*);
} ID3D11DeviceContextVtbl;
struct ID3D11DeviceContext { const ID3D11DeviceContextVtbl *lpVtbl; };

typedef char op_d3d11_present_slot_check[(offsetof(IDXGISwapChainVtbl, Present) / sizeof(void*) == 8u) ? 1 : -1];
typedef char op_d3d11_device_slot_check[(offsetof(IDXGISwapChainVtbl, GetDevice) / sizeof(void*) == 7u) ? 1 : -1];
typedef char op_d3d11_buffer_slot_check[(offsetof(IDXGISwapChainVtbl, GetBuffer) / sizeof(void*) == 9u) ? 1 : -1];
typedef char op_d3d11_context_slot_check[(offsetof(ID3D11DeviceVtbl, GetImmediateContext) / sizeof(void*) == 40u) ? 1 : -1];
typedef char op_d3d11_copy_slot_check[(offsetof(ID3D11DeviceContextVtbl, CopyResource) / sizeof(void*) == 47u) ? 1 : -1];

/* ------------------------------------------------------------------ */
/* Hook state                                                          */
/* ------------------------------------------------------------------ */

static void *g_shared_mem = NULL;
static unsigned int g_map_size = OP_HOOK_MAP_SIZE;
static CRITICAL_SECTION g_capture_lock;

typedef HRESULT (WINAPI *PFN_Present)(IDXGISwapChain*, unsigned int, unsigned int);
static PFN_Present g_orig_present = NULL;
static ID3D11Device *g_device = NULL;
static ID3D11DeviceContext *g_context = NULL;
static ID3D11Texture2D *g_staging = NULL;
static unsigned int g_stag_w = 0, g_stag_h = 0, g_stag_format = 0;
static volatile LONG g_hook_status = 0;
static volatile LONG g_hook_error = 0;
static volatile LONG g_hook_calls = 0;
static volatile LONG g_capture_stage = 0;

__declspec(dllexport) LONG op_d3d11_hook_status(void) {
    return InterlockedCompareExchange(&g_hook_status, 0, 0);
}

__declspec(dllexport) LONG op_d3d11_hook_error(void) {
    return InterlockedCompareExchange(&g_hook_error, 0, 0);
}

__declspec(dllexport) LONG op_d3d11_hook_calls(void) {
    return InterlockedCompareExchange(&g_hook_calls, 0, 0);
}

__declspec(dllexport) LONG op_d3d11_capture_stage(void) {
    return InterlockedCompareExchange(&g_capture_stage, 0, 0);
}

typedef struct {
    unsigned int Width, Height;
    struct { unsigned int Numerator, Denominator; } RefreshRate;
    unsigned int Format, ScanlineOrdering, Scaling;
    struct { unsigned int Count, Quality; } SampleDesc;
    unsigned int BufferUsage, BufferCount;
    void *OutputWindow;
    int Windowed;
    unsigned int SwapEffect, Flags;
} OP_SWAP_DESC;
typedef char op_swap_desc_size_check[(sizeof(OP_SWAP_DESC) == 72u) ? 1 : -1];

/* ------------------------------------------------------------------ */
/* Hooked Present                                                      */
/* ------------------------------------------------------------------ */

static HRESULT WINAPI hook_present(IDXGISwapChain *sc, unsigned int sync, unsigned int flags) {
    ID3D11Device *actual = NULL;
    ID3D11Texture2D *backbuf = NULL;
    TEX2D_DESC desc;
    MAPPED_SUB mapped;
    HRESULT hr;
    unsigned int w, h;

    InterlockedIncrement(&g_hook_calls);
    if (!g_shared_mem || !sc) return g_orig_present(sc, sync, flags);
    g_capture_stage = 1;
    EnterCriticalSection(&g_capture_lock);
    hr = sc->lpVtbl->GetDevice(sc, &IID_ID3D11Device, (void**)&actual);
    if (FAILED(hr) || !actual) goto done;
    g_capture_stage = 2;
    if (actual != g_device) {
        if (g_staging) { g_staging->lpVtbl->Release(g_staging); g_staging = NULL; }
        if (g_context) { g_context->lpVtbl->Release(g_context); g_context = NULL; }
        if (g_device) g_device->lpVtbl->Release(g_device);
        g_device = actual;
        actual = NULL;
        g_stag_w = g_stag_h = g_stag_format = 0;
        g_device->lpVtbl->GetImmediateContext(g_device, (void**)&g_context);
    }
    if (!g_context) goto done;
    g_capture_stage = 3;
    hr = sc->lpVtbl->GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void**)&backbuf);
    if (FAILED(hr) || !backbuf) goto done;
    g_capture_stage = 4;
    backbuf->lpVtbl->GetDesc(backbuf, &desc);
    w = desc.Width;
    h = desc.Height;

    /* 只发布 BGRA 单采样帧；先核尺寸，再申请目标设备的 staging。 */
    if (desc.Format != 87u || desc.SampleDesc.Count != 1u ||
        !op_hook_frame_fits(w, h, g_map_size, sizeof(op_hook_header))) goto done;
    g_capture_stage = 5;
    if (!g_staging || g_stag_w != w || g_stag_h != h || g_stag_format != desc.Format) {
        TEX2D_DESC sd;
        if (g_staging) { g_staging->lpVtbl->Release(g_staging); g_staging = NULL; }
        memset(&sd, 0, sizeof(sd));
        sd.Width = w; sd.Height = h;
        sd.MipLevels = 1; sd.ArraySize = 1;
        sd.Format = desc.Format;
        sd.SampleDesc.Count = 1;
        sd.Usage = 3; /* D3D11_USAGE_STAGING */
        sd.CPUAccessFlags = 0x20000; /* D3D11_CPU_ACCESS_READ */
        hr = g_device->lpVtbl->CreateTexture2D(g_device, &sd, NULL, &g_staging);
        if (FAILED(hr) || !g_staging) {
            g_stag_w = g_stag_h = g_stag_format = 0;
            goto done;
        }
        g_stag_w = w; g_stag_h = h; g_stag_format = desc.Format;
    }
    g_capture_stage = 6;

    g_context->lpVtbl->CopyResource(g_context, g_staging, backbuf);
    memset(&mapped, 0, sizeof(mapped));
    hr = g_context->lpVtbl->Map(g_context, g_staging, 0, 1/*READ*/, 0, &mapped);
    if (SUCCEEDED(hr)) {
        g_capture_stage = 7;
        if (mapped.pData && mapped.RowPitch >= w * 4u)
            op_hook_write_frame(g_shared_mem, w, h, mapped.pData, mapped.RowPitch);
        g_context->lpVtbl->Unmap(g_context, g_staging, 0);
        g_hook_error = 0;
        g_capture_stage = 8;
    } else g_hook_error = (LONG)hr;

done:
    if (backbuf) backbuf->lpVtbl->Release(backbuf);
    if (actual) actual->lpVtbl->Release(actual);
    LeaveCriticalSection(&g_capture_lock);
    return g_orig_present(sc, sync, flags);
}

/* ------------------------------------------------------------------ */
/* Create dummy swapchain to get vtable, then patch                    */
/* ------------------------------------------------------------------ */

static void try_find_and_hook(void) {
    LONG status = -1;
    HWND window = CreateWindowExW(0, L"STATIC", L"op hook probe", WS_POPUP,
                                   0, 0, 1, 1, NULL, NULL, NULL, NULL);
    if (!window) { g_hook_error = (LONG)GetLastError(); goto finish; }
    status = -2;
    HMODULE d3d11 = GetModuleHandleA("d3d11.dll");
    if (!d3d11) d3d11 = LoadLibraryA("d3d11.dll");
    if (!d3d11) { g_hook_error = (LONG)GetLastError(); goto finish; }

    typedef HRESULT (WINAPI *PFN_D3D11CreateDeviceAndSwapChain)(
        void*, int, void*, unsigned int, const void*, unsigned int, unsigned int,
        const void*, IDXGISwapChain**, ID3D11Device**, void*, ID3D11DeviceContext**);
    FARPROC symbol = GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain");
    PFN_D3D11CreateDeviceAndSwapChain pCreate = NULL;
    status = -3;
    if (sizeof(symbol) != sizeof(pCreate)) goto finish;
    memcpy(&pCreate, &symbol, sizeof(pCreate));
    if (!pCreate) goto finish;

    OP_SWAP_DESC scd;
    memset(&scd, 0, sizeof(scd));
    scd.Width = 1; scd.Height = 1;
    scd.Format = 87; /* DXGI_FORMAT_B8G8R8A8_UNORM */
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = 0x20; /* DXGI_USAGE_RENDER_TARGET_OUTPUT */
    scd.BufferCount = 1;
    scd.OutputWindow = window;
    scd.Windowed = 1;
    scd.SwapEffect = 0;

    unsigned int feature_level = 0xb000; /* D3D_FEATURE_LEVEL_11_0 */
    IDXGISwapChain *sc = NULL;
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext *ctx = NULL;

    HRESULT hr = pCreate(NULL, 1/*HARDWARE*/, NULL, 0, &feature_level, 1, 7/*SDK*/,
                         &scd, &sc, &dev, NULL, &ctx);
    status = -4;
    if (FAILED(hr) || !sc || !dev || !ctx) {
        g_hook_error = (LONG)hr;
        if (sc) sc->lpVtbl->Release(sc);
        if (ctx) ctx->lpVtbl->Release(ctx);
        if (dev) dev->lpVtbl->Release(dev);
        goto finish;
    }

    /* Patch Present in vtable (index 8) */
    void **vtable = *(void***)sc;
    g_orig_present = (PFN_Present)vtable[8];

    DWORD old_prot;
    status = -5;
    if (VirtualProtect(&vtable[8], sizeof(void*), PAGE_READWRITE, &old_prot)) {
        vtable[8] = (void*)hook_present;
        VirtualProtect(&vtable[8], sizeof(void*), old_prot, &old_prot);
        status = 1;
    } else {
        g_hook_error = (LONG)GetLastError();
    }

    sc->lpVtbl->Release(sc);
    ctx->lpVtbl->Release(ctx);
    dev->lpVtbl->Release(dev);

finish:
    if (window) DestroyWindow(window);
    InterlockedExchange(&g_hook_status, status);
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
        DWORD pid = GetCurrentProcessId();
        g_shared_mem = op_hook_open_shared(pid, g_map_size);
        if (g_shared_mem) {
            HANDLE thread = CreateThread(NULL, 0, op_init_hook_thread, NULL, 0, NULL);
            if (thread) CloseHandle(thread);
            else g_hook_status = -6;
        }
        else g_hook_status = -7;
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_staging) { g_staging->lpVtbl->Release(g_staging); g_staging = NULL; }
        if (g_context) { g_context->lpVtbl->Release(g_context); g_context = NULL; }
        if (g_device) { g_device->lpVtbl->Release(g_device); g_device = NULL; }
        if (g_shared_mem) { UnmapViewOfFile(g_shared_mem); g_shared_mem = NULL; }
        DeleteCriticalSection(&g_capture_lock);
    }
    return TRUE;
}
