#ifndef FOREIGN_XGE_RESOURCE_OOM_H
#define FOREIGN_XGE_RESOURCE_OOM_H
#include "xrt_decl.h"
void resource_test_begin(void);
int resource_test_balanced(void);
int resource_test_fail(int count);
void resource_test_clear(void);
#endif
