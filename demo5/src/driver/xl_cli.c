#include "xl_cli.h"
#if XLANG_DEBUGMODE_ENABLED
#include "xl_cli_debug.h"
#endif

typedef struct XL_CommandEntry {
	const char* sName;
	XL_Command eCommand;
} XL_CommandEntry;

static const XL_CommandEntry __gXLCommands[] = {
#if XLANG_DEBUGMODE_ENABLED
	{"lex", XL_CMD_LEX},
	{"parse", XL_CMD_PARSE},
	{"sem", XL_CMD_SEM},
	{"lower", XL_CMD_LOWER},
	{"cgen", XL_CMD_CGEN},
	{"dbg", XL_CMD_DBG},
	{"dap", XL_CMD_DAP},
#endif
	{"build", XL_CMD_BUILD},
	{"run", XL_CMD_RUN},
	{"build-c", XL_CMD_BUILD_C},
	{"run-c", XL_CMD_RUN_C}
};

static XL_Command __xlCliParseCommand(const char* sName)
{
	size_t i;

	if ( sName == NULL ) {
		return XL_CMD_NONE;
	}

	for ( i = 0; i < XL_COUNTOF(__gXLCommands); ++i ) {
		if ( strcmp(__gXLCommands[i].sName, sName) == 0 ) {
			return __gXLCommands[i].eCommand;
		}
	}

	return XL_CMD_NONE;
}

static bool __xlCliNeedInput(XL_Command eCommand)
{
#if XLANG_DEBUGMODE_ENABLED
	if ( eCommand == XL_CMD_DAP ) {
		return false;
	}
#endif
	return eCommand != XL_CMD_NONE;
}

void xlCliInit(XL_CliOptions* pOpts)
{
	if ( pOpts == NULL ) {
		return;
	}

	memset(pOpts, 0, sizeof(*pOpts));
	pOpts->eCommand = XL_CMD_NONE;
}

const char* xlCliCommandName(XL_Command eCommand)
{
	size_t i;

	for ( i = 0; i < XL_COUNTOF(__gXLCommands); ++i ) {
		if ( __gXLCommands[i].eCommand == eCommand ) {
			return __gXLCommands[i].sName;
		}
	}

	return "unknown";
}

int xlCliParse(int argc, char** argv, XL_CliOptions* pOpts, XL_DiagContext* pDiag)
{
	int i;

	if ( (argv == NULL) || (pOpts == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 1; i < argc; ++i ) {
		const char* sArg = argv[i];

		if ( sArg == NULL ) {
			continue;
		}
		if ( (strcmp(sArg, "-h") == 0) || (strcmp(sArg, "--help") == 0) ) {
			pOpts->bHelp = true;
			continue;
		}
#if XLANG_DEBUGMODE_ENABLED
		if ( strcmp(sArg, "--dump-json") == 0 ) {
			XL_CLI_DUMP_JSON(pOpts) = true;
			continue;
		}
#endif
#if XLANG_DEBUGMODE_ENABLED
		if ( strcmp(sArg, "--diag-json") == 0 ) {
			XL_CLI_DIAG_JSON(pOpts) = true;
			xlDiagSetFormat(pDiag, XL_DIAG_FORMAT_JSON);
			continue;
		}
#endif
#if XLANG_DEBUGMODE_ENABLED
		{
			XL_CliDebugParseResult eDebugResult;

			eDebugResult = xlCliTryParseDebugOption(&i, argc, argv, pOpts, pDiag);
			if ( eDebugResult == XL_CLI_DEBUG_PARSE_HANDLED ) {
				continue;
			}
			if ( eDebugResult == XL_CLI_DEBUG_PARSE_ERROR ) {
				return XL_RC_BAD_ARG;
			}
		}
#endif
		if ( strcmp(sArg, "--emit") == 0 ) {
			if ( (i + 1) >= argc ) {
				XL_DIAG_ERROR(pDiag, "cli", "--emit requires a path");
				return XL_RC_BAD_ARG;
			}
			pOpts->sEmitPath = argv[++i];
			continue;
		}
		if ( strcmp(sArg, "--stop-after") == 0 ) {
			if ( (i + 1) >= argc ) {
				XL_DIAG_ERROR(pDiag, "cli", "--stop-after requires a phase name");
				return XL_RC_BAD_ARG;
			}
			pOpts->sStopAfter = argv[++i];
			continue;
		}
		if ( sArg[0] == '-' ) {
			XL_DIAG_ERROR(pDiag, "cli", "unknown option: %s", sArg);
			return XL_RC_BAD_ARG;
		}
		if ( pOpts->eCommand == XL_CMD_NONE ) {
			pOpts->eCommand = __xlCliParseCommand(sArg);
			if ( pOpts->eCommand == XL_CMD_NONE ) {
				XL_DIAG_ERROR(pDiag, "cli", "unknown command: %s", sArg);
				return XL_RC_BAD_ARG;
			}
			continue;
		}
		if ( pOpts->sInputFile == NULL ) {
			pOpts->sInputFile = sArg;
			continue;
		}

		XL_DIAG_ERROR(pDiag, "cli", "unexpected argument: %s", sArg);
		return XL_RC_BAD_ARG;
	}

	if ( pOpts->bHelp ) {
		return XL_RC_OK;
	}

	if ( pOpts->eCommand == XL_CMD_NONE ) {
		return XL_RC_OK;
	}

	if ( __xlCliNeedInput(pOpts->eCommand) && (pOpts->sInputFile == NULL) ) {
		XL_DIAG_ERROR(
			pDiag,
			"cli",
			"command '%s' requires an input file",
			xlCliCommandName(pOpts->eCommand)
		);
		return XL_RC_BAD_ARG;
	}

	return XL_RC_OK;
}

void xlCliPrintHelp(const char* sProgName)
{
	if ( sProgName == NULL ) {
		sProgName = XLANG_DEBUGMODE_ENABLED ? "xldbg" : "xl";
	}

	printf("xlang compiler driver\n\n");
	printf("Usage:\n");
	printf("  %s <command> <file> [options]\n\n", sProgName);
	printf("Commands:\n");
#if XLANG_DEBUGMODE_ENABLED
	xlCliPrintDebugCommandHelp();
#endif
	printf("  build    build source into an output artifact\n");
	printf("  run      run program through compiler pipeline\n");
	printf("  build-c  build a C source file through embedded TCC\n");
	printf("  run-c    run a C source file through embedded TCC\n\n");
	printf("Options:\n");
#if XLANG_DEBUGMODE_ENABLED
	xlCliPrintDebugOptionHelp();
	printf("  --dump-json       emit machine-readable dump when available\n");
#endif
#if XLANG_DEBUGMODE_ENABLED
	printf("  --diag-json       emit diagnostics as JSON lines\n");
#endif
	printf("  --emit <path>     write output to a path\n");
	printf("  --stop-after <phase>\n");
	printf("  -h, --help        print help\n");
}
