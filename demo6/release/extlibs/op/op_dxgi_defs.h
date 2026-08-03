/*
 * op_dxgi_defs.h - Minimal COM/DXGI/D3D11/D3D9 vtable declarations for TCC.
 * TCC has no dxgi.h/d3d11.h/d3d9.h, so we declare the vtables manually.
 * Only the methods we actually call are typed; the rest are void* placeholders.
 */
#ifndef OP_DXGI_DEFS_H
#define OP_DXGI_DEFS_H

#include <windows.h>

/* ------------------------------------------------------------------ */
/* GUID helpers                                                        */
/* ------------------------------------------------------------------ */

typedef struct { unsigned long Data1; unsigned short Data2, Data3; unsigned char Data4[8]; } OP_GUID;
#define OP_DEFINE_GUID(name, l, w1, w2, b1,b2,b3,b4,b5,b6,b7,b8) \
    static const OP_GUID name = { l, w1, w2, { b1,b2,b3,b4,b5,b6,b7,b8 } }

OP_DEFINE_GUID(IID_IUnknown,           0x00000000,0x0000,0x0000, 0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);
OP_DEFINE_GUID(IID_IDXGIDevice,        0x54ec77fa,0x1377,0x44e6, 0x8c,0x32,0x88,0xfd,0x5f,0x44,0xc8,0x4c);
OP_DEFINE_GUID(IID_IDXGIAdapter,       0x2411e7e1,0x12ac,0x4ccf, 0xbd,0x14,0x97,0x98,0xe8,0x53,0x4d,0xc0);
OP_DEFINE_GUID(IID_IDXGIOutput,        0xae02eedb,0xc735,0x4690, 0x8d,0x52,0x5a,0x8d,0xc2,0x02,0x13,0xaa);
OP_DEFINE_GUID(IID_IDXGIOutput1,       0x00cddea8,0x939b,0x4b83, 0xa3,0x40,0xa6,0x85,0x22,0x66,0x66,0xcc);
OP_DEFINE_GUID(IID_IDXGIResource,      0x035f3ab4,0x482e,0x4e50, 0xb4,0x1f,0x8a,0x7f,0x8b,0xd8,0x96,0x0b);
OP_DEFINE_GUID(IID_ID3D11Texture2D,    0x6f15aaf2,0xd208,0x4e89, 0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c);
OP_DEFINE_GUID(IID_ID3D11Device,       0xdb6f6ddb,0xac77,0x4e88, 0x82,0x53,0x81,0x9d,0xf9,0xbb,0xf1,0x40);

/* ------------------------------------------------------------------ */
/* IUnknown                                                            */
/* ------------------------------------------------------------------ */

typedef struct IUnknown IUnknown;
typedef struct IUnknownVtbl {
    HRESULT (WINAPI *QueryInterface)(IUnknown*, const OP_GUID*, void**);
    ULONG   (WINAPI *AddRef)(IUnknown*);
    ULONG   (WINAPI *Release)(IUnknown*);
} IUnknownVtbl;
struct IUnknown { const IUnknownVtbl *lpVtbl; };

#define OP_RELEASE(p) do { if (p) { ((IUnknown*)(p))->lpVtbl->Release((IUnknown*)(p)); (p) = NULL; } } while(0)

/* ------------------------------------------------------------------ */
/* DXGI structures                                                     */
/* ------------------------------------------------------------------ */

typedef struct { int left, top, right, bottom; } OP_RECT;

typedef struct {
    int Width, Height;
    unsigned int Format;
    struct { unsigned int Count, Quality; } SampleDesc;
    unsigned int BufferUsage;
    unsigned int BufferCount;
    void *OutputWindow;
    int Windowed;
    unsigned int SwapEffect;
    unsigned int Flags;
} OP_DXGI_SWAP_CHAIN_DESC;

typedef struct {
    unsigned int Width, Height;
    unsigned int Format;
    unsigned int SampleDesc_Count, SampleDesc_Quality;
    unsigned int BufferUsage;
    unsigned int BufferCount;
    void *OutputWindow;
    int Windowed;
    unsigned int SwapEffect;
    unsigned int Flags;
} OP_DXGI_SWAP_CHAIN_DESC2;

typedef struct {
    unsigned int Width;
    unsigned int Height;
    unsigned int Format;
    unsigned int SampleDesc_Count;
    unsigned int SampleDesc_Quality;
    unsigned int BufferUsage;
    unsigned int BufferCount;
    void *OutputWindow;
    int Windowed;
    unsigned int SwapEffect;
    unsigned int Flags;
} DXGI_SWAP_CHAIN_DESC;

typedef struct {
    unsigned int Width;
    unsigned int Height;
    unsigned int Format;
    struct { unsigned int Count; unsigned int Quality; } SampleDesc;
    unsigned int BufferUsage;
    unsigned int BufferCount;
    void *OutputWindow;
    int Windowed;
    unsigned int SwapEffect;
    unsigned int Flags;
} DXGI_SWAP_CHAIN_DESC_FULL;

typedef struct {
    unsigned int Width;
    unsigned int Height;
} DXGI_OUTDUPL_DESC;

typedef struct {
    long long LastPresentTime;
    long long LastMouseUpdateTime;
    unsigned int AccumulatedFrames;
    int RectsCoalesced;
    int ProtectedContentMaskedOut;
    int PointerShapeBufferSize;
    int MouseMove;
} DXGI_OUTDUPL_FRAME_INFO;

typedef struct {
    unsigned int Width;
    unsigned int Height;
    unsigned int MipLevels;
    unsigned int ArraySize;
    unsigned int Format;
    struct { unsigned int Count; unsigned int Quality; } SampleDesc;
    unsigned int Usage;
    unsigned int BindFlags;
    unsigned int CPUAccessFlags;
    unsigned int MiscFlags;
} D3D11_TEXTURE2D_DESC;

typedef struct {
    unsigned int RowPitch;
    unsigned int DepthPitch;
    void *pData;
} D3D11_MAPPED_SUBRESOURCE;

/* D3D11 constants */
#define D3D11_USAGE_STAGING     3
#define D3D11_CPU_ACCESS_READ   0x10000
#define D3D11_SDK_VERSION       7
#define D3D_DRIVER_TYPE_HARDWARE 1
#define D3D_FEATURE_LEVEL_11_0  0xb000
#define DXGI_FORMAT_B8G8R8A8_UNORM 87

/* ------------------------------------------------------------------ */
/* ID3D11Texture2D / ID3D11Resource                                    */
/* ------------------------------------------------------------------ */

typedef struct ID3D11Texture2D ID3D11Texture2D;
typedef struct ID3D11Texture2DVtbl {
    HRESULT (WINAPI *QueryInterface)(ID3D11Texture2D*, const OP_GUID*, void**);
    ULONG   (WINAPI *AddRef)(ID3D11Texture2D*);
    ULONG   (WINAPI *Release)(ID3D11Texture2D*);
    /* ID3D11DeviceChild */
    void *p1[4];
    /* ID3D11Resource */
    void *p2[3];
    /* ID3D11Texture2D */
    void (WINAPI *GetDesc)(ID3D11Texture2D*, D3D11_TEXTURE2D_DESC*);
} ID3D11Texture2DVtbl;
struct ID3D11Texture2D { const ID3D11Texture2DVtbl *lpVtbl; };

/* ------------------------------------------------------------------ */
/* ID3D11DeviceContext                                                  */
/* ------------------------------------------------------------------ */

typedef struct ID3D11DeviceContext ID3D11DeviceContext;
typedef struct ID3D11DeviceContextVtbl {
    HRESULT (WINAPI *QueryInterface)(ID3D11DeviceContext*, const OP_GUID*, void**);
    ULONG   (WINAPI *AddRef)(ID3D11DeviceContext*);
    ULONG   (WINAPI *Release)(ID3D11DeviceContext*);
    /* ID3D11DeviceChild: GetDevice, GetPrivateData, SetPrivateData, SetPrivateDataInterface */
    void *p1[4];
    /* ID3D11DeviceContext methods (partial - we need CopyResource and Map/Unmap) */
    void *p2[7];  /* VSSetConstantBuffers..Draw (slots 7-13) */
    HRESULT (WINAPI *Map)(ID3D11DeviceContext*, void*, unsigned int, unsigned int, unsigned int, D3D11_MAPPED_SUBRESOURCE*);
    void (WINAPI *Unmap)(ID3D11DeviceContext*, void*, unsigned int);
    void *p3[80]; /* remaining methods up to CopyResource */
    void (WINAPI *CopyResource)(ID3D11DeviceContext*, void*, void*);
} ID3D11DeviceContextVtbl;
struct ID3D11DeviceContext { const ID3D11DeviceContextVtbl *lpVtbl; };

/* ------------------------------------------------------------------ */
/* ID3D11Device                                                         */
/* ------------------------------------------------------------------ */

typedef struct ID3D11Device ID3D11Device;
typedef struct ID3D11DeviceVtbl {
    HRESULT (WINAPI *QueryInterface)(ID3D11Device*, const OP_GUID*, void**);
    ULONG   (WINAPI *AddRef)(ID3D11Device*);
    ULONG   (WINAPI *Release)(ID3D11Device*);
    /* CreateBuffer, CreateTexture1D, CreateTexture2D... */
    void *p1[2]; /* CreateBuffer, CreateTexture1D */
    HRESULT (WINAPI *CreateTexture2D)(ID3D11Device*, const D3D11_TEXTURE2D_DESC*, const void*, ID3D11Texture2D**);
    void *p2[35]; /* remaining up to GetImmediateContext */
    void (WINAPI *GetImmediateContext)(ID3D11Device*, ID3D11DeviceContext**);
} ID3D11DeviceVtbl;
struct ID3D11Device { const ID3D11DeviceVtbl *lpVtbl; };

/* ------------------------------------------------------------------ */
/* IDXGIResource                                                        */
/* ------------------------------------------------------------------ */

typedef struct IDXGIResource IDXGIResource;
typedef struct IDXGIResourceVtbl {
    HRESULT (WINAPI *QueryInterface)(IDXGIResource*, const OP_GUID*, void**);
    ULONG   (WINAPI *AddRef)(IDXGIResource*);
    ULONG   (WINAPI *Release)(IDXGIResource*);
    void *p1[4]; /* IDXGIObject */
    void *p2[3]; /* IDXGIDeviceSubObject */
    HRESULT (WINAPI *GetSharedHandle)(IDXGIResource*, void**);
    void *p3[2];
} IDXGIResourceVtbl;
struct IDXGIResource { const IDXGIResourceVtbl *lpVtbl; };

/* ------------------------------------------------------------------ */
/* IDXGIOutputDuplication                                               */
/* ------------------------------------------------------------------ */

typedef struct IDXGIOutputDuplication IDXGIOutputDuplication;
typedef struct IDXGIOutputDuplicationVtbl {
    HRESULT (WINAPI *QueryInterface)(IDXGIOutputDuplication*, const OP_GUID*, void**);
    ULONG   (WINAPI *AddRef)(IDXGIOutputDuplication*);
    ULONG   (WINAPI *Release)(IDXGIOutputDuplication*);
    /* IDXGIObject: SetPrivateData, SetPrivateDataInterface, GetPrivateData, GetParent */
    void *p1[4];
    /* IDXGIOutputDuplication */
    void (WINAPI *GetDesc)(IDXGIOutputDuplication*, void*);
    HRESULT (WINAPI *AcquireNextFrame)(IDXGIOutputDuplication*, unsigned int, void*, IDXGIResource**);
    void *p2[5]; /* GetFrameDirtyRects..ReleaseFrame */
    HRESULT (WINAPI *ReleaseFrame)(IDXGIOutputDuplication*);
} IDXGIOutputDuplicationVtbl;
struct IDXGIOutputDuplication { const IDXGIOutputDuplicationVtbl *lpVtbl; };

/* ------------------------------------------------------------------ */
/* IDXGIOutput1                                                         */
/* ------------------------------------------------------------------ */

typedef struct IDXGIOutput1 IDXGIOutput1;
typedef struct IDXGIOutput1Vtbl {
    HRESULT (WINAPI *QueryInterface)(IDXGIOutput1*, const OP_GUID*, void**);
    ULONG   (WINAPI *AddRef)(IDXGIOutput1*);
    ULONG   (WINAPI *Release)(IDXGIOutput1*);
    /* IDXGIObject: 4 methods */
    void *p1[4];
    /* IDXGIOutput: GetDesc..GetFrameStatistics = 12 methods */
    void *p2[12];
    /* IDXGIOutput1: GetDisplayModeList1, FindClosestMatchingMode1, GetDisplaySurfaceData1, DuplicateOutput */
    void *p3[3];
    HRESULT (WINAPI *DuplicateOutput)(IDXGIOutput1*, IUnknown*, IDXGIOutputDuplication**);
} IDXGIOutput1Vtbl;
struct IDXGIOutput1 { const IDXGIOutput1Vtbl *lpVtbl; };

/* ------------------------------------------------------------------ */
/* IDXGIAdapter                                                         */
/* ------------------------------------------------------------------ */

typedef struct IDXGIAdapter IDXGIAdapter;
typedef struct IDXGIAdapterVtbl {
    HRESULT (WINAPI *QueryInterface)(IDXGIAdapter*, const OP_GUID*, void**);
    ULONG   (WINAPI *AddRef)(IDXGIAdapter*);
    ULONG   (WINAPI *Release)(IDXGIAdapter*);
    /* IDXGIObject: 4 methods */
    void *p1[4];
    /* IDXGIAdapter: EnumOutputs, GetDesc, CheckInterfaceSupport */
    HRESULT (WINAPI *EnumOutputs)(IDXGIAdapter*, unsigned int, IDXGIOutput1**);
    void *p2[2];
} IDXGIAdapterVtbl;
struct IDXGIAdapter { const IDXGIAdapterVtbl *lpVtbl; };

/* ------------------------------------------------------------------ */
/* IDXGIFactory1                                                        */
/* ------------------------------------------------------------------ */

typedef struct IDXGIFactory1 IDXGIFactory1;
typedef struct IDXGIFactory1Vtbl {
    HRESULT (WINAPI *QueryInterface)(IDXGIFactory1*, const OP_GUID*, void**);
    ULONG   (WINAPI *AddRef)(IDXGIFactory1*);
    ULONG   (WINAPI *Release)(IDXGIFactory1*);
    /* IDXGIObject: 4 methods */
    void *p1[4];
    /* IDXGIFactory: EnumAdapters, MakeWindowAssociation, GetWindowAssociation, CreateSwapChain, CreateSoftwareAdapter = 5 */
    void *p2[5];
    /* IDXGIFactory1: EnumAdapters1 */
    HRESULT (WINAPI *EnumAdapters1)(IDXGIFactory1*, unsigned int, IDXGIAdapter**);
} IDXGIFactory1Vtbl;
struct IDXGIFactory1 { const IDXGIFactory1Vtbl *lpVtbl; };

/* ------------------------------------------------------------------ */
/* IDXGISwapChain (for D3D11 hook)                                     */
/* ------------------------------------------------------------------ */

typedef struct IDXGISwapChain IDXGISwapChain;
typedef struct IDXGISwapChainVtbl {
    HRESULT (WINAPI *QueryInterface)(IDXGISwapChain*, const OP_GUID*, void**);
    ULONG   (WINAPI *AddRef)(IDXGISwapChain*);
    ULONG   (WINAPI *Release)(IDXGISwapChain*);
    /* IDXGIObject: 4 */
    void *p1[4];
    /* IDXGIDeviceSubObject: 3 */
    void *p2[3];
    /* IDXGISwapChain: Present, GetBuffer, SetFullscreenState, GetFullscreenState, GetDesc, ResizeBuffers, ResizeTarget, GetContainingOutput, GetFrameStatistics, GetLastPresentCount */
    HRESULT (WINAPI *Present)(IDXGISwapChain*, unsigned int, unsigned int);
    HRESULT (WINAPI *GetBuffer)(IDXGISwapChain*, unsigned int, const OP_GUID*, void**);
    void *p3[8];
} IDXGISwapChainVtbl;
struct IDXGISwapChain { const IDXGISwapChainVtbl *lpVtbl; };

/* ------------------------------------------------------------------ */
/* D3D9 interfaces (for hook)                                          */
/* ------------------------------------------------------------------ */

typedef struct IDirect3DDevice9 IDirect3DDevice9;
typedef struct IDirect3DDevice9Vtbl {
    /* IUnknown: 3 */
    HRESULT (WINAPI *QueryInterface)(IDirect3DDevice9*, const OP_GUID*, void**);
    ULONG   (WINAPI *AddRef)(IDirect3DDevice9*);
    ULONG   (WINAPI *Release)(IDirect3DDevice9*);
    /* IDirect3DDevice9 methods - Present is at index 17 */
    void *m[14]; /* TestCooperativeLevel..Clear (4-17) */
    HRESULT (WINAPI *Present)(IDirect3DDevice9*, const void*, const void*, void*, const void*);
} IDirect3DDevice9Vtbl;
struct IDirect3DDevice9 { const IDirect3DDevice9Vtbl *lpVtbl; };

typedef struct IDirect3DSurface9 IDirect3DSurface9;
typedef struct IDirect3DSurface9Vtbl {
    HRESULT (WINAPI *QueryInterface)(IDirect3DSurface9*, const OP_GUID*, void**);
    ULONG   (WINAPI *AddRef)(IDirect3DSurface9*);
    ULONG   (WINAPI *Release)(IDirect3DSurface9*);
    /* IDirect3DResource9: GetDevice, SetPrivateData, GetPrivateData, FreePrivateData, SetPriority, GetPriority, PreLoad, GetType = 8 */
    void *r[8];
    /* IDirect3DSurface9: GetContainer, GetDesc, LockRect, UnlockRect */
    void *p1[2];
    void *p2[1]; /* GetDesc */
    HRESULT (WINAPI *LockRect)(IDirect3DSurface9*, void*, const void*, unsigned int);
    HRESULT (WINAPI *UnlockRect)(IDirect3DSurface9*);
} IDirect3DSurface9Vtbl;
struct IDirect3DSurface9 { const IDirect3DSurface9Vtbl *lpVtbl; };

/* D3D9 constants */
#define D3DRTYPE_SURFACE 1
#define D3DPOOL_SYSTEMMEM 0
#define D3DFMT_A8R8G8B8 21
#define D3DLOCK_READONLY 0x10

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

/* ------------------------------------------------------------------ */
/* Function pointer types for dynamic loading                          */
/* ------------------------------------------------------------------ */

typedef HRESULT (WINAPI *PFN_D3D11CreateDevice)(
    void*, int, void*, unsigned int,
    const void*, unsigned int, unsigned int,
    ID3D11Device**, void*, ID3D11DeviceContext**);

typedef HRESULT (WINAPI *PFN_CreateDXGIFactory1)(const OP_GUID*, void**);

#endif /* OP_DXGI_DEFS_H */
