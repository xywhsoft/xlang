#include "xl_cli.h"

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
#endif
	{"build", XL_CMD_BUILD},
	{"run", XL_CMD_RUN},
	{"build-c", XL_CMD_BUILD_C},
	{"run-c", XL_CMD_RUN_C},
	{"bind", XL_CMD_BIND}
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
		if ( strcmp(sArg, "--dump-token") == 0 ) {
#if XLANG_DEBUGMODE_ENABLED
			pOpts->bDumpToken = true;
			continue;
#else
			XL_DIAG_ERROR(pDiag, "cli", "--dump-token is only available in xldbg");
			return XL_RC_BAD_ARG;
#endif
		}
		if ( strcmp(sArg, "--dump-ast") == 0 ) {
#if XLANG_DEBUGMODE_ENABLED
			pOpts->bDumpAst = true;
			continue;
#else
			XL_DIAG_ERROR(pDiag, "cli", "--dump-ast is only available in xldbg");
			return XL_RC_BAD_ARG;
#endif
		}
		if ( strcmp(sArg, "--dump-sem") == 0 ) {
#if XLANG_DEBUGMODE_ENABLED
			pOpts->bDumpSem = true;
			continue;
#else
			XL_DIAG_ERROR(pDiag, "cli", "--dump-sem is only available in xldbg");
			return XL_RC_BAD_ARG;
#endif
		}
		if ( strcmp(sArg, "--dump-hir") == 0 ) {
#if XLANG_DEBUGMODE_ENABLED
			pOpts->bDumpHir = true;
			continue;
#else
			XL_DIAG_ERROR(pDiag, "cli", "--dump-hir is only available in xldbg");
			return XL_RC_BAD_ARG;
#endif
		}
		if ( strcmp(sArg, "--dump-mir") == 0 ) {
#if XLANG_DEBUGMODE_ENABLED
			pOpts->bDumpMir = true;
			continue;
#else
			XL_DIAG_ERROR(pDiag, "cli", "--dump-mir is only available in xldbg");
			return XL_RC_BAD_ARG;
#endif
		}
		if ( strcmp(sArg, "--dump-c") == 0 ) {
#if XLANG_DEBUGMODE_ENABLED
			pOpts->bDumpC = true;
			continue;
#else
			XL_DIAG_ERROR(pDiag, "cli", "--dump-c is only available in xldbg");
			return XL_RC_BAD_ARG;
#endif
		}
		if ( strcmp(sArg, "--dump-json") == 0 ) {
			pOpts->bDumpJson = true;
			continue;
		}
		if ( strcmp(sArg, "--diag-json") == 0 ) {
			pOpts->bDiagJson = true;
			xlDiagSetFormat(pDiag, XL_DIAG_FORMAT_JSON);
			continue;
		}
		if ( strcmp(sArg, "--trace-parser") == 0 ) {
#if XLANG_DEBUGMODE_ENABLED
			pOpts->bTraceParser = true;
			continue;
#else
			XL_DIAG_ERROR(pDiag, "cli", "--trace-parser is only available in xldbg");
			return XL_RC_BAD_ARG;
#endif
		}
		if ( strcmp(sArg, "--trace-sem") == 0 ) {
#if XLANG_DEBUGMODE_ENABLED
			pOpts->bTraceSem = true;
			continue;
#else
			XL_DIAG_ERROR(pDiag, "cli", "--trace-sem is only available in xldbg");
			return XL_RC_BAD_ARG;
#endif
		}
		if ( strcmp(sArg, "--trace-lower") == 0 ) {
#if XLANG_DEBUGMODE_ENABLED
			pOpts->bTraceLower = true;
			continue;
#else
			XL_DIAG_ERROR(pDiag, "cli", "--trace-lower is only available in xldbg");
			return XL_RC_BAD_ARG;
#endif
		}
		if ( strcmp(sArg, "--trace-runtime") == 0 ) {
#if XLANG_DEBUGMODE_ENABLED
			pOpts->bTraceRuntime = true;
			continue;
#else
			XL_DIAG_ERROR(pDiag, "cli", "--trace-runtime is only available in xldbg");
			return XL_RC_BAD_ARG;
#endif
		}
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
	printf("  lex      tokenize source and dump tokens\n");
	printf("  parse    build AST from source\n");
	printf("  sem      run semantic analysis\n");
	printf("  lower    lower AST into HIR/MIR\n");
	printf("  cgen     generate C code\n");
#endif
	printf("  build    build source into an output artifact\n");
	printf("  run      run program through compiler pipeline\n");
	printf("  build-c  build a C source file through embedded TCC\n");
	printf("  run-c    run a C source file through embedded TCC\n\n");
	printf("  bind     generate an xlang binding package skeleton from a C header\n\n");
	printf("Options:\n");
#if XLANG_DEBUGMODE_ENABLED
	printf("  --dump-token      dump token stream\n");
	printf("  --dump-ast        dump AST\n");
	printf("  --dump-sem        dump semantic state\n");
	printf("  --dump-hir        dump HIR\n");
	printf("  --dump-mir        dump MIR\n");
	printf("  --dump-c          dump generated C\n");
	printf("  --dump-json       emit machine-readable dump when available\n");
	printf("  --diag-json       emit diagnostics as JSON lines\n");
	printf("  --trace-parser    enable parser trace\n");
	printf("  --trace-sem       enable semantic trace\n");
	printf("  --trace-lower     enable lowering trace\n");
	printf("  --trace-runtime   enable runtime trace\n");
#endif
	printf("  --emit <path>     write output to a path\n");
	printf("  --stop-after <phase>\n");
	printf("  -h, --help        print help\n");
}
