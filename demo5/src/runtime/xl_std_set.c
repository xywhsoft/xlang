#include "../common/xl_common.h"

const char* xlStdSetModuleSource(void)
{
	return
		"func len(set values) as int\n"
		"end\n"
		"func has(set values, var item) as bool\n"
		"end\n"
		"func clear(set values) as void\n"
		"end\n"
		"func remove(set values, var item) as bool\n"
		"end\n"
		"\n"
		"export len\n"
		"export has\n"
		"export clear\n"
		"export remove\n";
}
