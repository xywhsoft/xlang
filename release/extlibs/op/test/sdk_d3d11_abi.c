#include <d3d11.h>
#include <dxgi.h>
#include <stddef.h>

/* 与 hook 的手写声明独立核对系统 SDK 的真实 ABI。 */
typedef char sdk_swap_desc_size[(sizeof(DXGI_SWAP_CHAIN_DESC) == 72u) ? 1 : -1];
typedef char sdk_swap_device_slot[(offsetof(IDXGISwapChainVtbl, GetDevice) / sizeof(void*) == 7u) ? 1 : -1];
typedef char sdk_swap_present_slot[(offsetof(IDXGISwapChainVtbl, Present) / sizeof(void*) == 8u) ? 1 : -1];
typedef char sdk_swap_buffer_slot[(offsetof(IDXGISwapChainVtbl, GetBuffer) / sizeof(void*) == 9u) ? 1 : -1];
typedef char sdk_device_context_slot[(offsetof(ID3D11DeviceVtbl, GetImmediateContext) / sizeof(void*) == 40u) ? 1 : -1];
typedef char sdk_map_slot[(offsetof(ID3D11DeviceContextVtbl, Map) / sizeof(void*) == 14u) ? 1 : -1];
typedef char sdk_copy_slot[(offsetof(ID3D11DeviceContextVtbl, CopyResource) / sizeof(void*) == 47u) ? 1 : -1];
typedef char sdk_mapped_data_offset[(offsetof(D3D11_MAPPED_SUBRESOURCE, pData) == 0u) ? 1 : -1];
typedef char sdk_mapped_size[(sizeof(D3D11_MAPPED_SUBRESOURCE) == 16u) ? 1 : -1];
typedef char sdk_read_access_bit[(D3D11_CPU_ACCESS_READ == 0x20000u) ? 1 : -1];
