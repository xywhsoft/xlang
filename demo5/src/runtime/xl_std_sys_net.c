#include "../common/xl_common.h"

const char* xlStdSysNetModuleSource(void)
{
	return
		"func resolve(string host) as string\n"
		"end\n"
		"\n"
		"export resolve\n";
}
