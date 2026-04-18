#include "../common/xl_common.h"

const char* xlStdChannelModuleSource(void)
{
	return
		"func make() as channel\n"
		"\treturn channel(0)\n"
		"end\n"
		"func make(int capacity) as channel\n"
		"\treturn channel(capacity)\n"
		"end\n"
		"func send(channel value, int item) as bool\n"
		"\treturn value.send(item)\n"
		"end\n"
		"func send(channel value, float item) as bool\n"
		"\treturn value.send(item)\n"
		"end\n"
		"func send(channel value, string item) as bool\n"
		"\treturn value.send(item)\n"
		"end\n"
		"func send(channel value, bool item) as bool\n"
		"\treturn value.send(item)\n"
		"end\n"
		"func send(channel value, error item) as bool\n"
		"\treturn value.send(item)\n"
		"end\n"
		"func close(channel value) as bool\n"
		"\treturn value.close()\n"
		"end\n"
		"\n"
		"export make\n"
		"export send\n"
		"export close\n";
}
