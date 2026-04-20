#include "../common/xl_common.h"

const char* xlStdNetWsModuleSource(void)
{
	return
		"func key() as string\n"
		"end\n"
		"func accept(string key) as string\n"
		"end\n"
		"func request(string host, string path, string key) as string\n"
		"end\n"
		"func response(string key) as string\n"
		"end\n"
		"func frame_text(string text, bool mask) as string\n"
		"end\n"
		"func unframe(string frame) as dict<string>\n"
		"end\n"
		"\n"
		"export key\n"
		"export accept\n"
		"export request\n"
		"export response\n"
		"export frame_text\n"
		"export unframe\n";
}
