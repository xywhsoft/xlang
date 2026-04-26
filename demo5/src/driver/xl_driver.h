#ifndef XL_DRIVER_H
#define XL_DRIVER_H

#include "../common/xl_common.h"
#include "../diag/xl_diag.h"
#include "xl_cli.h"

typedef struct XL_DumpContext XL_DumpContext;

int xlDriverRunRelease(const XL_CliOptions* pOpts, XL_DiagContext* pDiag);

#if XLANG_DEBUGMODE_ENABLED
int xlDriverRunDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump);
#endif

#endif
