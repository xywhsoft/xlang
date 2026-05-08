#ifndef XL_SEM_DUMP_H
#define XL_SEM_DUMP_H

#include "xl_resolve.h"

char* xlSemDumpText(const XL_SemResult* pResult);
char* xlSemDumpJson(const XL_SemResult* pResult);
char* xlSemDumpSymbolsJson(const XL_SemResult* pResult);
char* xlSemDumpTypesJson(const XL_SemResult* pResult);
char* xlSemDumpFuncsJson(const XL_SemResult* pResult);
char* xlSemDumpXleFunctionsJson(const XL_SemResult* pResult);
char* xlSemDumpDiagText(const XL_SemResult* pResult);

#endif
