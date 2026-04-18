#include "../common/xl_common.h"

const char* xlStdTemplateModuleSource(void)
{
	return
		"func render(string template, dict<string> values) as string\n"
		"end\n"
		"\n"
		"export render\n";
}
