#include "../common/xl_common.h"

const char* xlStdPathModuleSource(void)
{
	return
		"func join(string left, string right) as string\n"
		"end\n"
		"func name_ext(string value) as string\n"
		"end\n"
		"func name(string value) as string\n"
		"end\n"
		"func ext(string value) as string\n"
		"end\n"
		"func dir(string value) as string\n"
		"end\n"
		"func is_abs(string value) as bool\n"
		"end\n"
		"func random(string head, string foot, int len) as string\n"
		"end\n"
		"\n"
		"export join\n"
		"export name_ext\n"
		"export name\n"
		"export ext\n"
		"export dir\n"
		"export is_abs\n"
		"export random\n";
}
