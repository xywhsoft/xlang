#ifndef XL_CLI_H
#define XL_CLI_H

#include "../common/xl_common.h"
#include "../diag/xl_diag.h"

typedef enum XL_Command {
	XL_CMD_NONE = 0,
	XL_CMD_LEX,
	XL_CMD_PARSE,
	XL_CMD_SEM,
	XL_CMD_LOWER,
	XL_CMD_CGEN,
	XL_CMD_BUILD,
	XL_CMD_RUN,
	XL_CMD_BUILD_C,
	XL_CMD_RUN_C
} XL_Command;

typedef struct XL_CliOptions {
	XL_Command eCommand;
	const char* sInputFile;
	const char* sEmitPath;
	const char* sStopAfter;
	bool bHelp;
	bool bDumpToken;
	bool bDumpAst;
	bool bDumpSem;
	bool bDumpHir;
	bool bDumpMir;
	bool bDumpC;
	bool bDumpJson;
	bool bTraceParser;
	bool bTraceSem;
	bool bTraceLower;
	bool bTraceRuntime;
} XL_CliOptions;

void xlCliInit(XL_CliOptions* pOpts);
int xlCliParse(int argc, char** argv, XL_CliOptions* pOpts, XL_DiagContext* pDiag);
void xlCliPrintHelp(const char* sProgName);
const char* xlCliCommandName(XL_Command eCommand);

#endif
