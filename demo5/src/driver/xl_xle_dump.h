#ifndef XL_XLE_DUMP_H
#define XL_XLE_DUMP_H

#include "xl_module_link.h"
#include "../sem/xl_resolve.h"

char* xlXleDumpPackageJson(const XL_ModuleLinkResult* pModuleResult, const XL_SemResult* pSemResult, const char* sInputFile);

#endif
