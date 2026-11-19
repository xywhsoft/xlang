#ifndef FOREIGN_XUI_FIND_RESULTS_H
#define FOREIGN_XUI_FIND_RESULTS_H

#include "xge3_runtime.h"

void findResultsHeapBegin(void);
void findResultsHeapEnd(int expected);
int findResultsTextEqual(xstrview left, xstrview right);
void findResultsNativeHot(xui_widget editor, xui_code_find_scope scope);

#endif
