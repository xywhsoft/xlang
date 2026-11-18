#ifndef XL6_TEST_XGE_INPUT_EVENTS_H
#define XL6_TEST_XGE_INPUT_EVENTS_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
void inputEventHeapBegin(void);
void inputEventHeapEnd(int mode);
void inputEventNativeHot(void);
#ifdef __cplusplus
}
#endif
#endif
