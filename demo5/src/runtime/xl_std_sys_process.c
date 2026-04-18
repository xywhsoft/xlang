#include "../common/xl_common.h"

const char* xlStdSysProcessModuleSource(void)
{
	return
		"import std.process\n"
		"\n"
		"func exec(string command) as int\n"
		"\treturn process.exec(command)\n"
		"end\n"
		"func capture(string command) as string\n"
		"\treturn process.capture(command)\n"
		"end\n"
		"func exec_in(string command, string workdir) as int\n"
		"\treturn process.exec_in(command, workdir)\n"
		"end\n"
		"func capture_in(string command, string workdir) as string\n"
		"\treturn process.capture_in(command, workdir)\n"
		"end\n"
		"func terminal_supported() as bool\n"
		"\treturn process.terminal_supported()\n"
		"end\n"
		"\n"
		"export exec\n"
		"export capture\n"
		"export exec_in\n"
		"export capture_in\n"
		"export terminal_supported\n";
}
