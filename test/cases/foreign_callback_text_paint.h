#ifndef XL_TEST_XUI_CALLBACK_TEXT_PAINT_H
#define XL_TEST_XUI_CALLBACK_TEXT_PAINT_H

#include "xge3_runtime.h"

int xl_test_paint_protocol(void);
int xl_test_paint_create_shape(xui_proxy proxy, const xui_text_shape_t** shape);
int xl_test_paint_decline_shape(xui_proxy proxy);
int xl_test_paint_measure(xui_proxy proxy, const xui_text_shape_t* shape);

#endif
