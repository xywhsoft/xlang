#include "../common/xl_common.h"

const char* xlStdNetUrlModuleSource(void)
{
	return
		"func encode_query(dict<string> values) as string\n"
		"end\n"
		"func decode_query(string text) as dict<string>\n"
		"end\n"
		"\n"
		"func parse(string text) as dict<string>\n"
		"end\n"
		"\n"
		"func build(dict<string> parts) as string\n"
		"end\n"
		"\n"
		"export encode_query\n"
		"export decode_query\n"
		"export parse\n"
		"export build\n";
}
