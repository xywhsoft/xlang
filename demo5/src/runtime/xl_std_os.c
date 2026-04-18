#include "../common/xl_common.h"

const char* xlStdOsModuleSource(void)
{
	return
		"import std.host\n"
		"\n"
		"func name() as string\n"
		"\treturn host.os()\n"
		"end\n"
		"func arch() as string\n"
		"\treturn host.arch()\n"
		"end\n"
		"func user() as string\n"
		"\treturn host.user()\n"
		"end\n"
		"func host_name() as string\n"
		"\treturn host.name()\n"
		"end\n"
		"func executable() as string\n"
		"\treturn host.executable()\n"
		"end\n"
		"\n"
		"export name\n"
		"export arch\n"
		"export user\n"
		"export host_name\n"
		"export executable\n";
}
