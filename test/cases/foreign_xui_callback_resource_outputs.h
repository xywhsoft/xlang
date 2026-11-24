#ifndef XL_TEST_XUI_CALLBACK_RESOURCE_OUTPUTS_H
#define XL_TEST_XUI_CALLBACK_RESOURCE_OUTPUTS_H

#include "xge3_runtime.h"

int xl_test_callback_output_state(xvalue**);
int xl_test_callback_output_read_begin(void);
int xl_test_callback_output_read_end(void);
int xl_test_callback_output_native_failure(xui_proxy);
int xl_test_callback_output_proxy_edges(xui_proxy);

#endif
