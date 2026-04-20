#include "../common/xl_common.h"

const char* xlStdNetModuleSource(void)
{
	return
		"import std.sys.net as sys_net\n"
		"\n"
		"func resolve(string host) as string\n"
		"\treturn sys_net.resolve(host)\n"
		"end\n"
		"\n"
		"export resolve\n";
}
