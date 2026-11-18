#ifndef FOREIGN_XUI_CODE_ASSIST_H
#define FOREIGN_XUI_CODE_ASSIST_H

#include "xge3_runtime.h"

void codeAssistHeapBegin(void);
void codeAssistHeapEnd(int mode);
void codeAssistHeapBudget(int allowance);
void codeAssistNativeHot(xui_widget editor, uint32_t version, int offset);

#endif
