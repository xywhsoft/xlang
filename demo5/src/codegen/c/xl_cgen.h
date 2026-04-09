#ifndef XL_CGEN_H
#define XL_CGEN_H

#include "../../common/xl_common.h"
#include "../../diag/xl_diag.h"
#include "../../lower/xl_mir.h"

int xlCgenMir(const XL_MirNode* pRoot, XL_DiagContext* pDiag, char** psOutCode);

#endif
