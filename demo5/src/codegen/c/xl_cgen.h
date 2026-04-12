#ifndef XL_CGEN_H
#define XL_CGEN_H

#include "../../common/xl_common.h"
#include "../../diag/xl_diag.h"
#include "../../driver/xl_module_link.h"
#include "../../lower/xl_mir.h"

int xlCgenMir(const XL_MirNode* pRoot, const XL_ModuleLinkResult* pModules, XL_DiagContext* pDiag, char** psOutCode);

#endif
