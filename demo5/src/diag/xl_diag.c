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

static void __xlDiagJsonString(FILE* pStream, const char* sText)
{
	const unsigned char* p;

	fputc('"', pStream);
	if ( sText != NULL ) {
		for ( p = (const unsigned char*)sText; *p != '\0'; ++p ) {
			switch ( *p ) {
				case '\\': fputs("\\\\", pStream); break;
				case '"': fputs("\\\"", pStream); break;
				case '\r': fputs("\\r", pStream); break;
				case '\n': fputs("\\n", pStream); break;
				case '\t': fputs("\\t", pStream); break;
				default:
					if ( *p < 0x20 ) {
						fprintf(pStream, "\\u%04x", (unsigned int)*p);
					} else {
						fputc((int)*p, pStream);
					}
					break;
			}
		}
	}
	fputc('"', pStream);
}

static int __xlDiagReadSourceLine(const char* sFile, int iLine, char* sOut, size_t iOutSize)
{
	FILE* pFile;
	char sLine[1024];
	int iCurrentLine;
	size_t iLen;

	if ( (sFile == NULL) || (sFile[0] == '\0') || (iLine <= 0) || (sOut == NULL) || (iOutSize == 0) ) {
		return 0;
	}
	pFile = fopen(sFile, "rb");
	if ( pFile == NULL ) {
		return 0;
	}
	iCurrentLine = 0;
	while ( fgets(sLine, sizeof(sLine), pFile) != NULL ) {
		++iCurrentLine;
		if ( iCurrentLine == iLine ) {
			iLen = strlen(sLine);
			while ( (iLen != 0) && ((sLine[iLen - 1] == '\n') || (sLine[iLen - 1] == '\r')) ) {
				sLine[--iLen] = '\0';
			}
			snprintf(sOut, iOutSize, "%s", sLine);
			fclose(pFile);
			return 1;
		}
	}
	fclose(pFile);
	return 0;
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
	const char* sLocationFile;
	char sMessage[XL_DIAG_TEXT_MAX * 2];
	char sSourceLine[1024];

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

	vsnprintf(sMessage, sizeof(sMessage), sFormat, args);
	sLocationFile = ((sSourceFile != NULL) && (sSourceFile[0] != '\0')) ? sSourceFile : pDiag->sDefaultSourceFile;
	if ( pDiag->eFormat == XL_DIAG_FORMAT_JSON ) {
		fputs("{\"level\":", pStream);
		__xlDiagJsonString(pStream, __xlDiagLevelName(eLevel));
		fputs(",\"phase\":", pStream);
		__xlDiagJsonString(pStream, (sPhase != NULL) ? sPhase : "core");
		fputs(",\"file\":", pStream);
		__xlDiagJsonString(pStream, sLocationFile);
		fprintf(pStream, ",\"line\":%d,\"column\":%d,\"message\":", iSourceLine, iSourceColumn);
		__xlDiagJsonString(pStream, sMessage);
		fputs("}\n", pStream);
		fflush(pStream);
		return;
	}

	fprintf(pStream, "[%s][%s] ", __xlDiagLevelName(eLevel), (sPhase != NULL) ? sPhase : "core");
	if ( (sLocationFile != NULL) && (sLocationFile[0] != '\0') && (iSourceLine > 0) ) {
		fprintf(pStream, "%s:%d:%d: ", sLocationFile, iSourceLine, iSourceColumn);
	} else if ( iSourceLine > 0 ) {
		fprintf(pStream, "%d:%d: ", iSourceLine, iSourceColumn);
	} else {
		fprintf(pStream, "%s:%d: ", (sFile != NULL) ? sFile : "<unknown>", iLine);
	}

	fputs(sMessage, pStream);
	fputc('\n', pStream);
	if ( __xlDiagReadSourceLine(sLocationFile, iSourceLine, sSourceLine, sizeof(sSourceLine)) ) {
		int i;

		fprintf(pStream, "  %s\n  ", sSourceLine);
		for ( i = 1; i < iSourceColumn; ++i ) {
			fputc((sSourceLine[i - 1] == '\t') ? '\t' : ' ', pStream);
		}
		fputs("^\n", pStream);
	}
	fflush(pStream);
}

void xlDiagInit(XL_DiagContext* pDiag, FILE* pOut, FILE* pErr)
{
	if ( pDiag == NULL ) {
		return;
	}

	pDiag->pOut = pOut;
	pDiag->pErr = pErr;
	pDiag->sDefaultSourceFile = NULL;
	pDiag->eFormat = XL_DIAG_FORMAT_TEXT;
	pDiag->iWarnCount = 0;
	pDiag->iErrorCount = 0;
}

void xlDiagSetFormat(XL_DiagContext* pDiag, XL_DiagFormat eFormat)
{
	if ( pDiag != NULL ) {
		pDiag->eFormat = eFormat;
	}
}

void xlDiagSetDefaultSourceFile(XL_DiagContext* pDiag, const char* sSourceFile)
{
	if ( pDiag != NULL ) {
		pDiag->sDefaultSourceFile = sSourceFile;
	}
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
