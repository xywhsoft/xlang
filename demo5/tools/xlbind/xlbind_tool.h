#ifndef XLBIND_TOOL_H
#define XLBIND_TOOL_H

#include "../../src/common/xl_common.h"

typedef struct XLBindOptions {
	const char* sInputFile;
	const char* sEmitPath;
	bool bHelp;
} XLBindOptions;

void xlBindInit(XLBindOptions* pOpts);
int xlBindParse(int argc, char** argv, XLBindOptions* pOpts);
void xlBindPrintHelp(const char* sProgName);
int xlBindRun(const XLBindOptions* pOpts);

#endif
