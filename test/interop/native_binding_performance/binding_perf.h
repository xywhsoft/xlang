#ifndef BINDING_PERF_H
#define BINDING_PERF_H
#include <xge3_runtime.h>
/* CPU-only fixture. Code and provider state are process-owned. These are not
 * production wrappers and are not included in the delivered packages. */
int binding_perf_proxy(xui_proxy*);
int binding_perf_font(xui_proxy,xui_font*);
int binding_perf_surface(xui_proxy,xui_surface*);
int binding_perf_draw(xui_proxy,xui_surface,xui_draw_context*);
int binding_perf_shape(xui_proxy,xui_font,const xui_text_shape_t**);
unsigned long long binding_perf_checksum(void);
unsigned long long binding_perf_clock(void);
unsigned long long binding_perf_frequency(void);
int binding_perf_verify(void);
void binding_perf_counter_begin(void);
void binding_perf_counter_end(int);
#endif
