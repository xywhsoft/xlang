#include "../common/xl_common.h"

const char* xlStdAtomicModuleSource(void)
{
	return
		"func make(int value) as point\n"
		"end\n"
		"func free(point value) as bool\n"
		"end\n"
		"func load(point value) as int\n"
		"end\n"
		"func store(point value, int item) as int\n"
		"end\n"
		"func exchange(point value, int item) as int\n"
		"end\n"
		"func compare_exchange(point value, int expected, int desired) as int\n"
		"end\n"
		"func add(point value, int delta) as int\n"
		"end\n"
		"\n"
		"export make\n"
		"export free\n"
		"export load\n"
		"export store\n"
		"export exchange\n"
		"export compare_exchange\n"
		"export add\n";
}
