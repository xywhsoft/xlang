#ifndef FOREIGN_XUI_TIMELINE_MESSAGE_STATE_H
#define FOREIGN_XUI_TIMELINE_MESSAGE_STATE_H

#include "xge3_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif
void stateHeapBegin(void);
void stateHeapEnd(int mode);
void stateBudget(int allowance);
bool stateAllocationFailed(void);
void stateNativeTimelineRead(xui_widget view, int layer, int span);
void stateNativeMessageRead(xui_widget view);
#ifdef __cplusplus
}
#endif

#endif
