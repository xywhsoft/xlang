#include "../common/xl_common.h"

const char* xlStdThreadModuleSource(void)
{
	return
		"func current_id() as int\n"
		"end\n"
		"func sleep(int ms) as void\n"
		"end\n"
		"func yield_now() as void\n"
		"end\n"
		"\n"
		"export current_id\n"
		"export sleep\n"
		"export yield_now\n";
}
