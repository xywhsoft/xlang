#ifndef XL6_TEST_XUI_TABLE_COLUMNS_H
#define XL6_TEST_XUI_TABLE_COLUMNS_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
void tableColumnsBegin(void);
void tableColumnsEnd(int mode);
void tableColumnsBudget(int allowance);
void tableColumnsNativeHot(xui_widget table);
#ifdef __cplusplus
}
#endif
#endif
