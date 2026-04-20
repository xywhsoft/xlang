#ifndef XL_EXEC_CTX_H
#define XL_EXEC_CTX_H

#include "../common/xl_common.h"
#include "../diag/xl_diag.h"
#include "../host/xl_host.h"

typedef struct XL_ExecCtx {
	char sInputFile[XL_PATH_MAX_LEN];
	char sReleaseRoot[XL_PATH_MAX_LEN];
	char sProjectRoot[XL_PATH_MAX_LEN];
	char sTccRoot[XL_PATH_MAX_LEN];
	XL_HostArtifact arrArtifacts[XL_HOST_ARTIFACT_MAX];
	size_t iArtifactCount;
	XL_HostExternSymbol arrExternSymbols[XL_HOST_EXTERN_SYMBOL_MAX];
	size_t iExternSymbolCount;
	XL_HostLinkLibrary arrLinkLibraries[XL_HOST_LINK_LIBRARY_MAX];
	size_t iLinkLibraryCount;
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
int xlExecCtxAddArtifact(XL_ExecCtx* pCtx, const char* sKind, const char* sFile);
int xlExecCtxAddExternSymbol(XL_ExecCtx* pCtx, const char* sName);
int xlExecCtxAddLinkLibrary(XL_ExecCtx* pCtx, const char* sName);
int xlExecRunCode(XL_ExecCtx* pCtx, XL_DiagContext* pDiag, const char* sCode, char** psTraceText);

#endif
