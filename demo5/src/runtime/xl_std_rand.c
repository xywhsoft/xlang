#include "../common/xl_common.h"

const char* xlStdRandModuleSource(void)
{
	return
		"func int32() as int\n"
		"end\n"
		"func range(int min, int max) as int\n"
		"end\n"
		"func text(int len) as string\n"
		"end\n"
		"\n"
		"export int32\n"
		"export range\n"
		"export text\n";
}
