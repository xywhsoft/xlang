#include "../common/xl_common.h"

const char* xlStdObjectModuleSource(void)
{
	return
		"func weak(var value) as point\n"
		"end\n"
		"func alive(point value) as bool\n"
		"end\n"
		"func lock(point value) as var\n"
		"end\n"
		"func release(point value) as bool\n"
		"end\n"
		"func refcount(var value) as int\n"
		"end\n"
		"func unique(var value) as bool\n"
		"end\n"
		"func collect() as int\n"
		"end\n"
		"\n"
		"export weak\n"
		"export alive\n"
		"export lock\n"
		"export release\n"
		"export refcount\n"
		"export unique\n"
		"export collect\n";
}
