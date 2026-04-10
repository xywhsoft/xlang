#include "../common/xl_common.h"

const char* xlStdDirModuleSource(void)
{
	return
		"func list(string path) as string\n"
		"end\n"
		"func files(string path) as string\n"
		"end\n"
		"func dirs(string path) as string\n"
		"end\n"
		"func tree(string path) as string\n"
		"end\n"
		"\n"
		"export list\n"
		"export files\n"
		"export dirs\n"
		"export tree\n";
}
