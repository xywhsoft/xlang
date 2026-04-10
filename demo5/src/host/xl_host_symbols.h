#ifndef XL_HOST_SYMBOLS_H
#define XL_HOST_SYMBOLS_H

#include "../common/xl_common.h"
#include "../../lib/libtcc.h"

int xlHostRegisterSymbols(TCCState* pTcc);
char* xlHostDumpSymbolsJson(void);

#endif
