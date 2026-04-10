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
		"const int millis_per_second = 1000\n"
		"const int seconds_per_day = 86400\n"
		"\n"
		"export now\n"
		"export to_string\n"
		"export now_text\n"
		"export date_text\n"
		"export time_text\n"
		"export relative\n"
		"export parse\n"
		"export millis_per_second\n"
		"export seconds_per_day\n";
}
