#include "../common/xl_common.h"

const char* xlStdDictModuleSource(void)
{
	return
		"func len(dict values) as int\n"
		"end\n"
		"func has(dict values, string key) as bool\n"
		"end\n"
		"func clear(dict values) as void\n"
		"end\n"
		"func remove(dict values, string key) as bool\n"
		"end\n"
		"\n"
		"export len\n"
		"export has\n"
		"export clear\n"
		"export remove\n";
}
