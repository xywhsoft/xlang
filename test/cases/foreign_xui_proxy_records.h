#ifndef FOREIGN_XUI_PROXY_RECORDS_H
#define FOREIGN_XUI_PROXY_RECORDS_H
#include "xge3_runtime.h"

typedef int (*proxy_records_proc)(const xui_surface_desc_t*, xui_surface_desc_t*,
    const xui_surface_sampler_t*, xui_surface_sampler_t*, const xui_system_font_desc_t*,
    xui_font_metrics_t*, xui_vec2_t*, xui_rect_t*, uint32_t*, void*);
int xl_test_proxy_records(proxy_records_proc, void*);
#endif
