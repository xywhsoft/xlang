#include "../common/xl_common.h"

const char* xlStdNetMultipartModuleSource(void)
{
	return
		"func boundary(string content_type) as string\n"
		"end\n"
		"func parse(string text, string boundary) as array<dict<string> >\n"
		"end\n"
		"func build(array<dict> parts, string boundary) as string\n"
		"end\n"
		"\n"
		"export boundary\n"
		"export parse\n"
		"export build\n";
}
