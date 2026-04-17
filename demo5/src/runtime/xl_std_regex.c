#include "../common/xl_common.h"

const char* xlStdRegexModuleSource(void)
{
	return
		"func is_match(string pattern, string text) as bool\n"
		"end\n"
		"func find(string pattern, string text) as string\n"
		"end\n"
		"func captures(string pattern, string text) as list<string>\n"
		"end\n"
		"func capture_count(string pattern) as int\n"
		"end\n"
		"func replace_first(string pattern, string text, string value) as string\n"
		"end\n"
		"func replace_all(string pattern, string text, string value) as string\n"
		"end\n"
		"\n"
		"export is_match\n"
		"export find\n"
		"export captures\n"
		"export capture_count\n"
		"export replace_first\n"
		"export replace_all\n";
}
