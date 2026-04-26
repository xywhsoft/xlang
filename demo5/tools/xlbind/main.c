#include "xlbind_tool.h"

int main(int argc, char** argv)
{
	XLBindOptions opts;
	int iRc;

	xlBindInit(&opts);
	iRc = xlBindParse(argc, argv, &opts);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	if ( opts.bHelp ) {
		xlBindPrintHelp((argc > 0) ? argv[0] : "xlbind");
		return XL_RC_OK;
	}
	return xlBindRun(&opts);
}
