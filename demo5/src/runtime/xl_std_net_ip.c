#include "../common/xl_common.h"

const char* xlStdNetIpModuleSource(void)
{
	return
		"func parse(string text) as dict<string>\n"
		"end\n"
		"func build(dict<string> value) as string\n"
		"end\n"
		"func normalize(string text) as string\n"
		"end\n"
		"func is_ip(string text) as bool\n"
		"end\n"
		"func is_ipv4(string text) as bool\n"
		"end\n"
		"func is_ipv6(string text) as bool\n"
		"end\n"
		"\n"
		"export parse\n"
		"export build\n"
		"export normalize\n"
		"export is_ip\n"
		"export is_ipv4\n"
		"export is_ipv6\n";
}
