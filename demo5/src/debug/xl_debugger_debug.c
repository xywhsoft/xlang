#include "xl_debugger_debug.h"

#if XLANG_DEBUGMODE_ENABLED
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

typedef struct XL_DebugHostSymbolEntry {
	const char* sName;
	const void* pValue;
} XL_DebugHostSymbolEntry;

static XL_DebugController* __gXLDebugCurrentController = NULL;

static void __xlDebugCopyText(char* sOut, size_t iOutSize, const char* sText)
{
	size_t iLen;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	if ( iLen >= iOutSize ) {
		iLen = iOutSize - 1u;
	}
	if ( iLen != 0u ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
}

static int __xlDebugPathCharEq(char chLeft, char chRight)
{
	if ( (chLeft == '\\') || (chLeft == '/') ) chLeft = '/';
	if ( (chRight == '\\') || (chRight == '/') ) chRight = '/';
	if ( (chLeft >= 'A') && (chLeft <= 'Z') ) chLeft = (char)(chLeft - 'A' + 'a');
	if ( (chRight >= 'A') && (chRight <= 'Z') ) chRight = (char)(chRight - 'A' + 'a');
	return chLeft == chRight;
}

static int __xlDebugSourceMatches(const char* sLeft, const char* sRight)
{
	size_t iLeftLen;
	size_t iRightLen;
	size_t i;

	if ( (sLeft == NULL) || (sRight == NULL) || (sLeft[0] == '\0') || (sRight[0] == '\0') ) {
		return 0;
	}
	iLeftLen = strlen(sLeft);
	iRightLen = strlen(sRight);
	if ( iLeftLen == iRightLen ) {
		for ( i = 0u; i < iLeftLen; ++i ) {
			if ( !__xlDebugPathCharEq(sLeft[i], sRight[i]) ) {
				return 0;
			}
		}
		return 1;
	}
	if ( iLeftLen > iRightLen ) {
		const char* sSuffix = sLeft + (iLeftLen - iRightLen);
		if ( (sSuffix > sLeft) && (sSuffix[-1] != '\\') && (sSuffix[-1] != '/') ) {
			return 0;
		}
		return __xlDebugSourceMatches(sSuffix, sRight);
	}
	return __xlDebugSourceMatches(sRight, sLeft);
}

static int __xlDebugBreakpointMatches(const XL_DebugBreakpoint* pBreakpoint, const char* sSource, int iLine, int iColumn)
{
	XL_UNUSED(iColumn);

	if ( (pBreakpoint == NULL) || (iLine <= 0) ) {
		return 0;
	}
	if ( pBreakpoint->iLine != iLine ) {
		return 0;
	}
	return __xlDebugSourceMatches(pBreakpoint->sSource, sSource);
}

static void __xlDebugControllerMarkStopped(XL_DebugController* pController, const char* sReason)
{
	if ( pController == NULL ) {
		return;
	}
	pController->bStopped = 1;
	__xlDebugCopyText(pController->sStopReason, sizeof(pController->sStopReason), (sReason != NULL) ? sReason : "breakpoint");
	__xlDebugCopyText(pController->sStopSource, sizeof(pController->sStopSource), pController->sCurrentSource);
	pController->iStopLine = pController->iCurrentLine;
	pController->iStopColumn = pController->iCurrentColumn;
	pController->iStopFrameDepth = pController->iFrameDepth;
	pController->iStopProbeCount = pController->iProbeCount;
}

void xlDebugControllerInit(XL_DebugController* pController)
{
	if ( pController == NULL ) {
		return;
	}
	memset(pController, 0, sizeof(*pController));
#if defined(_WIN32) || defined(_WIN64)
	pController->hStoppedEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
	pController->hResumeEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
#endif
}

void xlDebugControllerFree(XL_DebugController* pController)
{
	if ( pController == NULL ) {
		return;
	}
#if defined(_WIN32) || defined(_WIN64)
	if ( pController->hStoppedEvent != NULL ) {
		CloseHandle((HANDLE)pController->hStoppedEvent);
		pController->hStoppedEvent = NULL;
	}
	if ( pController->hResumeEvent != NULL ) {
		CloseHandle((HANDLE)pController->hResumeEvent);
		pController->hResumeEvent = NULL;
	}
#endif
	memset(pController, 0, sizeof(*pController));
}

void xlDebugControllerMakeCurrent(XL_DebugController* pController)
{
	__gXLDebugCurrentController = pController;
}

void xlDebugControllerClearCurrent(XL_DebugController* pController)
{
	if ( (__gXLDebugCurrentController == pController) || (pController == NULL) ) {
		__gXLDebugCurrentController = NULL;
	}
}

int xlDebugControllerSetBreakpoints(
	XL_DebugController* pController,
	const char* sSource,
	const int* arrLines,
	const int* arrColumns,
	size_t iCount
)
{
	size_t i;

	if ( (pController == NULL) || (sSource == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( iCount > XL_DEBUG_BREAKPOINT_MAX ) {
		return XL_RC_FAIL;
	}

	pController->iBreakpointCount = 0u;
	for ( i = 0u; i < iCount; ++i ) {
		XL_DebugBreakpoint* pBreakpoint;

		if ( (arrLines == NULL) || (arrLines[i] <= 0) ) {
			continue;
		}
		if ( pController->iBreakpointCount >= XL_DEBUG_BREAKPOINT_MAX ) {
			return XL_RC_FAIL;
		}

		pBreakpoint = &pController->arrBreakpoints[pController->iBreakpointCount++];
		memset(pBreakpoint, 0, sizeof(*pBreakpoint));
		__xlDebugCopyText(pBreakpoint->sSource, sizeof(pBreakpoint->sSource), sSource);
		pBreakpoint->iLine = arrLines[i];
		pBreakpoint->iColumn = (arrColumns != NULL) ? arrColumns[i] : 0;
		pBreakpoint->bVerified = 0;
		pBreakpoint->iActualLine = arrLines[i];
		__xlDebugCopyText(pBreakpoint->sMessage, sizeof(pBreakpoint->sMessage), "breakpoint is not bound until launch");
	}

	return XL_RC_OK;
}

size_t xlDebugControllerBreakpointCount(const XL_DebugController* pController)
{
	return (pController != NULL) ? pController->iBreakpointCount : 0u;
}

const XL_DebugBreakpoint* xlDebugControllerBreakpointAt(const XL_DebugController* pController, size_t iIndex)
{
	if ( (pController == NULL) || (iIndex >= pController->iBreakpointCount) ) {
		return NULL;
	}
	return &pController->arrBreakpoints[iIndex];
}

size_t xlDebugControllerProbeCount(const XL_DebugController* pController)
{
	return (pController != NULL) ? pController->iProbeCount : 0u;
}

void xlDebugControllerClearStop(XL_DebugController* pController)
{
	if ( pController == NULL ) {
		return;
	}
	pController->bStopped = 0;
	pController->sStopReason[0] = '\0';
	pController->sStopSource[0] = '\0';
	pController->iStopLine = 0;
	pController->iStopColumn = 0;
	pController->iStopFrameDepth = 0;
	pController->iStopProbeCount = 0u;
}

int xlDebugControllerHasStop(const XL_DebugController* pController)
{
	return (pController != NULL) ? pController->bStopped : 0;
}

void xlDebugControllerSetBlocking(XL_DebugController* pController, int bEnabled)
{
	if ( pController == NULL ) {
		return;
	}
	pController->bBlockingEnabled = bEnabled ? 1 : 0;
}

void xlDebugControllerResume(XL_DebugController* pController)
{
	if ( pController == NULL ) {
		return;
	}
#if defined(_WIN32) || defined(_WIN64)
	if ( pController->hStoppedEvent != NULL ) {
		ResetEvent((HANDLE)pController->hStoppedEvent);
	}
	if ( pController->hResumeEvent != NULL ) {
		SetEvent((HANDLE)pController->hResumeEvent);
	}
#endif
}

static void __xlDebugControllerWaitIfStopped(XL_DebugController* pController)
{
	if ( (pController == NULL) || !pController->bBlockingEnabled ) {
		return;
	}
#if defined(_WIN32) || defined(_WIN64)
	if ( (pController->hStoppedEvent == NULL) || (pController->hResumeEvent == NULL) ) {
		return;
	}
	ResetEvent((HANDLE)pController->hResumeEvent);
	SetEvent((HANDLE)pController->hStoppedEvent);
	WaitForSingleObject((HANDLE)pController->hResumeEvent, INFINITE);
	ResetEvent((HANDLE)pController->hStoppedEvent);
	ResetEvent((HANDLE)pController->hResumeEvent);
#endif
}

void xl_dbg_stmt(const char* sSource, int iLine, int iColumn)
{
	XL_DebugController* pController;
	size_t i;

	pController = __gXLDebugCurrentController;
	if ( pController == NULL ) {
		return;
	}

	__xlDebugCopyText(pController->sCurrentSource, sizeof(pController->sCurrentSource), sSource);
	pController->iCurrentLine = iLine;
	pController->iCurrentColumn = iColumn;
	pController->iProbeCount += 1u;
	if ( pController->bStopped ) {
		return;
	}

	for ( i = 0u; i < pController->iBreakpointCount; ++i ) {
		XL_DebugBreakpoint* pBreakpoint = &pController->arrBreakpoints[i];

		if ( __xlDebugBreakpointMatches(pBreakpoint, sSource, iLine, iColumn) ) {
			pBreakpoint->bVerified = 1;
			pBreakpoint->iActualLine = iLine;
			__xlDebugCopyText(pBreakpoint->sMessage, sizeof(pBreakpoint->sMessage), "");
			__xlDebugControllerMarkStopped(pController, "breakpoint");
			__xlDebugControllerWaitIfStopped(pController);
			break;
		}
	}
}

void xl_dbg_enter_func(const char* sSource, int iLine, int iColumn, const char* sFuncName, int iFuncIndex)
{
	XL_UNUSED(sFuncName);
	XL_UNUSED(iFuncIndex);

	if ( __gXLDebugCurrentController != NULL ) {
		__gXLDebugCurrentController->iFrameDepth += 1;
	}
	xl_dbg_stmt(sSource, iLine, iColumn);
}

void xl_dbg_leave_func(void)
{
	if ( (__gXLDebugCurrentController != NULL) && (__gXLDebugCurrentController->iFrameDepth > 0) ) {
		__gXLDebugCurrentController->iFrameDepth -= 1;
	}
}

void xl_dbg_local_int(const char* sName, long long iValue)
{
	XL_UNUSED(sName);
	XL_UNUSED(iValue);
}

void xl_dbg_local_bool(const char* sName, int bValue)
{
	XL_UNUSED(sName);
	XL_UNUSED(bValue);
}

void xl_dbg_local_float(const char* sName, double fValue)
{
	XL_UNUSED(sName);
	XL_UNUSED(fValue);
}

void xl_dbg_local_string(const char* sName, const char* sValue)
{
	XL_UNUSED(sName);
	XL_UNUSED(sValue);
}

void xl_dbg_local_value(const char* sName, void* pValue)
{
	XL_UNUSED(sName);
	XL_UNUSED(pValue);
}

static int __xlDebugAddHostSymbol(TCCState* pTcc, const char* sName, const void* pValue)
{
	char sAlias[160];

	if ( (pTcc == NULL) || (sName == NULL) || (pValue == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( tcc_add_symbol(pTcc, sName, pValue) != 0 ) {
		return XL_RC_FAIL;
	}
#if defined(_WIN32) || defined(_WIN64)
	snprintf(sAlias, sizeof(sAlias), "_%s", sName);
	tcc_add_symbol(pTcc, sAlias, pValue);
	snprintf(sAlias, sizeof(sAlias), "__imp_%s", sName);
	tcc_add_symbol(pTcc, sAlias, pValue);
	snprintf(sAlias, sizeof(sAlias), "__imp__%s", sName);
	tcc_add_symbol(pTcc, sAlias, pValue);
#endif
	return XL_RC_OK;
}

int xlDebugRegisterHostSymbols(TCCState* pTcc)
{
	static const XL_DebugHostSymbolEntry arrSymbols[] = {
		{ "xl_dbg_stmt", xl_dbg_stmt },
		{ "xl_dbg_enter_func", xl_dbg_enter_func },
		{ "xl_dbg_leave_func", xl_dbg_leave_func },
		{ "xl_dbg_local_int", xl_dbg_local_int },
		{ "xl_dbg_local_bool", xl_dbg_local_bool },
		{ "xl_dbg_local_float", xl_dbg_local_float },
		{ "xl_dbg_local_string", xl_dbg_local_string },
		{ "xl_dbg_local_value", xl_dbg_local_value }
	};
	size_t i;

	if ( pTcc == NULL ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0u; i < XL_COUNTOF(arrSymbols); ++i ) {
		if ( __xlDebugAddHostSymbol(pTcc, arrSymbols[i].sName, arrSymbols[i].pValue) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}

	return XL_RC_OK;
}

#endif
