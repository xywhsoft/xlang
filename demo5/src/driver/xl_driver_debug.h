#ifndef XL_DRIVER_DEBUG_H
#define XL_DRIVER_DEBUG_H

#include "xl_driver.h"

#if XLANG_DEBUGMODE_ENABLED
int xlDriverRunLexDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump);
int xlDriverRunParseDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump);
int xlDriverRunSemDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump);
int xlDriverRunLowerDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump);
int xlDriverRunCgenDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump);
int xlDriverRunInteractiveDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump);
int xlDriverRunDapDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump);
#endif

#endif
