#include <d3d9.h>
#include <stddef.h>

/* 独立查询系统 SDK；hook 源文件也断言同一组槽位。 */
typedef char sdk_present_slot[(offsetof(IDirect3DDevice9Vtbl, Present) / sizeof(void*) == 17u) ? 1 : -1];
typedef char sdk_backbuffer_slot[(offsetof(IDirect3DDevice9Vtbl, GetBackBuffer) / sizeof(void*) == 18u) ? 1 : -1];
typedef char sdk_readback_slot[(offsetof(IDirect3DDevice9Vtbl, GetRenderTargetData) / sizeof(void*) == 32u) ? 1 : -1];
typedef char sdk_offscreen_slot[(offsetof(IDirect3DDevice9Vtbl, CreateOffscreenPlainSurface) / sizeof(void*) == 36u) ? 1 : -1];
typedef char sdk_surface_desc_slot[(offsetof(IDirect3DSurface9Vtbl, GetDesc) / sizeof(void*) == 12u) ? 1 : -1];
typedef char sdk_surface_lock_slot[(offsetof(IDirect3DSurface9Vtbl, LockRect) / sizeof(void*) == 13u) ? 1 : -1];
typedef char sdk_present_params_size[(sizeof(D3DPRESENT_PARAMETERS) == 64u) ? 1 : -1];
