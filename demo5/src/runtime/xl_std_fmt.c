#include "../common/xl_common.h"

const char* xlStdFmtModuleSource(void)
{
	return
		"func int_text(int value, string pattern) as string\n"
		"end\n"
		"func float_text(float value, string pattern) as string\n"
		"end\n"
		"\n"
		"export int_text\n"
		"export float_text\n";
}
