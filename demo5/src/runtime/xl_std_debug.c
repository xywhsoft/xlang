#include "../common/xl_common.h"

const char* xlStdDebugModuleSource(void)
{
	return
		"func text(error value) as string\n"
		"end\n"
		"func text(var value) as string\n"
		"end\n"
		"func print(error value) as void\n"
		"end\n"
		"func print(var value) as void\n"
		"end\n"
		"func println(error value) as void\n"
		"end\n"
		"func println(var value) as void\n"
		"end\n"
		"\n"
		"export text\n"
		"export print\n"
		"export println\n";
}
