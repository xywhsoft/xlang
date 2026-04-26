#ifndef XL_CLI_DEBUG_H
#define XL_CLI_DEBUG_H

#include "xl_cli.h"

#if XLANG_DEBUGMODE_ENABLED
typedef enum XL_CliDebugParseResult {
	XL_CLI_DEBUG_PARSE_NOT_HANDLED = 0,
	XL_CLI_DEBUG_PARSE_HANDLED = 1,
	XL_CLI_DEBUG_PARSE_ERROR = -1
} XL_CliDebugParseResult;

XL_CliDebugParseResult xlCliTryParseDebugOption(
	int* pArgIndex,
	int argc,
	char** argv,
	XL_CliOptions* pOpts,
	XL_DiagContext* pDiag
);

void xlCliPrintDebugCommandHelp(void);
void xlCliPrintDebugOptionHelp(void);
#endif

#endif
