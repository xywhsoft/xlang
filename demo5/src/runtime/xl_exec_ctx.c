#include "xl_exec_ctx.h"

static void __xlCopyText(char* sOut, size_t iOutSize, const char* sText)
{
	size_t iLen;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	if ( iLen >= iOutSize ) {
		iLen = iOutSize - 1;
	}
	if ( iLen != 0 ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
}

void xlExecCtxInit(XL_ExecCtx* pCtx, const char* sInputFile, const char* sReleaseRoot)
{
	XL_HostContext hostCtx;

	if ( pCtx == NULL ) {
		return;
	}

	memset(pCtx, 0, sizeof(*pCtx));
	__xlCopyText(pCtx->sEntrySymbol, sizeof(pCtx->sEntrySymbol), "xl_generated_entry");
	xlHostContextInit(&hostCtx, sInputFile, sReleaseRoot);
	__xlCopyText(pCtx->sInputFile, sizeof(pCtx->sInputFile), hostCtx.sInputFile);
	__xlCopyText(pCtx->sReleaseRoot, sizeof(pCtx->sReleaseRoot), hostCtx.sReleaseRoot);
	__xlCopyText(pCtx->sProjectRoot, sizeof(pCtx->sProjectRoot), hostCtx.sProjectRoot);
	__xlCopyText(pCtx->sTccRoot, sizeof(pCtx->sTccRoot), hostCtx.sTccRoot);
}

int xlExecRunCode(XL_ExecCtx* pCtx, XL_DiagContext* pDiag, const char* sCode, char** psTraceText)
{
	XL_HostContext hostCtx;
	XL_HostRunResult result;
	int iRc;

	if ( (pCtx == NULL) || (sCode == NULL) || (psTraceText == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	memset(&hostCtx, 0, sizeof(hostCtx));
	__xlCopyText(hostCtx.sInputFile, sizeof(hostCtx.sInputFile), pCtx->sInputFile);
	__xlCopyText(hostCtx.sReleaseRoot, sizeof(hostCtx.sReleaseRoot), pCtx->sReleaseRoot);
	__xlCopyText(hostCtx.sProjectRoot, sizeof(hostCtx.sProjectRoot), pCtx->sProjectRoot);
	__xlCopyText(hostCtx.sTccRoot, sizeof(hostCtx.sTccRoot), pCtx->sTccRoot);

	iRc = xlHostRunCText(
		&hostCtx,
		pDiag,
		pCtx->sInputFile,
		sCode,
		pCtx->sEntrySymbol,
		psTraceText,
		&result
	);

	pCtx->iExitCode = result.iExitCode;
	pCtx->bHadRuntimeError = result.bHadRuntimeError;
	pCtx->bHadException = result.bHadException;
	pCtx->iExceptionKind = result.iExceptionKind;
	__xlCopyText(pCtx->sExceptionText, sizeof(pCtx->sExceptionText), result.sExceptionText);
	pCtx->iExceptionInt = result.iExceptionInt;
	pCtx->fExceptionFloat = result.fExceptionFloat;
	__xlCopyText(pCtx->sEntrySymbol, sizeof(pCtx->sEntrySymbol), result.sEntrySymbol);
	return iRc;
}
