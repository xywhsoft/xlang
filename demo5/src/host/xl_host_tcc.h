#ifndef XL_HOST_TCC_H
#define XL_HOST_TCC_H

#include "xl_host.h"

int xlHostTccRunText(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sVirtualPath,
	const char* sCode,
	const char* sEntrySymbol,
	char** psTraceText,
	XL_HostRunResult* pResult
);

int xlHostTccRunFile(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sSourceFile,
	char** psTraceText,
	XL_HostRunResult* pResult
);
int xlHostTccBuildText(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sVirtualPath,
	const char* sCode,
	const char* sEntrySymbol,
	const char* sOutputFile,
	char** psTraceText,
	XL_HostBuildResult* pResult
);
int xlHostTccBuildFile(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sSourceFile,
	const char* sOutputFile,
	char** psTraceText,
	XL_HostBuildResult* pResult
);

#endif
