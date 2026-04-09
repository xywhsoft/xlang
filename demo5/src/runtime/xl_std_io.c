#include "../common/xl_common.h"

const char* xlStdIoModuleSource(void)
{
	return
		"func print(int x) as void\n"
		"end\n"
		"func print(float x) as void\n"
		"end\n"
		"func print(string x) as void\n"
		"end\n"
		"func print(bool x) as void\n"
		"end\n"
		"\n"
		"func println() as void\n"
		"end\n"
		"func println(int x) as void\n"
		"end\n"
		"func println(float x) as void\n"
		"end\n"
		"func println(string x) as void\n"
		"end\n"
		"func println(bool x) as void\n"
		"end\n"
		"\n"
		"export print\n"
		"export println\n";
}
