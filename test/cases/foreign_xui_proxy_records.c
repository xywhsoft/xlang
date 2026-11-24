#include "foreign_xui_proxy_records.h"
#include <assert.h>

int xl_test_proxy_records(proxy_records_proc callback, void* user)
{
    const xui_surface_desc_t surface = {.iWidth = 24, .iHeight = 16};
    const xui_surface_sampler_t sampler = {.iMinFilter = 2, .iWrapS = 3};
    const xui_system_font_desc_t font = {.iSize = sizeof(font), .fSize = 14.0f};
    xui_surface_desc_t surface_out = {0};
    xui_surface_sampler_t sampler_out = {0};
    xui_font_metrics_t metrics = {0};
    xui_vec2_t vector = {0};
    xui_rect_t rect = {0};
    uint32_t generation = 0;
    int status = callback(&surface, &surface_out, &sampler, &sampler_out, &font, &metrics,
        &vector, &rect, &generation, user);
    if (status) return status;
    assert(surface_out.iWidth == 24 && surface_out.iHeight == 16);
    assert(sampler_out.iMinFilter == 2 && sampler_out.iWrapS == 3);
    assert(metrics.fSize == font.fSize && metrics.fLineHeight == 18.0f);
    assert(vector.fX == 24.0f && vector.fY == 16.0f);
    assert(rect.fW == 24 && rect.fH == 16);
    assert(generation == UINT32_C(4294967295));
    return 0;
}
