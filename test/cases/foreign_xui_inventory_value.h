#ifndef FOREIGN_XUI_INVENTORY_VALUE_H
#define FOREIGN_XUI_INVENTORY_VALUE_H

#include "xge3_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif
void inventoryValueHeapBegin(void);
bool inventoryValueHeapBalanced(void);
void inventoryValueHeapReport(int mode);
void inventoryValueBudget(int allowance);
bool inventoryValueFailureTriggered(void);
void inventoryValueNativeLoop(xui_widget grid, int count);
#ifdef __cplusplus
}
#endif

#endif
