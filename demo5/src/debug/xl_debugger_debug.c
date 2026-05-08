#include "xl_debugger_debug.h"

#if XLANG_DEBUGMODE_ENABLED
#include "../runtime/xlrt_generated.h"

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

static char* __xlDebugDupText(const char* sText)
{
	size_t iLen;
	char* sOut;

	if ( sText == NULL ) {
		sText = "";
	}
	iLen = strlen(sText);
	sOut = (char*)malloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}
	if ( iLen != 0u ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
	return sOut;
}

static void __xlDebugJsonAppendRaw(char* sOut, size_t iOutSize, size_t* piUsed, const char* sText)
{
	size_t iLen;
	size_t iSpace;

	if ( (sOut == NULL) || (iOutSize == 0u) || (piUsed == NULL) || (sText == NULL) ) {
		return;
	}
	if ( *piUsed >= iOutSize ) {
		return;
	}
	iLen = strlen(sText);
	iSpace = iOutSize - *piUsed - 1u;
	if ( iLen > iSpace ) {
		iLen = iSpace;
	}
	if ( iLen != 0u ) {
		memcpy(sOut + *piUsed, sText, iLen);
		*piUsed += iLen;
	}
	sOut[*piUsed] = '\0';
}

static void __xlDebugJsonAppendInt(char* sOut, size_t iOutSize, size_t* piUsed, int iValue)
{
	char sValue[32];

	snprintf(sValue, sizeof(sValue), "%d", iValue);
	__xlDebugJsonAppendRaw(sOut, iOutSize, piUsed, sValue);
}

static void __xlDebugJsonAppendString(char* sOut, size_t iOutSize, size_t* piUsed, const char* sText)
{
	const unsigned char* sScan;

	__xlDebugJsonAppendRaw(sOut, iOutSize, piUsed, "\"");
	if ( sText == NULL ) {
		sText = "";
	}
	for ( sScan = (const unsigned char*)sText; *sScan != '\0'; ++sScan ) {
		char sEscaped[8];

		if ( *sScan == '"' ) {
			__xlDebugJsonAppendRaw(sOut, iOutSize, piUsed, "\\\"");
		} else if ( *sScan == '\\' ) {
			__xlDebugJsonAppendRaw(sOut, iOutSize, piUsed, "\\\\");
		} else if ( *sScan == '\n' ) {
			__xlDebugJsonAppendRaw(sOut, iOutSize, piUsed, "\\n");
		} else if ( *sScan == '\r' ) {
			__xlDebugJsonAppendRaw(sOut, iOutSize, piUsed, "\\r");
		} else if ( *sScan == '\t' ) {
			__xlDebugJsonAppendRaw(sOut, iOutSize, piUsed, "\\t");
		} else if ( *sScan < 32u ) {
			snprintf(sEscaped, sizeof(sEscaped), "\\u%04x", (unsigned int)*sScan);
			__xlDebugJsonAppendRaw(sOut, iOutSize, piUsed, sEscaped);
		} else {
			char sChar[2];

			sChar[0] = (char)*sScan;
			sChar[1] = '\0';
			__xlDebugJsonAppendRaw(sOut, iOutSize, piUsed, sChar);
		}
	}
	__xlDebugJsonAppendRaw(sOut, iOutSize, piUsed, "\"");
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
	if ( !pBreakpoint->bEnabled ) {
		return 0;
	}
	if ( pBreakpoint->iLine != iLine ) {
		return 0;
	}
	return __xlDebugSourceMatches(pBreakpoint->sSource, sSource);
}

static int __xlDebugControllerHasExecutableLine(const XL_DebugController* pController, const char* sSource, int iLine)
{
	size_t i;

	if ( (pController == NULL) || (sSource == NULL) || (iLine <= 0) || (pController->iExecutableLineCount == 0u) ) {
		return 0;
	}
	if ( !__xlDebugSourceMatches(pController->sExecutableSource, sSource) ) {
		return 0;
	}
	for ( i = 0u; i < pController->iExecutableLineCount; ++i ) {
		if ( pController->arrExecutableLines[i] == iLine ) {
			return 1;
		}
	}
	return 0;
}

static int __xlDebugSameLocation(const char* sLeftSource, int iLeftLine, int iLeftColumn, const char* sRightSource, int iRightLine, int iRightColumn)
{
	if ( (iLeftLine != iRightLine) || (iLeftColumn != iRightColumn) ) {
		return 0;
	}
	return __xlDebugSourceMatches(sLeftSource, sRightSource);
}

static void __xlDebugFrameSet(
	XL_DebugFrame* pFrame,
	const char* sSource,
	int iLine,
	int iColumn,
	const char* sFuncName,
	int iFuncIndex,
	int iDepth
)
{
	if ( pFrame == NULL ) {
		return;
	}
	__xlDebugCopyText(pFrame->sSource, sizeof(pFrame->sSource), sSource);
	__xlDebugCopyText(pFrame->sFunc, sizeof(pFrame->sFunc), (sFuncName != NULL) ? sFuncName : "");
	pFrame->iLine = iLine;
	pFrame->iColumn = iColumn;
	pFrame->iFuncIndex = iFuncIndex;
	pFrame->iDepth = iDepth;
	pFrame->iArgumentStart = 0u;
	pFrame->iArgumentCount = 0u;
	pFrame->iLocalStart = 0u;
	pFrame->iLocalCount = 0u;
}

static void __xlDebugControllerMirrorTopVariables(XL_DebugController* pController)
{
	const XL_DebugFrame* pFrame;
	size_t iCount;

	if ( pController == NULL ) {
		return;
	}
	pController->iArgumentCount = 0u;
	pController->iLocalCount = 0u;
	if ( pController->iFrameCount == 0u ) {
		return;
	}

	pFrame = &pController->arrFrames[pController->iFrameCount - 1u];
	iCount = pFrame->iArgumentCount;
	if ( (pFrame->iArgumentStart + iCount) > pController->iFrameArgumentCount ) {
		iCount = (pFrame->iArgumentStart < pController->iFrameArgumentCount) ? (pController->iFrameArgumentCount - pFrame->iArgumentStart) : 0u;
	}
	if ( iCount > XL_DEBUG_VARIABLE_MAX ) {
		iCount = XL_DEBUG_VARIABLE_MAX;
	}
	if ( iCount != 0u ) {
		memcpy(pController->arrArguments, &pController->arrFrameArguments[pFrame->iArgumentStart], iCount * sizeof(pController->arrArguments[0]));
	}
	pController->iArgumentCount = iCount;

	iCount = pFrame->iLocalCount;
	if ( (pFrame->iLocalStart + iCount) > pController->iFrameLocalCount ) {
		iCount = (pFrame->iLocalStart < pController->iFrameLocalCount) ? (pController->iFrameLocalCount - pFrame->iLocalStart) : 0u;
	}
	if ( iCount > XL_DEBUG_VARIABLE_MAX ) {
		iCount = XL_DEBUG_VARIABLE_MAX;
	}
	if ( iCount != 0u ) {
		memcpy(pController->arrLocals, &pController->arrFrameLocals[pFrame->iLocalStart], iCount * sizeof(pController->arrLocals[0]));
	}
	pController->iLocalCount = iCount;
}

static void __xlDebugControllerSetCurrentFromTopFrame(XL_DebugController* pController)
{
	const XL_DebugFrame* pFrame;

	if ( pController == NULL ) {
		return;
	}
	if ( pController->iFrameCount == 0u ) {
		pController->sCurrentSource[0] = '\0';
		pController->sCurrentFunc[0] = '\0';
		pController->iCurrentLine = 0;
		pController->iCurrentColumn = 0;
		pController->iFrameDepth = 0;
		return;
	}

	pFrame = &pController->arrFrames[pController->iFrameCount - 1u];
	__xlDebugCopyText(pController->sCurrentSource, sizeof(pController->sCurrentSource), pFrame->sSource);
	__xlDebugCopyText(pController->sCurrentFunc, sizeof(pController->sCurrentFunc), pFrame->sFunc);
	pController->iCurrentLine = pFrame->iLine;
	pController->iCurrentColumn = pFrame->iColumn;
	pController->iFrameDepth = (int)pController->iFrameCount;
	__xlDebugControllerMirrorTopVariables(pController);
}

static void __xlDebugControllerClearRuntimeState(XL_DebugController* pController)
{
	if ( pController == NULL ) {
		return;
	}
	pController->sCurrentSource[0] = '\0';
	pController->sCurrentFunc[0] = '\0';
	pController->iCurrentLine = 0;
	pController->iCurrentColumn = 0;
	pController->iFrameDepth = 0;
	pController->iFrameCount = 0u;
	pController->iStopFrameCount = 0u;
	pController->iFrameArgumentCount = 0u;
	pController->iFrameLocalCount = 0u;
	pController->iStopFrameArgumentCount = 0u;
	pController->iStopFrameLocalCount = 0u;
	pController->iProbeCount = 0u;
	pController->iArgumentCount = 0u;
	pController->iLocalCount = 0u;
	pController->iStopArgumentCount = 0u;
	pController->iStopLocalCount = 0u;
	pController->iStopSequence = 0u;
	pController->bPauseRequested = 0;
	pController->eState = XL_DEBUG_STATE_STARTING;
	xlDebugControllerClearStop(pController);
}

static void __xlDebugControllerMarkStopped(XL_DebugController* pController, const char* sReason)
{
	if ( pController == NULL ) {
		return;
	}
	pController->bStopped = 1;
	pController->bPauseRequested = 0;
	pController->eState = XL_DEBUG_STATE_STOPPED;
	pController->iStopSequence += 1u;
	__xlDebugCopyText(pController->sStopReason, sizeof(pController->sStopReason), (sReason != NULL) ? sReason : "breakpoint");
	__xlDebugCopyText(pController->sStopSource, sizeof(pController->sStopSource), pController->sCurrentSource);
	__xlDebugCopyText(pController->sStopFunc, sizeof(pController->sStopFunc), pController->sCurrentFunc);
	pController->iStopLine = pController->iCurrentLine;
	pController->iStopColumn = pController->iCurrentColumn;
	pController->iStopFrameDepth = pController->iFrameDepth;
	pController->iStopProbeCount = pController->iProbeCount;
	pController->eStepMode = XL_DEBUG_STEP_NONE;
	pController->sStepSource[0] = '\0';
	pController->iStepLine = 0;
	pController->iStepColumn = 0;
	pController->iStepFrameDepth = 0;
	pController->iStepProbeCount = 0u;
	pController->iStopFrameCount = pController->iFrameCount;
	if ( pController->iStopFrameCount > XL_DEBUG_FRAME_MAX ) {
		pController->iStopFrameCount = XL_DEBUG_FRAME_MAX;
	}
	if ( pController->iStopFrameCount != 0u ) {
		memcpy(pController->arrStopFrames, pController->arrFrames, pController->iStopFrameCount * sizeof(pController->arrStopFrames[0]));
	}
	pController->iStopFrameArgumentCount = pController->iFrameArgumentCount;
	if ( pController->iStopFrameArgumentCount > XL_DEBUG_FRAME_VARIABLE_MAX ) {
		pController->iStopFrameArgumentCount = XL_DEBUG_FRAME_VARIABLE_MAX;
	}
	if ( pController->iStopFrameArgumentCount != 0u ) {
		memcpy(pController->arrStopFrameArguments, pController->arrFrameArguments, pController->iStopFrameArgumentCount * sizeof(pController->arrStopFrameArguments[0]));
	}
	pController->iStopFrameLocalCount = pController->iFrameLocalCount;
	if ( pController->iStopFrameLocalCount > XL_DEBUG_FRAME_VARIABLE_MAX ) {
		pController->iStopFrameLocalCount = XL_DEBUG_FRAME_VARIABLE_MAX;
	}
	if ( pController->iStopFrameLocalCount != 0u ) {
		memcpy(pController->arrStopFrameLocals, pController->arrFrameLocals, pController->iStopFrameLocalCount * sizeof(pController->arrStopFrameLocals[0]));
	}
	pController->iStopArgumentCount = pController->iArgumentCount;
	if ( pController->iStopArgumentCount > XL_DEBUG_VARIABLE_MAX ) {
		pController->iStopArgumentCount = XL_DEBUG_VARIABLE_MAX;
	}
	pController->iStopLocalCount = pController->iLocalCount;
	if ( pController->iStopLocalCount > XL_DEBUG_VARIABLE_MAX ) {
		pController->iStopLocalCount = XL_DEBUG_VARIABLE_MAX;
	}
	if ( pController->iStopArgumentCount != 0u ) {
		memcpy(pController->arrStopArguments, pController->arrArguments, pController->iStopArgumentCount * sizeof(pController->arrStopArguments[0]));
	}
	if ( pController->iStopLocalCount != 0u ) {
		memcpy(pController->arrStopLocals, pController->arrLocals, pController->iStopLocalCount * sizeof(pController->arrStopLocals[0]));
	}
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
	xlDebugControllerClearExecutableLines(pController);
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
	__xlDebugControllerClearRuntimeState(pController);
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
	const int* arrUnsupported,
	size_t iCount
)
{
	size_t i;
	size_t iKeepCount;
	size_t iValidCount;

	if ( (pController == NULL) || (sSource == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( iCount > XL_DEBUG_BREAKPOINT_MAX ) {
		return XL_RC_FAIL;
	}

	iKeepCount = 0u;
	for ( i = 0u; i < pController->iBreakpointCount; ++i ) {
		if ( !__xlDebugSourceMatches(pController->arrBreakpoints[i].sSource, sSource) ) {
			++iKeepCount;
		}
	}
	iValidCount = 0u;
	for ( i = 0u; i < iCount; ++i ) {
		if ( (arrLines != NULL) && (arrLines[i] > 0) ) {
			++iValidCount;
		}
	}
	if ( (iKeepCount + iValidCount) > XL_DEBUG_BREAKPOINT_MAX ) {
		return XL_RC_FAIL;
	}

	iKeepCount = 0u;
	for ( i = 0u; i < pController->iBreakpointCount; ++i ) {
		if ( !__xlDebugSourceMatches(pController->arrBreakpoints[i].sSource, sSource) ) {
			if ( iKeepCount != i ) {
				pController->arrBreakpoints[iKeepCount] = pController->arrBreakpoints[i];
			}
			++iKeepCount;
		}
	}
	pController->iBreakpointCount = iKeepCount;

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
		pBreakpoint->bEnabled = ((arrUnsupported != NULL) && arrUnsupported[i]) ? 0 : 1;
		pBreakpoint->bVerified = 0;
		pBreakpoint->iActualLine = arrLines[i];
		__xlDebugCopyText(
			pBreakpoint->sMessage,
			sizeof(pBreakpoint->sMessage),
			pBreakpoint->bEnabled ? "breakpoint is not bound until launch" : "conditional breakpoints are not supported"
		);
	}

	return XL_RC_OK;
}

int xlDebugSourceMatches(const char* sLeft, const char* sRight)
{
	return __xlDebugSourceMatches(sLeft, sRight);
}

void xlDebugControllerClearExecutableLines(XL_DebugController* pController)
{
	size_t i;

	if ( pController == NULL ) {
		return;
	}
	for ( i = 0u; i < pController->iExecutableLineCount; ++i ) {
		free(pController->arrExecutableKinds[i]);
		free(pController->arrExecutableFunctions[i]);
		pController->arrExecutableKinds[i] = NULL;
		pController->arrExecutableFunctions[i] = NULL;
	}
	pController->sExecutableSource[0] = '\0';
	pController->iExecutableLineCount = 0u;
}

int xlDebugControllerAddExecutableLine(
	XL_DebugController* pController,
	const char* sSource,
	int iLine,
	int iColumn,
	const char* sKind,
	const char* sFunction
)
{
	size_t i;

	if ( (pController == NULL) || (sSource == NULL) || (sSource[0] == '\0') || (iLine <= 0) ) {
		return XL_RC_BAD_ARG;
	}
	if ( pController->sExecutableSource[0] == '\0' ) {
		__xlDebugCopyText(pController->sExecutableSource, sizeof(pController->sExecutableSource), sSource);
	} else if ( !__xlDebugSourceMatches(pController->sExecutableSource, sSource) ) {
		return XL_RC_OK;
	}
	for ( i = 0u; i < pController->iExecutableLineCount; ++i ) {
		if ( pController->arrExecutableLines[i] == iLine ) {
			if ( (pController->arrExecutableColumns[i] <= 0) || ((iColumn > 0) && (iColumn < pController->arrExecutableColumns[i])) ) {
				pController->arrExecutableColumns[i] = iColumn;
			}
			if ( (sKind != NULL) && (sKind[0] != '\0') && (pController->arrExecutableKinds[i] == NULL) ) {
				pController->arrExecutableKinds[i] = __xlDebugDupText(sKind);
				if ( pController->arrExecutableKinds[i] == NULL ) {
					return XL_RC_IO_ERROR;
				}
			}
			if ( (sFunction != NULL) && (sFunction[0] != '\0') && (pController->arrExecutableFunctions[i] == NULL) ) {
				pController->arrExecutableFunctions[i] = __xlDebugDupText(sFunction);
				if ( pController->arrExecutableFunctions[i] == NULL ) {
					return XL_RC_IO_ERROR;
				}
			}
			return XL_RC_OK;
		}
	}
	if ( pController->iExecutableLineCount >= XL_DEBUG_EXECUTABLE_LINE_MAX ) {
		return XL_RC_FAIL;
	}
	pController->arrExecutableLines[pController->iExecutableLineCount] = iLine;
	pController->arrExecutableColumns[pController->iExecutableLineCount] = iColumn;
	pController->arrExecutableKinds[pController->iExecutableLineCount] = __xlDebugDupText(sKind);
	pController->arrExecutableFunctions[pController->iExecutableLineCount] = __xlDebugDupText(sFunction);
	if ( (pController->arrExecutableKinds[pController->iExecutableLineCount] == NULL) ||
		(pController->arrExecutableFunctions[pController->iExecutableLineCount] == NULL) ) {
		free(pController->arrExecutableKinds[pController->iExecutableLineCount]);
		free(pController->arrExecutableFunctions[pController->iExecutableLineCount]);
		pController->arrExecutableKinds[pController->iExecutableLineCount] = NULL;
		pController->arrExecutableFunctions[pController->iExecutableLineCount] = NULL;
		return XL_RC_IO_ERROR;
	}
	pController->iExecutableLineCount += 1u;
	return XL_RC_OK;
}

void xlDebugControllerBindBreakpoints(XL_DebugController* pController)
{
	size_t i;

	if ( (pController == NULL) || (pController->iExecutableLineCount == 0u) ) {
		return;
	}
	for ( i = 0u; i < pController->iBreakpointCount; ++i ) {
		XL_DebugBreakpoint* pBreakpoint;

		pBreakpoint = &pController->arrBreakpoints[i];
		if ( !pBreakpoint->bEnabled ) {
			pBreakpoint->bVerified = 0;
			pBreakpoint->iActualLine = pBreakpoint->iLine;
			__xlDebugCopyText(pBreakpoint->sMessage, sizeof(pBreakpoint->sMessage), "conditional breakpoints are not supported");
			continue;
		}
		pBreakpoint->iActualLine = pBreakpoint->iLine;
		if ( __xlDebugControllerHasExecutableLine(pController, pBreakpoint->sSource, pBreakpoint->iLine) ) {
			pBreakpoint->bVerified = 1;
			pBreakpoint->sMessage[0] = '\0';
		} else {
			pBreakpoint->bVerified = 0;
			__xlDebugCopyText(pBreakpoint->sMessage, sizeof(pBreakpoint->sMessage), "no executable code at requested line");
		}
	}
}

char* xlDebugControllerDumpExecutableLinesJson(const XL_DebugController* pController)
{
	char* sJson;
	size_t iCap;
	size_t iUsed;
	size_t i;

	if ( pController == NULL ) {
		return NULL;
	}
	iCap = 96u + (strlen(pController->sExecutableSource) * 6u) + (pController->iExecutableLineCount * 192u);
	sJson = (char*)malloc(iCap);
	if ( sJson == NULL ) {
		return NULL;
	}
	iUsed = 0u;
	sJson[0] = '\0';

	__xlDebugJsonAppendRaw(sJson, iCap, &iUsed, "{\"source\":");
	__xlDebugJsonAppendString(sJson, iCap, &iUsed, pController->sExecutableSource);
	__xlDebugJsonAppendRaw(sJson, iCap, &iUsed, ",\"lines\":[");
	for ( i = 0u; i < pController->iExecutableLineCount; ++i ) {
		if ( i != 0u ) {
			__xlDebugJsonAppendRaw(sJson, iCap, &iUsed, ",");
		}
		__xlDebugJsonAppendRaw(sJson, iCap, &iUsed, "{\"line\":");
		__xlDebugJsonAppendInt(sJson, iCap, &iUsed, pController->arrExecutableLines[i]);
		__xlDebugJsonAppendRaw(sJson, iCap, &iUsed, ",\"column\":");
		__xlDebugJsonAppendInt(sJson, iCap, &iUsed, pController->arrExecutableColumns[i]);
		__xlDebugJsonAppendRaw(sJson, iCap, &iUsed, ",\"kind\":");
		__xlDebugJsonAppendString(sJson, iCap, &iUsed, pController->arrExecutableKinds[i]);
		__xlDebugJsonAppendRaw(sJson, iCap, &iUsed, ",\"function\":");
		__xlDebugJsonAppendString(sJson, iCap, &iUsed, pController->arrExecutableFunctions[i]);
		__xlDebugJsonAppendRaw(sJson, iCap, &iUsed, "}");
	}
	__xlDebugJsonAppendRaw(sJson, iCap, &iUsed, "]}");
	return sJson;
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

void xlDebugControllerClearBreakpoints(XL_DebugController* pController)
{
	if ( pController == NULL ) {
		return;
	}
	pController->iBreakpointCount = 0u;
}

void xlDebugControllerClearStop(XL_DebugController* pController)
{
	if ( pController == NULL ) {
		return;
	}
	pController->bStopped = 0;
	pController->sStopReason[0] = '\0';
	pController->sStopSource[0] = '\0';
	pController->sStopFunc[0] = '\0';
	pController->iStopLine = 0;
	pController->iStopColumn = 0;
	pController->iStopFrameDepth = 0;
	pController->iStopProbeCount = 0u;
	pController->iStopFrameCount = 0u;
	pController->iStopFrameArgumentCount = 0u;
	pController->iStopFrameLocalCount = 0u;
	pController->iStopArgumentCount = 0u;
	pController->iStopLocalCount = 0u;
}

int xlDebugControllerHasStop(const XL_DebugController* pController)
{
	return (pController != NULL) ? pController->bStopped : 0;
}

XL_DebugState xlDebugControllerState(const XL_DebugController* pController)
{
	return (pController != NULL) ? pController->eState : XL_DEBUG_STATE_TERMINATED;
}

void xlDebugControllerSetState(XL_DebugController* pController, XL_DebugState eState)
{
	if ( pController == NULL ) {
		return;
	}
	pController->eState = eState;
}

void xlDebugControllerStopAtException(
	XL_DebugController* pController,
	const char* sSource,
	int iLine,
	int iColumn
)
{
	if ( pController == NULL ) {
		return;
	}
	__xlDebugCopyText(pController->sCurrentSource, sizeof(pController->sCurrentSource), (sSource != NULL) ? sSource : "");
	pController->iCurrentLine = iLine;
	pController->iCurrentColumn = iColumn;
	__xlDebugControllerMarkStopped(pController, "exception");
}

void xlDebugControllerSetBlocking(XL_DebugController* pController, int bEnabled)
{
	if ( pController == NULL ) {
		return;
	}
	pController->bBlockingEnabled = bEnabled ? 1 : 0;
}

void xlDebugControllerSetStopOnEntry(XL_DebugController* pController, int bEnabled)
{
	if ( pController == NULL ) {
		return;
	}
	pController->bStopOnEntry = bEnabled ? 1 : 0;
}

void xlDebugControllerRequestPause(XL_DebugController* pController)
{
	if ( pController == NULL ) {
		return;
	}
	pController->bPauseRequested = 1;
}

void xlDebugControllerSetStepMode(XL_DebugController* pController, XL_DebugStepMode eMode)
{
	if ( pController == NULL ) {
		return;
	}
	pController->eStepMode = eMode;
	if ( eMode == XL_DEBUG_STEP_NONE ) {
		pController->sStepSource[0] = '\0';
		pController->iStepLine = 0;
		pController->iStepColumn = 0;
		pController->iStepFrameDepth = 0;
		pController->iStepProbeCount = 0u;
		return;
	}
	__xlDebugCopyText(
		pController->sStepSource,
		sizeof(pController->sStepSource),
		pController->bStopped ? pController->sStopSource : pController->sCurrentSource
	);
	pController->iStepLine = pController->bStopped ? pController->iStopLine : pController->iCurrentLine;
	pController->iStepColumn = pController->bStopped ? pController->iStopColumn : pController->iCurrentColumn;
	pController->iStepFrameDepth = pController->bStopped ? pController->iStopFrameDepth : pController->iFrameDepth;
	pController->iStepProbeCount = pController->bStopped ? pController->iStopProbeCount : pController->iProbeCount;
}

void xlDebugControllerResume(XL_DebugController* pController)
{
	if ( pController == NULL ) {
		return;
	}
	pController->eState = XL_DEBUG_STATE_RUNNING;
#if defined(_WIN32) || defined(_WIN64)
	if ( pController->hStoppedEvent != NULL ) {
		ResetEvent((HANDLE)pController->hStoppedEvent);
	}
	if ( pController->hResumeEvent != NULL ) {
		SetEvent((HANDLE)pController->hResumeEvent);
	}
#endif
}

static int __xlDebugControllerShouldStepStop(XL_DebugController* pController)
{
	if ( (pController == NULL) || (pController->eStepMode == XL_DEBUG_STEP_NONE) ) {
		return 0;
	}
	if ( pController->iProbeCount <= pController->iStepProbeCount ) {
		return 0;
	}
	switch ( pController->eStepMode ) {
	case XL_DEBUG_STEP_IN:
		return 1;
	case XL_DEBUG_STEP_NEXT:
		if ( pController->iFrameDepth > pController->iStepFrameDepth ) {
			return 0;
		}
		return !__xlDebugSameLocation(
			pController->sStepSource,
			pController->iStepLine,
			pController->iStepColumn,
			pController->sCurrentSource,
			pController->iCurrentLine,
			pController->iCurrentColumn
		);
	case XL_DEBUG_STEP_OUT:
		return pController->iFrameDepth < pController->iStepFrameDepth;
	default:
		return 0;
	}
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
	if ( pController->iFrameCount != 0u ) {
		XL_DebugFrame* pFrame = &pController->arrFrames[pController->iFrameCount - 1u];

		__xlDebugCopyText(pFrame->sSource, sizeof(pFrame->sSource), sSource);
		pFrame->iLine = iLine;
		pFrame->iColumn = iColumn;
	}
	pController->iProbeCount += 1u;
	if ( pController->bStopped ) {
		return;
	}

	if ( pController->bStopOnEntry ) {
		pController->bStopOnEntry = 0;
		__xlDebugControllerMarkStopped(pController, "entry");
		__xlDebugControllerWaitIfStopped(pController);
		return;
	}

	if ( pController->bPauseRequested ) {
		pController->bPauseRequested = 0;
		__xlDebugControllerMarkStopped(pController, "pause");
		__xlDebugControllerWaitIfStopped(pController);
		return;
	}

	if ( (pController->iBreakpointCount == 0u) && (pController->eStepMode == XL_DEBUG_STEP_NONE) ) {
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
			return;
		}
	}
	if ( __xlDebugControllerShouldStepStop(pController) ) {
		__xlDebugControllerMarkStopped(pController, "step");
		__xlDebugControllerWaitIfStopped(pController);
	}
}

void xl_dbg_enter_func(const char* sSource, int iLine, int iColumn, const char* sFuncName, int iFuncIndex)
{
	if ( __gXLDebugCurrentController != NULL ) {
		XL_DebugController* pController = __gXLDebugCurrentController;

		__gXLDebugCurrentController->iArgumentCount = 0u;
		__gXLDebugCurrentController->iLocalCount = 0u;
		if ( pController->iFrameCount < XL_DEBUG_FRAME_MAX ) {
			__xlDebugFrameSet(
				&pController->arrFrames[pController->iFrameCount],
				sSource,
				iLine,
				iColumn,
				sFuncName,
				iFuncIndex,
				(int)pController->iFrameCount + 1
			);
			pController->arrFrames[pController->iFrameCount].iArgumentStart = pController->iFrameArgumentCount;
			pController->arrFrames[pController->iFrameCount].iLocalStart = pController->iFrameLocalCount;
			pController->iFrameCount += 1u;
		}
		__xlDebugControllerSetCurrentFromTopFrame(pController);
	}
	xl_dbg_stmt(sSource, iLine, iColumn);
}

void xl_dbg_leave_func(void)
{
	if ( __gXLDebugCurrentController != NULL ) {
		XL_DebugController* pController = __gXLDebugCurrentController;

		if ( pController->iFrameCount != 0u ) {
			const XL_DebugFrame* pFrame = &pController->arrFrames[pController->iFrameCount - 1u];

			pController->iFrameArgumentCount = pFrame->iArgumentStart;
			pController->iFrameLocalCount = pFrame->iLocalStart;
			pController->iFrameCount -= 1u;
		}
		__xlDebugControllerSetCurrentFromTopFrame(pController);
	}
}

static void __xlDebugSetVariable(
	XL_DebugVariable* arrVariables,
	size_t* piVariableCount,
	const char* sName,
	const char* sType,
	const char* sValue,
	void* pValue
)
{
	XL_DebugVariable* pVariable;
	size_t i;

	if ( (arrVariables == NULL) || (piVariableCount == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return;
	}

	for ( i = 0u; i < *piVariableCount; ++i ) {
		if ( strcmp(arrVariables[i].sName, sName) == 0 ) {
			pVariable = &arrVariables[i];
			__xlDebugCopyText(pVariable->sType, sizeof(pVariable->sType), sType);
			__xlDebugCopyText(pVariable->sValue, sizeof(pVariable->sValue), sValue);
			pVariable->pValue = pValue;
			return;
		}
	}
	if ( *piVariableCount >= XL_DEBUG_VARIABLE_MAX ) {
		return;
	}

	pVariable = &arrVariables[(*piVariableCount)++];
	memset(pVariable, 0, sizeof(*pVariable));
	__xlDebugCopyText(pVariable->sName, sizeof(pVariable->sName), sName);
	__xlDebugCopyText(pVariable->sType, sizeof(pVariable->sType), sType);
	__xlDebugCopyText(pVariable->sValue, sizeof(pVariable->sValue), sValue);
	pVariable->pValue = pValue;
}

static void __xlDebugSetFrameVariable(
	XL_DebugVariable* arrVariables,
	size_t* piVariableCount,
	size_t iVariableMax,
	size_t iStart,
	size_t* piFrameVariableCount,
	const char* sName,
	const char* sType,
	const char* sValue,
	void* pValue
)
{
	XL_DebugVariable* pVariable;
	size_t i;

	if ( (arrVariables == NULL) ||
		(piVariableCount == NULL) ||
		(piFrameVariableCount == NULL) ||
		(sName == NULL) ||
		(sName[0] == '\0') ||
		(iStart > *piVariableCount) ) {
		return;
	}

	for ( i = 0u; i < *piFrameVariableCount; ++i ) {
		size_t iVariableIndex = iStart + i;

		if ( iVariableIndex >= *piVariableCount ) {
			break;
		}
		if ( strcmp(arrVariables[iVariableIndex].sName, sName) == 0 ) {
			pVariable = &arrVariables[iVariableIndex];
			__xlDebugCopyText(pVariable->sType, sizeof(pVariable->sType), sType);
			__xlDebugCopyText(pVariable->sValue, sizeof(pVariable->sValue), sValue);
			pVariable->pValue = pValue;
			return;
		}
	}
	if ( *piVariableCount >= iVariableMax ) {
		return;
	}

	pVariable = &arrVariables[*piVariableCount];
	memset(pVariable, 0, sizeof(*pVariable));
	__xlDebugCopyText(pVariable->sName, sizeof(pVariable->sName), sName);
	__xlDebugCopyText(pVariable->sType, sizeof(pVariable->sType), sType);
	__xlDebugCopyText(pVariable->sValue, sizeof(pVariable->sValue), sValue);
	pVariable->pValue = pValue;
	*piVariableCount += 1u;
	*piFrameVariableCount += 1u;
}

static void __xlDebugSetArgument(const char* sName, const char* sType, const char* sValue, void* pValue)
{
	XL_DebugController* pController;
	XL_DebugFrame* pFrame;

	pController = __gXLDebugCurrentController;
	if ( pController == NULL ) {
		return;
	}
	if ( pController->iFrameCount != 0u ) {
		pFrame = &pController->arrFrames[pController->iFrameCount - 1u];
		__xlDebugSetFrameVariable(
			pController->arrFrameArguments,
			&pController->iFrameArgumentCount,
			XL_DEBUG_FRAME_VARIABLE_MAX,
			pFrame->iArgumentStart,
			&pFrame->iArgumentCount,
			sName,
			sType,
			sValue,
			pValue
		);
	}
	__xlDebugSetVariable(pController->arrArguments, &pController->iArgumentCount, sName, sType, sValue, pValue);
}

static void __xlDebugSetLocal(const char* sName, const char* sType, const char* sValue, void* pValue)
{
	XL_DebugController* pController;
	XL_DebugFrame* pFrame;

	pController = __gXLDebugCurrentController;
	if ( pController == NULL ) {
		return;
	}
	if ( pController->iFrameCount != 0u ) {
		pFrame = &pController->arrFrames[pController->iFrameCount - 1u];
		__xlDebugSetFrameVariable(
			pController->arrFrameLocals,
			&pController->iFrameLocalCount,
			XL_DEBUG_FRAME_VARIABLE_MAX,
			pFrame->iLocalStart,
			&pFrame->iLocalCount,
			sName,
			sType,
			sValue,
			pValue
		);
	}
	__xlDebugSetVariable(pController->arrLocals, &pController->iLocalCount, sName, sType, sValue, pValue);
}

void xl_dbg_arg_int(const char* sName, long long iValue)
{
	char sValue[64];

	snprintf(sValue, sizeof(sValue), "%lld", iValue);
	__xlDebugSetArgument(sName, "int", sValue, NULL);
}

void xl_dbg_arg_bool(const char* sName, int bValue)
{
	__xlDebugSetArgument(sName, "bool", bValue ? "true" : "false", NULL);
}

void xl_dbg_arg_float(const char* sName, double fValue)
{
	char sValue[64];

	snprintf(sValue, sizeof(sValue), "%.17g", fValue);
	__xlDebugSetArgument(sName, "float", sValue, NULL);
}

void xl_dbg_arg_string(const char* sName, const char* sValue)
{
	__xlDebugSetArgument(sName, "string", (sValue != NULL) ? sValue : "", NULL);
}

static void __xlDebugValueText(char* sOut, size_t iOutSize, void* pValue)
{
	xvalue value;
	const char* sText;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	if ( pValue == NULL ) {
		__xlDebugCopyText(sOut, iOutSize, "");
		return;
	}

	value = (xvalue)pValue;
	switch ( value->Type ) {
		case XVO_DT_NULL:
		case XVO_DT_BOOL:
		case XVO_DT_INT:
		case XVO_DT_FLOAT:
		case XVO_DT_TEXT:
		case XVO_DT_TIME:
		case XVO_DT_POINT:
			sText = xl_value_to_string(value);
			__xlDebugCopyText(sOut, iOutSize, (sText != NULL) ? sText : "");
			break;
		default:
			snprintf(sOut, iOutSize, "[var:%p]", pValue);
			break;
	}
}

void xl_dbg_arg_value(const char* sName, void* pValue)
{
	char sValue[256];

	__xlDebugValueText(sValue, sizeof(sValue), pValue);
	__xlDebugSetArgument(sName, "var", sValue, pValue);
}

void xl_dbg_local_int(const char* sName, long long iValue)
{
	char sValue[64];

	snprintf(sValue, sizeof(sValue), "%lld", iValue);
	__xlDebugSetLocal(sName, "int", sValue, NULL);
}

void xl_dbg_local_bool(const char* sName, int bValue)
{
	__xlDebugSetLocal(sName, "bool", bValue ? "true" : "false", NULL);
}

void xl_dbg_local_float(const char* sName, double fValue)
{
	char sValue[64];

	snprintf(sValue, sizeof(sValue), "%.17g", fValue);
	__xlDebugSetLocal(sName, "float", sValue, NULL);
}

void xl_dbg_local_string(const char* sName, const char* sValue)
{
	__xlDebugSetLocal(sName, "string", (sValue != NULL) ? sValue : "", NULL);
}

void xl_dbg_local_value(const char* sName, void* pValue)
{
	char sValue[256];

	__xlDebugValueText(sValue, sizeof(sValue), pValue);
	__xlDebugSetLocal(sName, "var", sValue, pValue);
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
		{ "xl_dbg_arg_int", xl_dbg_arg_int },
		{ "xl_dbg_arg_bool", xl_dbg_arg_bool },
		{ "xl_dbg_arg_float", xl_dbg_arg_float },
		{ "xl_dbg_arg_string", xl_dbg_arg_string },
		{ "xl_dbg_arg_value", xl_dbg_arg_value },
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
