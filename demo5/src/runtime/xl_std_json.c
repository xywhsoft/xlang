#include "../common/xl_common.h"

const char* xlStdJsonModuleSource(void)
{
	return
		"func encode(var value) as string\n"
		"end\n"
		"func decode(string text) as var\n"
		"end\n"
		"\n"
		"export encode\n"
		"export decode\n";
}
