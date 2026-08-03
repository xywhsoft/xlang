/*
 * op_d3d11_hook.c - D3D11 SwapChain::Present Hook DLL
 * Injected into target process, hooks IDXGISwapChain::Present via vtable patch.
 * Captures back buffer to shared memory after each Present call.
 * TCC-compatible: compile with `tcc -shared -o op_d3d11_hook.dll op_d3d11_hook.c -luser32 -lgdi32 -ld3d11 -ldxgi`
 */
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "op_hook_shared.h"

/* ------------------------------------------------------------------ */
/* Minimal COM/D3D11 declarations for TCC                              */
/* ------------------------------------------------------------------ */

typedef struct { unsigned long Data1; unsigned short Data2, Data3; unsigned char Data4[8]; } HGUID;
static const HGUID IID_ID3D11Texture2D = {0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};

typedef struct IUnknown IUnknown;
typedef struct IUnknownVtbl {
    HRESULT (WINAPI *QueryInterface)(IUnknown*, const void*, void**);
    ULONG   (WINAPI *AddRef)(IUnknown*);
    ULONG   (WINAPI *Release)(IUnknown*);
} IUnknownVtbl;
struct IUnknown { const IUnknownVtbl *lpVtbl; };

typedef struct {
    unsigned int Width, Height, MipLevels, ArraySize, Format;
    struct { unsigned int Count, Quality; } SampleDesc;
    unsigned int Usage, BindFlags, CPUAccessFlags, MiscFlags;
} TEX2D_DESC;

typedef struct {
    unsigned int RowPitch, DepthPitch;
    void *pData;
} MAPPED_SUB;

/* IDXGISwapChain vtable: Present is at index 8 */
typedef struct IDXGISwapChain IDXGISwapChain;
typedef struct IDXGISwapChainVtbl {
    HRESULT (WINAPI *QueryInterface)(IDXGISwapChain*, const void*, void**);
    ULONG   (WINAPI *AddRef)(IDXGISwapChain*);
    ULONG   (WINAPI *Release)(IDXGISwapChain*);
    /* IDXGIObject: 4 */
    void *o[4];
    /* IDXGIDeviceSubObject: 3 */
    void *d[3];
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
    void *m2[35];
    void (WINAPI *GetImmediateContext)(ID3D11Device*, void**);
} ID3D11DeviceVtbl;
struct ID3D11Device { const ID3D11DeviceVtbl *lpVtbl; };

/* ID3D11DeviceContext - CopyResource at index 98, Map at 14, Unmap at 15 */
typedef struct ID3D11DeviceContext ID3D11DeviceContext;
typedef struct ID3D11DeviceContextVtbl {
    HRESULT (WINAPI *QueryInterface)(ID3D11DeviceContext*, const void*, void**);
    ULONG   (WINAPI *AddRef)(ID3D11DeviceContext*);
    ULONG   (WINAPI *Release)(ID3D11DeviceContext*);
    void *child[4];
    void *pre_map[7]; /* slots 7-13 */
    HRESULT (WINAPI *Map)(ID3D11DeviceContext*, void*, unsigned int, unsigned int, unsigned int, MAPPED_SUB*);
    void (WINAPI *Unmap)(ID3D11DeviceContext*, void*, unsigned int);
    void *mid[80]; /* slots 16-95 */
    void *slot96[1]; /* FinishCommandList */
    void (WINAPI *CopySubresourceRegion)(ID3D11DeviceContext*, void*, unsigned int, unsigned int, unsigned int, unsigned int, void*, unsigned int, const void*);
    void (WINAPI *CopyResource)(ID3D11DeviceContext*, void*, void*);
} ID3D11DeviceContextVtbl;
struct ID3D11DeviceContext { const ID3D11DeviceContextVtbl *lpVtbl; };

/* ------------------------------------------------------------------ */
/* Hook state                                                          */
/* ------------------------------------------------------------------ */

static void *g_shared_mem = NULL;
static unsigned int g_map_size = 4 * 1024 * 1024;

typedef HRESULT (WINAPI *PFN_Present)(IDXGISwapChain*, unsigned int, unsigned int);
static PFN_Present g_orig_present = NULL;
static ID3D11Device *g_device = NULL;
static ID3D11DeviceContext *g_context = NULL;
static ID3D11Texture2D *g_staging = NULL;
static unsigned int g_stag_w = 0, g_stag_h = 0;

/* ------------------------------------------------------------------ */
/* Hooked Present                                                      */
/* ------------------------------------------------------------------ */

static HRESULT WINAPI hook_present(IDXGISwapChain *sc, unsigned int sync, unsigned int flags) {
    if (g_shared_mem && sc && g_device && g_context) {
        ID3D11Texture2D *backbuf = NULL;
        HRESULT hr = sc->lpVtbl->GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void**)&backbuf);
        if (SUCCEEDED(hr) && backbuf) {
            TEX2D_DESC desc;
            backbuf->lpVtbl->GetDesc(backbuf, &desc);
            unsigned int w = desc.Width, h = desc.Height;

            /* Create/recreate staging texture if needed */
            if (!g_staging || g_stag_w != w || g_stag_h != h) {
                if (g_staging) { g_staging->lpVtbl->Release(g_staging); g_staging = NULL; }
                TEX2D_DESC sd;
                memset(&sd, 0, sizeof(sd));
                sd.Width = w; sd.Height = h;
                sd.MipLevels = 1; sd.ArraySize = 1;
                sd.Format = desc.Format;
                sd.SampleDesc.Count = 1;
                sd.Usage = 3; /* D3D11_USAGE_STAGING */
                sd.CPUAccessFlags = 0x10000; /* D3D11_CPU_ACCESS_READ */
                g_device->lpVtbl->CreateTexture2D(g_device, &sd, NULL, &g_staging);
                g_stag_w = w; g_stag_h = h;
            }

            if (g_staging && w * h * 4 + sizeof(op_hook_header) <= g_map_size) {
                g_context->lpVtbl->CopyResource(g_context, g_staging, backbuf);
                MAPPED_SUB mapped;
                hr = g_context->lpVtbl->Map(g_context, g_staging, 0, 1/*READ*/, 0, &mapped);
                if (SUCCEEDED(hr)) {
                    op_hook_write_frame(g_shared_mem, w, h, mapped.pData, mapped.RowPitch);
                    g_context->lpVtbl->Unmap(g_context, g_staging, 0);
                }
            }
            backbuf->lpVtbl->Release(backbuf);
        }
    }
    return g_orig_present(sc, sync, flags);
}

/* ------------------------------------------------------------------ */
/* Create dummy swapchain to get vtable, then patch                    */
/* ------------------------------------------------------------------ */

static void try_find_and_hook(void) {
    HMODULE d3d11 = GetModuleHandleA("d3d11.dll");
    if (!d3d11) d3d11 = LoadLibraryA("d3d11.dll");
    if (!d3d11) return;

    typedef HRESULT (WINAPI *PFN_D3D11CreateDeviceAndSwapChain)(
        void*, int, void*, unsigned int, const void*, unsigned int, unsigned int,
        const void*, IDXGISwapChain**, ID3D11Device**, void*, ID3D11DeviceContext**);
    PFN_D3D11CreateDeviceAndSwapChain pCreate =
        (PFN_D3D11CreateDeviceAndSwapChain)GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain");
    if (!pCreate) return;

    /* Minimal swap chain desc */
    typedef struct {
        unsigned int Width, Height, Format;
        struct { unsigned int Count, Quality; } SampleDesc;
        unsigned int BufferUsage, BufferCount;
        void *OutputWindow; int Windowed;
        unsigned int SwapEffect, Flags;
    } SC_DESC;

    SC_DESC scd;
    memset(&scd, 0, sizeof(scd));
    scd.Width = 1; scd.Height = 1;
    scd.Format = 87; /* DXGI_FORMAT_B8G8R8A8_UNORM */
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = 0x20; /* DXGI_USAGE_RENDER_TARGET_OUTPUT */
    scd.BufferCount = 1;
    scd.OutputWindow = GetDesktopWindow();
    scd.Windowed = 1;
    scd.SwapEffect = 0;

    unsigned int feature_level = 0xb000; /* D3D_FEATURE_LEVEL_11_0 */
    IDXGISwapChain *sc = NULL;
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext *ctx = NULL;

    HRESULT hr = pCreate(NULL, 1/*HARDWARE*/, NULL, 0, &feature_level, 1, 7/*SDK*/,
                         &scd, &sc, &dev, NULL, &ctx);
    if (FAILED(hr) || !sc || !dev || !ctx) return;

    g_device = dev;
    g_context = ctx;

    /* Patch Present in vtable (index 8) */
    void **vtable = *(void***)sc;
    g_orig_present = (PFN_Present)vtable[8];

    DWORD old_prot;
    if (VirtualProtect(&vtable[8], sizeof(void*), PAGE_READWRITE, &old_prot)) {
        vtable[8] = (void*)hook_present;
        VirtualProtect(&vtable[8], sizeof(void*), old_prot, &old_prot);
    }

    sc->lpVtbl->Release(sc);
}

/* ------------------------------------------------------------------ */
/* DLL Entry                                                           */
/* ------------------------------------------------------------------ */

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, void *reserved) {
    (void)hinst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        DWORD pid = GetCurrentProcessId();
        g_shared_mem = op_hook_open_shared(pid, g_map_size);
        try_find_and_hook();
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_staging) { g_staging->lpVtbl->Release(g_staging); g_staging = NULL; }
        if (g_shared_mem) { UnmapViewOfFile(g_shared_mem); g_shared_mem = NULL; }
    }
    return TRUE;
}
