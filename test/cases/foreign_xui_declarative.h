#ifndef XL6_TEST_FOREIGN_XUI_DECLARATIVE_H
#define XL6_TEST_FOREIGN_XUI_DECLARATIVE_H
#include <xui.h>
#include "src/runtime/xl6_foreign_view.h"
extern const XL6_ForeignViewDescriptorV1 declarativeXuiContextPolicy;
extern const XL6_ForeignViewDescriptorV1 declarativeXuiWidgetPolicy;
void declarative_xui_start(void);
void declarative_xui_counters_begin(void);
void declarative_xui_counters_end(void);
#endif
