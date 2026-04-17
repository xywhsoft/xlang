#include "../common/xl_common.h"

const char* xlStdHashModuleSource(void)
{
	return
		"func hash32(string text) as int\n"
		"end\n"
		"func hash64(string text) as string\n"
		"end\n"
		"func hash64_micro(string text) as string\n"
		"end\n"
		"func hash64_nano(string text) as string\n"
		"end\n"
		"func sha256(string text) as string\n"
		"end\n"
		"func sha512(string text) as string\n"
		"end\n"
		"\n"
		"export hash32\n"
		"export hash64\n"
		"export hash64_micro\n"
		"export hash64_nano\n"
		"export sha256\n"
		"export sha512\n";
}
