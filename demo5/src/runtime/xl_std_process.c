#include "../common/xl_common.h"

const char* xlStdProcessModuleSource(void)
{
	return
		"func exec(string command) as int\n"
		"end\n"
		"func capture(string command) as string\n"
		"end\n"
		"func exec_in(string command, string workdir) as int\n"
		"end\n"
		"func capture_in(string command, string workdir) as string\n"
		"end\n"
		"\n"
		"export exec\n"
		"export capture\n"
		"export exec_in\n"
		"export capture_in\n";
}
