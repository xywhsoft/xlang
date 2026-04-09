#include "src/common/xl_common.h"
#include "src/ast/xl_ast_dump.h"
#include "src/diag/xl_diag.h"
#include "src/diag/xl_dump.h"
#include "src/lexer/xl_lexer_api.h"
#include "src/parser/xl_parser_api.h"
#include "src/driver/xl_cli.h"
#include "src/driver/xl_driver.h"

int main(int argc, char** argv)
{
	XL_CliOptions opts;
	XL_DiagContext diag;
	XL_DumpContext dump;
	int iRc;

	xlCliInit(&opts);
	xlDiagInit(&diag, stderr, stderr);
	xlDumpInit(&dump, "release");

	iRc = xlCliParse(argc, argv, &opts, &diag);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	if ( opts.bHelp || (opts.eCommand == XL_CMD_NONE) ) {
		xlCliPrintHelp(argv[0]);
		return XL_RC_OK;
	}

	return xlDriverRun(&opts, &diag, &dump);
}
