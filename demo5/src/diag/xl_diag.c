#include "xl_diag.h"

static const char* __xlDiagLevelName(XL_DiagLevel eLevel)
{
	switch ( eLevel ) {
		case XL_DIAG_LEVEL_INFO:
			return "info";
		case XL_DIAG_LEVEL_WARN:
			return "warn";
		case XL_DIAG_LEVEL_ERROR:
			return "error";
		default:
			return "unknown";
	}
}

static void __xlDiagEmitV(
	XL_DiagContext* pDiag,
	XL_DiagLevel eLevel,
	const char* sPhase,
	const char* sFile,
	int iLine,
	const char* sSourceFile,
	int iSourceLine,
	int iSourceColumn,
	const char* sFormat,
	va_list args
)
{
	FILE* pStream;

	if ( (pDiag == NULL) || (sFormat == NULL) ) {
		return;
	}

	pStream = (eLevel == XL_DIAG_LEVEL_ERROR) ? pDiag->pErr : pDiag->pOut;
	if ( pStream == NULL ) {
		pStream = stderr;
	}

	if ( eLevel == XL_DIAG_LEVEL_WARN ) {
		pDiag->iWarnCount += 1;
	} else if ( eLevel == XL_DIAG_LEVEL_ERROR ) {
		pDiag->iErrorCount += 1;
	}

	fprintf(pStream, "[%s][%s] ", __xlDiagLevelName(eLevel), (sPhase != NULL) ? sPhase : "core");
	if ( (sSourceFile != NULL) && (sSourceFile[0] != '\0') ) {
		fprintf(pStream, "%s:%d:%d: ", sSourceFile, iSourceLine, iSourceColumn);
	} else if ( iSourceLine > 0 ) {
		fprintf(pStream, "%d:%d: ", iSourceLine, iSourceColumn);
	} else {
		fprintf(pStream, "%s:%d: ", (sFile != NULL) ? sFile : "<unknown>", iLine);
	}

	vfprintf(pStream, sFormat, args);
	fputc('\n', pStream);
	fflush(pStream);
}

void xlDiagInit(XL_DiagContext* pDiag, FILE* pOut, FILE* pErr)
{
	if ( pDiag == NULL ) {
		return;
	}

	pDiag->pOut = pOut;
	pDiag->pErr = pErr;
	pDiag->iWarnCount = 0;
	pDiag->iErrorCount = 0;
}

void xlDiagEmit(
	XL_DiagContext* pDiag,
	XL_DiagLevel eLevel,
	const char* sPhase,
	const char* sFile,
	int iLine,
	const char* sFormat,
	...
)
{
	va_list args;

	va_start(args, sFormat);
	__xlDiagEmitV(pDiag, eLevel, sPhase, sFile, iLine, NULL, 0, 0, sFormat, args);
	va_end(args);
}

void xlDiagEmitSource(
	XL_DiagContext* pDiag,
	XL_DiagLevel eLevel,
	const char* sPhase,
	const char* sFile,
	int iLine,
	const char* sSourceFile,
	int iSourceLine,
	int iSourceColumn,
	const char* sFormat,
	...
)
{
	va_list args;

	va_start(args, sFormat);
	__xlDiagEmitV(pDiag, eLevel, sPhase, sFile, iLine, sSourceFile, iSourceLine, iSourceColumn, sFormat, args);
	va_end(args);
}
