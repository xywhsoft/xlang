#include "xl_cli_debug.h"

#if XLANG_DEBUGMODE_ENABLED
static XL_CliDebugParseResult __xlCliDebugConsumeFlag(
	const char* sArg,
	const char* sName,
	bool* pFlag
)
{
	if ( strcmp(sArg, sName) != 0 ) {
		return XL_CLI_DEBUG_PARSE_NOT_HANDLED;
	}

	if ( pFlag != NULL ) {
		*pFlag = true;
	}
	return XL_CLI_DEBUG_PARSE_HANDLED;
}

XL_CliDebugParseResult xlCliTryParseDebugOption(
	int* pArgIndex,
	int argc,
	char** argv,
	XL_CliOptions* pOpts,
	XL_DiagContext* pDiag
)
{
	const char* sArg;
	XL_CliDebugParseResult eResult;

	(void)argc;
	(void)pDiag;

	if ( (pArgIndex == NULL) || (argv == NULL) || (pOpts == NULL) ) {
		return XL_CLI_DEBUG_PARSE_ERROR;
	}

	sArg = argv[*pArgIndex];
	if ( sArg == NULL ) {
		return XL_CLI_DEBUG_PARSE_NOT_HANDLED;
	}

	eResult = __xlCliDebugConsumeFlag(sArg, "--dump-token", &XL_CLI_DUMP_TOKEN(pOpts));
	if ( eResult != XL_CLI_DEBUG_PARSE_NOT_HANDLED ) return eResult;
	eResult = __xlCliDebugConsumeFlag(sArg, "--dump-ast", &XL_CLI_DUMP_AST(pOpts));
	if ( eResult != XL_CLI_DEBUG_PARSE_NOT_HANDLED ) return eResult;
	eResult = __xlCliDebugConsumeFlag(sArg, "--dump-sem", &XL_CLI_DUMP_SEM(pOpts));
	if ( eResult != XL_CLI_DEBUG_PARSE_NOT_HANDLED ) return eResult;
	eResult = __xlCliDebugConsumeFlag(sArg, "--dump-hir", &XL_CLI_DUMP_HIR(pOpts));
	if ( eResult != XL_CLI_DEBUG_PARSE_NOT_HANDLED ) return eResult;
	eResult = __xlCliDebugConsumeFlag(sArg, "--dump-mir", &XL_CLI_DUMP_MIR(pOpts));
	if ( eResult != XL_CLI_DEBUG_PARSE_NOT_HANDLED ) return eResult;
	eResult = __xlCliDebugConsumeFlag(sArg, "--dump-c", &XL_CLI_DUMP_C(pOpts));
	if ( eResult != XL_CLI_DEBUG_PARSE_NOT_HANDLED ) return eResult;
	eResult = __xlCliDebugConsumeFlag(sArg, "--trace-parser", &XL_CLI_TRACE_PARSER(pOpts));
	if ( eResult != XL_CLI_DEBUG_PARSE_NOT_HANDLED ) return eResult;
	eResult = __xlCliDebugConsumeFlag(sArg, "--trace-sem", &XL_CLI_TRACE_SEM(pOpts));
	if ( eResult != XL_CLI_DEBUG_PARSE_NOT_HANDLED ) return eResult;
	eResult = __xlCliDebugConsumeFlag(sArg, "--trace-lower", &XL_CLI_TRACE_LOWER(pOpts));
	if ( eResult != XL_CLI_DEBUG_PARSE_NOT_HANDLED ) return eResult;
	eResult = __xlCliDebugConsumeFlag(sArg, "--trace-runtime", &XL_CLI_TRACE_RUNTIME(pOpts));
	if ( eResult != XL_CLI_DEBUG_PARSE_NOT_HANDLED ) return eResult;

	if ( strcmp(sArg, "--listen") == 0 ) {
		if ( (*pArgIndex + 1) >= argc ) {
			XL_DIAG_ERROR(pDiag, "cli", "--listen requires an address");
			return XL_CLI_DEBUG_PARSE_ERROR;
		}
		pOpts->dbg.sDapListen = argv[++(*pArgIndex)];
		return XL_CLI_DEBUG_PARSE_HANDLED;
	}

	return XL_CLI_DEBUG_PARSE_NOT_HANDLED;
}

void xlCliPrintDebugCommandHelp(void)
{
	printf("  lex      tokenize source and dump tokens\n");
	printf("  parse    build AST from source\n");
	printf("  sem      run semantic analysis\n");
	printf("  lower    lower AST into HIR/MIR\n");
	printf("  cgen     generate C code\n");
	printf("  dbg      run source through the interactive debugger\n");
	printf("  dap      run the Debug Adapter Protocol server\n");
}

void xlCliPrintDebugOptionHelp(void)
{
	printf("  --dump-token      dump token stream\n");
	printf("  --dump-ast        dump AST\n");
	printf("  --dump-sem        dump semantic state\n");
	printf("  --dump-hir        dump HIR\n");
	printf("  --dump-mir        dump MIR\n");
	printf("  --dump-c          dump generated C\n");
	printf("  --trace-parser    enable parser trace\n");
	printf("  --trace-sem       enable semantic trace\n");
	printf("  --trace-lower     enable lowering trace\n");
	printf("  --trace-runtime   enable runtime trace\n");
	printf("  --listen <addr>   DAP listen address, reserved for TCP mode\n");
}
#endif
