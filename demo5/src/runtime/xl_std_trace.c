#include "../common/xl_common.h"

const char* xlStdTraceModuleSource(void)
{
	return
		"func write(string text) as void\n"
		"end\n"
		"func error(error value) as void\n"
		"end\n"
		"\n"
		"export write\n"
		"export error\n";
}
