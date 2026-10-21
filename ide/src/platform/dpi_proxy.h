#ifndef XLANG_IDE_DPI_PROXY_H
#define XLANG_IDE_DPI_PROXY_H
#include "xui.h"
typedef struct xide_dpi_proxy xide_dpi_proxy;
int xideDpiProxyCreate(xide_dpi_proxy** state,xui_proxy_t* proxy,float scale);
int xideDpiProxySetScale(xide_dpi_proxy* state,float scale);
float xideDpiProxyScale(const xide_dpi_proxy* state);
void xideDpiProxyDestroy(xide_dpi_proxy* state);
int xideDpiSurfaceReadPixels(xide_dpi_proxy* state,xui_surface surface,void* pixels,int stride,int* width,int* height);
int xideDpiFontPixelMetrics(xide_dpi_proxy* state,xui_font font,xui_font_metrics_t* metrics);
int xideDpiSurfacePresent(xide_dpi_proxy* state,xui_surface surface,int width,int height);
int xideDpiTextMeasure(xui_proxy proxy,xui_font font,const char* text,xui_vec2_t* size);
int xideDpiCodeEditCreate(xui_context context,xui_widget* output,const xui_code_edit_desc_t* desc);
#endif
