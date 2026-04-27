#ifndef XL_HOST_SYMBOLS_H
#define XL_HOST_SYMBOLS_H

#include "../common/xl_common.h"
#include "../../lib/libtcc.h"

int xlHostRegisterSymbols(TCCState* pTcc);
#if XLANG_DEBUGMODE_ENABLED
char* xlHostDumpSymbolsJson(void);
#endif

#endif
