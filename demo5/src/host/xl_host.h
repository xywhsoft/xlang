#ifndef XL_HOST_H
#define XL_HOST_H

#include "../common/xl_common.h"
#include "../common/xl_path.h"
#include "../diag/xl_diag.h"

typedef struct XL_HostContext {
	char sInputFile[XL_PATH_MAX_LEN];
	char sReleaseRoot[XL_PATH_MAX_LEN];
	char sProjectRoot[XL_PATH_MAX_LEN];
	char sTccRoot[XL_PATH_MAX_LEN];
} XL_HostContext;

typedef struct XL_HostRunResult {
	char sEntrySymbol[64];
	int iExitCode;
	int bHadRuntimeError;
	int bHadException;
	int iExceptionKind;
	char sExceptionText[XL_DIAG_TEXT_MAX];
	int64_t iExceptionInt;
	double fExceptionFloat;
} XL_HostRunResult;

typedef struct XL_HostBuildResult {
	char sOutputFile[XL_PATH_MAX_LEN];
	char sOutputKind[16];
	char sEntrySymbol[64];
	int bHadBuildError;
} XL_HostBuildResult;

void xlHostContextInit(XL_HostContext* pCtx, const char* sInputFile, const char* sReleaseRoot);
int xlHostRunCText(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sVirtualPath,
	const char* sCode,
	const char* sEntrySymbol,
	char** psTraceText,
	XL_HostRunResult* pResult
);
int xlHostRunCFile(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sSourceFile,
	char** psTraceText,
	XL_HostRunResult* pResult
);
int xlHostBuildCText(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sVirtualPath,
	const char* sCode,
	const char* sEntrySymbol,
	const char* sOutputFile,
	char** psTraceText,
	XL_HostBuildResult* pResult
);
int xlHostBuildCFile(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sSourceFile,
	const char* sOutputFile,
	char** psTraceText,
	XL_HostBuildResult* pResult
);

#endif
