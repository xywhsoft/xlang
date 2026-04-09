#ifndef XL_EXEC_CTX_H
#define XL_EXEC_CTX_H

#include "../common/xl_common.h"
#include "../common/xl_path.h"
#include "../diag/xl_diag.h"
#include "../../lib/libtcc.h"

typedef struct XL_ExecCtx {
	char sInputFile[XL_PATH_MAX_LEN];
	char sReleaseRoot[XL_PATH_MAX_LEN];
	char sProjectRoot[XL_PATH_MAX_LEN];
	char sLibTccDll[XL_PATH_MAX_LEN];
	char sTccRoot[XL_PATH_MAX_LEN];
	char sEntrySymbol[64];
	int iExitCode;
	int bHadRuntimeError;
	int bHadException;
	int iExceptionKind;
	char sExceptionText[XL_DIAG_TEXT_MAX];
	int64_t iExceptionInt;
	double fExceptionFloat;
} XL_ExecCtx;

void xlExecCtxInit(XL_ExecCtx* pCtx, const char* sInputFile, const char* sReleaseRoot);
int xlExecRunCode(XL_ExecCtx* pCtx, XL_DiagContext* pDiag, const char* sCode, char** psTraceText);

#endif
