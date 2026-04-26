#include "src/common/xl_common.h"
#include "src/diag/xl_diag.h"
#include "src/diag/xl_dump.h"
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
	xlDumpInit(&dump, NULL);

	iRc = xlCliParse(argc, argv, &opts, &diag);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	if ( opts.bHelp || (opts.eCommand == XL_CMD_NONE) ) {
		xlCliPrintHelp(argv[0]);
		return XL_RC_OK;
	}

	if ( XL_CLI_DIAG_JSON(&opts) ) {
		xlDiagSetFormat(&diag, XL_DIAG_FORMAT_JSON);
	}
	xlDiagSetDefaultSourceFile(&diag, opts.sInputFile);

	return xlDriverRunDebug(&opts, &diag, &dump);
}
