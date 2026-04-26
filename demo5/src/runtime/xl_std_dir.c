#include "../common/xl_common.h"

const char* xlStdDirModuleSource(void)
{
	return
		"func make(string path) as bool\n"
		"end\n"
		"func list(string path) as string\n"
		"end\n"
		"func files(string path) as string\n"
		"end\n"
		"func dirs(string path) as string\n"
		"end\n"
		"func tree(string path) as string\n"
		"end\n"
		"func copy(string src, string dst) as bool\n"
		"end\n"
		"func move(string src, string dst) as bool\n"
		"end\n"
		"func remove(string path) as bool\n"
		"end\n"
		"\n"
		"export make\n"
		"export list\n"
		"export files\n"
		"export dirs\n"
		"export tree\n"
		"export copy\n"
		"export move\n"
		"export remove\n";
}
