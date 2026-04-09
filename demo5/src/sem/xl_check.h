#ifndef XL_CHECK_H
#define XL_CHECK_H

#include "../common/xl_common.h"

int xlCheckJumpLevel(int iLevel);
int xlCheckLoopJumpAllowed(int iLoopDepth, int iLevel);
int xlCheckIterableTypeName(const char* sTypeName);

#endif
