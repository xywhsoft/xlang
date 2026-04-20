#include "../common/xl_common.h"

const char* xlStdNetProxyModuleSource(void)
{
	return
		"func parse(string url) as dict<string>\n"
		"end\n"
		"func basic_auth(string user, string pass) as string\n"
		"end\n"
		"func http_connect(string host, int port, string auth) as string\n"
		"end\n"
		"func socks5_greeting_hex(bool auth) as string\n"
		"end\n"
		"func socks5_connect_hex(string host, int port) as string\n"
		"end\n"
		"\n"
		"export parse\n"
		"export basic_auth\n"
		"export http_connect\n"
		"export socks5_greeting_hex\n"
		"export socks5_connect_hex\n";
}
