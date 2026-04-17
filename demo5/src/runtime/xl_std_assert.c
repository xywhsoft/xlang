#include "../common/xl_common.h"

const char* xlStdAssertModuleSource(void)
{
	return
		"func that(bool cond) as void\n"
		"\tassert cond\n"
		"end\n"
		"func that(bool cond, string message) as void\n"
		"\tassert cond, message\n"
		"end\n"
		"func fail() as void\n"
		"\tassert false\n"
		"end\n"
		"func fail(string message) as void\n"
		"\tassert false, message\n"
		"end\n"
		"func equal(int left, int right, string message) as void\n"
		"\tassert left == right, message\n"
		"end\n"
		"func equal(float left, float right, string message) as void\n"
		"\tassert left == right, message\n"
		"end\n"
		"func equal(string left, string right, string message) as void\n"
		"\tassert left == right, message\n"
		"end\n"
		"func equal(bool left, bool right, string message) as void\n"
		"\tassert left == right, message\n"
		"end\n"
		"\n"
		"export that\n"
		"export fail\n"
		"export equal\n";
}
