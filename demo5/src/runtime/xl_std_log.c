#include "../common/xl_common.h"

const char* xlStdLogModuleSource(void)
{
	return
		"func debug(string text) as void\n"
		"end\n"
		"func info(string text) as void\n"
		"end\n"
		"func warn(string text) as void\n"
		"end\n"
		"func error(string text) as void\n"
		"end\n"
		"func error(error value) as void\n"
		"end\n"
		"\n"
		"export debug\n"
		"export info\n"
		"export warn\n"
		"export error\n";
}
