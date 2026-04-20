#include "../common/xl_common.h"

const char* xlStdNetHttpModuleSource(void)
{
	return
		"func headers(dict<string> values) as string\n"
		"end\n"
		"func request(string method, string target, dict<string> headers, string body) as string\n"
		"end\n"
		"func response(int status, string reason, dict<string> headers, string body) as string\n"
		"end\n"
		"func parse_headers(string text) as dict<string>\n"
		"end\n"
		"\n"
		"export headers\n"
		"export request\n"
		"export response\n"
		"export parse_headers\n";
}
