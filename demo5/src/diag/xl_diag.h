#ifndef XL_DIAG_H
#define XL_DIAG_H

#include "../common/xl_common.h"

typedef enum XL_DiagLevel {
	XL_DIAG_LEVEL_INFO = 0,
	XL_DIAG_LEVEL_WARN = 1,
	XL_DIAG_LEVEL_ERROR = 2
} XL_DiagLevel;

typedef enum XL_DiagFormat {
	XL_DIAG_FORMAT_TEXT = 0,
	XL_DIAG_FORMAT_JSON = 1
} XL_DiagFormat;

typedef struct XL_DiagContext {
	FILE* pOut;
	FILE* pErr;
	const char* sDefaultSourceFile;
	XL_DiagFormat eFormat;
	int iWarnCount;
	int iErrorCount;
} XL_DiagContext;

void xlDiagInit(XL_DiagContext* pDiag, FILE* pOut, FILE* pErr);
void xlDiagSetFormat(XL_DiagContext* pDiag, XL_DiagFormat eFormat);
void xlDiagSetDefaultSourceFile(XL_DiagContext* pDiag, const char* sSourceFile);
void xlDiagEmit(
	XL_DiagContext* pDiag,
	XL_DiagLevel eLevel,
	const char* sPhase,
	const char* sFile,
	int iLine,
	const char* sFormat,
	...
);
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
);

#define XL_DIAG_INFO(diag, phase, fmt, ...) xlDiagEmit((diag), XL_DIAG_LEVEL_INFO, (phase), __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define XL_DIAG_WARN(diag, phase, fmt, ...) xlDiagEmit((diag), XL_DIAG_LEVEL_WARN, (phase), __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define XL_DIAG_ERROR(diag, phase, fmt, ...) xlDiagEmit((diag), XL_DIAG_LEVEL_ERROR, (phase), __FILE__, __LINE__, (fmt), ##__VA_ARGS__)

#endif
