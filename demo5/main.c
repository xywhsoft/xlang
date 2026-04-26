#include "src/common/xl_common.h"
#include "src/diag/xl_diag.h"
#include "src/driver/xl_cli.h"
#include "src/driver/xl_driver.h"

int main(int argc, char** argv)
{
	XL_CliOptions opts;
	XL_DiagContext diag;
	int iRc;

	xlCliInit(&opts);
	xlDiagInit(&diag, stderr, stderr);

	iRc = xlCliParse(argc, argv, &opts, &diag);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	if ( opts.bHelp || (opts.eCommand == XL_CMD_NONE) ) {
		xlCliPrintHelp(argv[0]);
		return XL_RC_OK;
	}

	xlDiagSetDefaultSourceFile(&diag, opts.sInputFile);

	return xlDriverRunRelease(&opts, &diag);
}
