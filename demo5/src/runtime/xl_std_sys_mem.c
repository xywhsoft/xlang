#include "../common/xl_common.h"

const char* xlStdSysMemModuleSource(void)
{
	return
		"func alloc(int size) as point\n"
		"end\n"
		"func free(point value) as bool\n"
		"end\n"
		"func is_null(point value) as bool\n"
		"end\n"
		"func fill(point value, int byte_value, int size) as bool\n"
		"end\n"
		"func copy(point dst, point src, int size) as bool\n"
		"end\n"
		"func write_string(point value, string text) as int\n"
		"end\n"
		"func read_string(point value, int size) as string\n"
		"end\n"
		"\n"
		"export alloc\n"
		"export free\n"
		"export is_null\n"
		"export fill\n"
		"export copy\n"
		"export write_string\n"
		"export read_string\n";
}
