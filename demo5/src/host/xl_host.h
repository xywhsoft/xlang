#ifndef XL_HOST_H
#define XL_HOST_H

#include "../common/xl_common.h"
#include "../common/xl_path.h"
#include "../diag/xl_diag.h"

#define XL_HOST_ARTIFACT_MAX 32
#define XL_HOST_EXTERN_SYMBOL_MAX 128
#define XL_HOST_LINK_LIBRARY_MAX 64

typedef struct XL_HostArtifact {
	char sKind[32];
	char sFile[XL_PATH_MAX_LEN];
} XL_HostArtifact;

typedef struct XL_HostExternSymbol {
	char sName[128];
} XL_HostExternSymbol;

typedef struct XL_HostLinkLibrary {
	char sName[128];
} XL_HostLinkLibrary;

typedef struct XL_HostContext {
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
int xlHostContextAddArtifact(XL_HostContext* pCtx, const char* sKind, const char* sFile);
int xlHostContextAddExternSymbol(XL_HostContext* pCtx, const char* sName);
int xlHostContextAddLinkLibrary(XL_HostContext* pCtx, const char* sName);
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
