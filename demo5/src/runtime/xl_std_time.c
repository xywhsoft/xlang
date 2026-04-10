#include "../common/xl_common.h"

const char* xlStdTimeModuleSource(void)
{
	return
		"func now() as time\n"
		"end\n"
		"func to_string(time value) as string\n"
		"end\n"
		"func now_text() as string\n"
		"end\n"
		"func date_text() as string\n"
		"end\n"
		"func time_text() as string\n"
		"end\n"
		"func relative(time value, time base) as string\n"
		"end\n"
		"func parse(string value) as time\n"
		"end\n"
		"\n"
		"const int 每秒毫秒 = 1000\n"
		"const int 每天秒数 = 86400\n"
		"\n"
		"export now\n"
		"export to_string\n"
		"export now_text\n"
		"export date_text\n"
		"export time_text\n"
		"export relative\n"
		"export parse\n"
		"export 每秒毫秒\n"
		"export 每天秒数\n";
}
