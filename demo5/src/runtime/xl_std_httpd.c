#include "../common/xl_common.h"

const char* xlStdNetHttpdModuleSource(void)
{
	return
		"func request(string text) as dict<string>\n"
		"end\n"
		"func match(string pattern, string path) as dict<string>\n"
		"end\n"
		"func response(int status, dict<string> headers, string body) as string\n"
		"end\n"
		"func text(int status, string body) as string\n"
		"end\n"
		"\n"
		"export request\n"
		"export match\n"
		"export response\n"
		"export text\n";
}
