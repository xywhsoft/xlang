#ifndef FOREIGN_XUI_COMBOBOX_ITEMS_H
#define FOREIGN_XUI_COMBOBOX_ITEMS_H

#include "xge3_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif
void comboItemsHeapBegin(void);
void comboItemsHeapEnd(int mode);
void comboItemsSetBudget(int allowance);
bool comboItemsFailureTriggered(void);
void comboItemsNativeRead(xui_widget combo);
void comboItemsNativeWrite(xui_widget combo);
#ifdef __cplusplus
}
#endif

#endif
