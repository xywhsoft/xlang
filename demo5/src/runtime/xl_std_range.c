#include "../common/xl_common.h"

const char* xlStdRangeModuleSource(void)
{
	return
		"func collect(int stop) as array<int>\n"
		"end\n"
		"func collect(int start, int stop) as array<int>\n"
		"end\n"
		"func collect(int start, int stop, int step) as array<int>\n"
		"end\n"
		"\n"
		"export collect\n";
}
