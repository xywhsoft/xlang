#ifndef XL6_TEST_XUI_NATIVE_SEQUENCE_H
#define XL6_TEST_XUI_NATIVE_SEQUENCE_H
#include <xge3_runtime.h>
int sequence_cpu_proxy(xui_proxy*);
int sequence_cpu_font(xui_proxy,xui_font*);
int sequence_cpu_shape_drops(void);
int sequence_cpu_font_drops(void);
void sequence_cpu_no_alloc(void);
int sequence_cpu_reset_alloc(void);
int sequence_cpu_verify(void);
#endif
