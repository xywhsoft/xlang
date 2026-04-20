#include "../common/xl_common.h"

const char* xlStdTypesModuleSource(void)
{
	return
		"func of(var value) as type\n"
		"\treturn Type(value)\n"
		"end\n"
		"func name(type value) as string\n"
		"\treturn value.name\n"
		"end\n"
		"func same(type left, type right) as bool\n"
		"\treturn left == right\n"
		"end\n"
		"\n"
		"export of\n"
		"export name\n"
		"export same\n";
}
