#include "../common/xl_common.h"

const char* xlStdQueryModuleSource(void)
{
	return
		"func encode(dict<string> values) as string\n"
		"end\n"
		"func decode(string text) as dict<string>\n"
		"end\n"
		"\n"
		"export encode\n"
		"export decode\n";
}
