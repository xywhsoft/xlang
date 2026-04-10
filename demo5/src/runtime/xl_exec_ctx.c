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
	size_t i;

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
	pCtx->iArtifactCount = 0;
	pCtx->iExternSymbolCount = 0;
	for ( i = 0; i < XL_HOST_ARTIFACT_MAX; ++i ) {
		pCtx->arrArtifacts[i].sKind[0] = '\0';
		pCtx->arrArtifacts[i].sFile[0] = '\0';
	}
	for ( i = 0; i < XL_HOST_EXTERN_SYMBOL_MAX; ++i ) {
		pCtx->arrExternSymbols[i].sName[0] = '\0';
	}
}

int xlExecCtxAddArtifact(XL_ExecCtx* pCtx, const char* sKind, const char* sFile)
{
	size_t i;

	if ( (pCtx == NULL) || (sKind == NULL) || (sFile == NULL) || (sKind[0] == '\0') || (sFile[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pCtx->iArtifactCount; ++i ) {
		if ( (strcmp(pCtx->arrArtifacts[i].sKind, sKind) == 0) &&
			(strcmp(pCtx->arrArtifacts[i].sFile, sFile) == 0) ) {
			return XL_RC_OK;
		}
	}

	if ( pCtx->iArtifactCount >= XL_HOST_ARTIFACT_MAX ) {
		return XL_RC_FAIL;
	}

	__xlCopyText(pCtx->arrArtifacts[pCtx->iArtifactCount].sKind, sizeof(pCtx->arrArtifacts[pCtx->iArtifactCount].sKind), sKind);
	__xlCopyText(pCtx->arrArtifacts[pCtx->iArtifactCount].sFile, sizeof(pCtx->arrArtifacts[pCtx->iArtifactCount].sFile), sFile);
	pCtx->iArtifactCount += 1;
	return XL_RC_OK;
}

int xlExecCtxAddExternSymbol(XL_ExecCtx* pCtx, const char* sName)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pCtx->iExternSymbolCount; ++i ) {
		if ( strcmp(pCtx->arrExternSymbols[i].sName, sName) == 0 ) {
			return XL_RC_OK;
		}
	}

	if ( pCtx->iExternSymbolCount >= XL_HOST_EXTERN_SYMBOL_MAX ) {
		return XL_RC_FAIL;
	}

	__xlCopyText(pCtx->arrExternSymbols[pCtx->iExternSymbolCount].sName, sizeof(pCtx->arrExternSymbols[pCtx->iExternSymbolCount].sName), sName);
	pCtx->iExternSymbolCount += 1;
	return XL_RC_OK;
}

int xlExecRunCode(XL_ExecCtx* pCtx, XL_DiagContext* pDiag, const char* sCode, char** psTraceText)
{
	XL_HostContext hostCtx;
	XL_HostRunResult result;
	int iRc;
	size_t i;

	if ( (pCtx == NULL) || (sCode == NULL) || (psTraceText == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	memset(&hostCtx, 0, sizeof(hostCtx));
	__xlCopyText(hostCtx.sInputFile, sizeof(hostCtx.sInputFile), pCtx->sInputFile);
	__xlCopyText(hostCtx.sReleaseRoot, sizeof(hostCtx.sReleaseRoot), pCtx->sReleaseRoot);
	__xlCopyText(hostCtx.sProjectRoot, sizeof(hostCtx.sProjectRoot), pCtx->sProjectRoot);
	__xlCopyText(hostCtx.sTccRoot, sizeof(hostCtx.sTccRoot), pCtx->sTccRoot);
	for ( i = 0; i < pCtx->iArtifactCount; ++i ) {
		iRc = xlHostContextAddArtifact(&hostCtx, pCtx->arrArtifacts[i].sKind, pCtx->arrArtifacts[i].sFile);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}
	for ( i = 0; i < pCtx->iExternSymbolCount; ++i ) {
		iRc = xlHostContextAddExternSymbol(&hostCtx, pCtx->arrExternSymbols[i].sName);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

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
