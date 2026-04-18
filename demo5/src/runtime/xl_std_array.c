#include "../common/xl_common.h"

const char* xlStdArrayModuleSource(void)
{
	return
		"func len(array values) as int\n"
		"end\n"
		"func append(array values, var item) as void\n"
		"end\n"
		"func join(array values, string sep) as string\n"
		"end\n"
		"func clear(array values) as void\n"
		"end\n"
		"func remove(array values, int index) as bool\n"
		"end\n"
		"\n"
		"export len\n"
		"export append\n"
		"export join\n"
		"export clear\n"
		"export remove\n";
}
