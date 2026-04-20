#include "../common/xl_common.h"

const char* xlStdNetDnsModuleSource(void)
{
	return
		"func resolve(string host) as string\n"
		"end\n"
		"func resolve_all(string host) as array<string>\n"
		"end\n"
		"func reverse(string ip) as string\n"
		"end\n"
		"\n"
		"export resolve\n"
		"export resolve_all\n"
		"export reverse\n";
}
