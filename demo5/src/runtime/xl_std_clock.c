#include "../common/xl_common.h"

const char* xlStdClockModuleSource(void)
{
	return
		"import std.time\n"
		"\n"
		"func now() as time\n"
		"\treturn time.now()\n"
		"end\n"
		"func text() as string\n"
		"\treturn time.now_text()\n"
		"end\n"
		"func date() as string\n"
		"\treturn time.date_text()\n"
		"end\n"
		"func time_of_day() as string\n"
		"\treturn time.time_text()\n"
		"end\n"
		"func sleep(int ms) as void\n"
		"\ttime.sleep(ms)\n"
		"end\n"
		"\n"
		"const int millis_per_second = 1000\n"
		"const int seconds_per_day = 86400\n"
		"\n"
		"export now\n"
		"export text\n"
		"export date\n"
		"export time_of_day\n"
		"export sleep\n"
		"export millis_per_second\n"
		"export seconds_per_day\n";
}
