#ifndef FOREIGN_XUI_MESSAGE_RENDERER_H
#define FOREIGN_XUI_MESSAGE_RENDERER_H

#include "test/cases/foreign_xui_render_backend.h"

#ifdef __cplusplus
extern "C" {
#endif
int messageRendererProxy(xui_proxy* output);
int messageRendererFillCount(void);
int messageRendererInstallNative(xui_widget list);
void messageRendererHeapBegin(void);
void messageRendererHeapEnd(int mode);
#ifdef __cplusplus
}
#endif

#endif
