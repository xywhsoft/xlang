#ifndef XL_CLI_H
#define XL_CLI_H

#include "../common/xl_common.h"
#include "../diag/xl_diag.h"

typedef enum XL_Command {
	XL_CMD_NONE = 0,
#if XLANG_DEBUGMODE_ENABLED
	XL_CMD_LEX,
	XL_CMD_PARSE,
	XL_CMD_SEM,
	XL_CMD_LOWER,
	XL_CMD_CGEN,
#endif
	XL_CMD_BUILD,
	XL_CMD_RUN,
	XL_CMD_BUILD_C,
	XL_CMD_RUN_C
} XL_Command;

#if XLANG_DEBUGMODE_ENABLED
typedef struct XL_CliDebugOptions {
	bool bDumpToken;
	bool bDumpAst;
	bool bDumpSem;
	bool bDumpHir;
	bool bDumpMir;
	bool bDumpC;
	bool bDumpJson;
	bool bDiagJson;
	bool bTraceParser;
	bool bTraceSem;
	bool bTraceLower;
	bool bTraceRuntime;
} XL_CliDebugOptions;
#endif

typedef struct XL_CliOptions {
	XL_Command eCommand;
	const char* sInputFile;
	const char* sEmitPath;
	const char* sStopAfter;
	bool bHelp;
#if XLANG_DEBUGMODE_ENABLED
	XL_CliDebugOptions dbg;
#endif
} XL_CliOptions;

#if XLANG_DEBUGMODE_ENABLED
#define XL_CLI_DUMP_TOKEN(pOpts)   ((pOpts)->dbg.bDumpToken)
#define XL_CLI_DUMP_AST(pOpts)     ((pOpts)->dbg.bDumpAst)
#define XL_CLI_DUMP_SEM(pOpts)     ((pOpts)->dbg.bDumpSem)
#define XL_CLI_DUMP_HIR(pOpts)     ((pOpts)->dbg.bDumpHir)
#define XL_CLI_DUMP_MIR(pOpts)     ((pOpts)->dbg.bDumpMir)
#define XL_CLI_DUMP_C(pOpts)       ((pOpts)->dbg.bDumpC)
#define XL_CLI_DUMP_JSON(pOpts)    ((pOpts)->dbg.bDumpJson)
#define XL_CLI_DIAG_JSON(pOpts)    ((pOpts)->dbg.bDiagJson)
#define XL_CLI_TRACE_PARSER(pOpts) ((pOpts)->dbg.bTraceParser)
#define XL_CLI_TRACE_SEM(pOpts)    ((pOpts)->dbg.bTraceSem)
#define XL_CLI_TRACE_LOWER(pOpts)  ((pOpts)->dbg.bTraceLower)
#define XL_CLI_TRACE_RUNTIME(pOpts)((pOpts)->dbg.bTraceRuntime)
#else
#define XL_CLI_DUMP_TOKEN(pOpts)   (0)
#define XL_CLI_DUMP_AST(pOpts)     (0)
#define XL_CLI_DUMP_SEM(pOpts)     (0)
#define XL_CLI_DUMP_HIR(pOpts)     (0)
#define XL_CLI_DUMP_MIR(pOpts)     (0)
#define XL_CLI_DUMP_C(pOpts)       (0)
#define XL_CLI_DUMP_JSON(pOpts)    (0)
#define XL_CLI_DIAG_JSON(pOpts)    (0)
#define XL_CLI_TRACE_PARSER(pOpts) (0)
#define XL_CLI_TRACE_SEM(pOpts)    (0)
#define XL_CLI_TRACE_LOWER(pOpts)  (0)
#define XL_CLI_TRACE_RUNTIME(pOpts)(0)
#endif

void xlCliInit(XL_CliOptions* pOpts);
int xlCliParse(int argc, char** argv, XL_CliOptions* pOpts, XL_DiagContext* pDiag);
void xlCliPrintHelp(const char* sProgName);
const char* xlCliCommandName(XL_Command eCommand);

#endif
