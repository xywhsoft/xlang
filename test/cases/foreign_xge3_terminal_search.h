#ifndef XL6_TEST_TERMINAL_SEARCH_H
#define XL6_TEST_TERMINAL_SEARCH_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
void terminalSearchHeapBegin(void);
void terminalSearchHeapEnd(int mode);
void terminalSearchNativeHot(xui_widget terminal);
void terminalSearchNativeOom(xui_widget terminal);
#ifdef __cplusplus
}
#endif
#endif
