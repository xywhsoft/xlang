#include "../common/xl_common.h"

const char* xlStdNetMimeModuleSource(void)
{
	return
		"func by_ext(string ext) as string\n"
		"end\n"
		"func by_path(string path) as string\n"
		"end\n"
		"func is_text(string mime) as bool\n"
		"end\n"
		"func parse(string text) as dict<string>\n"
		"end\n"
		"func build(dict<string> value) as string\n"
		"end\n"
		"\n"
		"export by_ext\n"
		"export by_path\n"
		"export is_text\n"
		"export parse\n"
		"export build\n";
}
