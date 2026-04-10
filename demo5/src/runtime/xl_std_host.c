#include "../common/xl_common.h"

const char* xlStdHostModuleSource(void)
{
	return
		"func name() as string\n"
		"end\n"
		"func ip() as string\n"
		"end\n"
		"\n"
		"export name\n"
		"export ip\n";
}
