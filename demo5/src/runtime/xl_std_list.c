#include "../common/xl_common.h"

const char* xlStdListModuleSource(void)
{
	return
		"func len(list values) as int\n"
		"end\n"
		"func append(list values, var item) as void\n"
		"end\n"
		"func join(list values, string sep) as string\n"
		"end\n"
		"func clear(list values) as void\n"
		"end\n"
		"func remove(list values, int index) as bool\n"
		"end\n"
		"\n"
		"export len\n"
		"export append\n"
		"export join\n"
		"export clear\n"
		"export remove\n";
}
