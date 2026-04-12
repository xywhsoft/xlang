#ifndef XL_CGEN_H
#define XL_CGEN_H

#include "../../common/xl_common.h"
#include "../../diag/xl_diag.h"
#include "../../driver/xl_module_link.h"
#include "../../lower/xl_lower.h"

int xlCgenMir(const XL_LowerResult* pLowerResult, const XL_ModuleLinkResult* pModules, XL_DiagContext* pDiag, char** psOutCode);

#endif
