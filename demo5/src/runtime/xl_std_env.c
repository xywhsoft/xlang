#include "../common/xl_common.h"

const char* xlStdEnvModuleSource(void)
{
	return
		"func get(string name) as string\n"
		"end\n"
		"func exists(string name) as bool\n"
		"end\n"
		"func cwd() as string\n"
		"end\n"
		"\n"
		"export get\n"
		"export exists\n"
		"export cwd\n";
}
