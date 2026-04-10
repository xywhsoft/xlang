#include "../common/xl_common.h"

const char* xlStdStrModuleSource(void)
{
	return
		"func contains(string text, string part) as bool\n"
		"end\n"
		"func starts_with(string text, string part) as bool\n"
		"end\n"
		"func ends_with(string text, string part) as bool\n"
		"end\n"
		"func lower(string text) as string\n"
		"end\n"
		"func upper(string text) as string\n"
		"end\n"
		"func replace(string text, string part, string value) as string\n"
		"end\n"
		"\n"
		"export contains\n"
		"export starts_with\n"
		"export ends_with\n"
		"export lower\n"
		"export upper\n"
		"export replace\n";
}
