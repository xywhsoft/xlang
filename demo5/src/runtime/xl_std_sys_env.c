#include "../common/xl_common.h"

const char* xlStdSysEnvModuleSource(void)
{
	return
		"import std.env\n"
		"\n"
		"func read(string name) as string\n"
		"\treturn env.read(name)\n"
		"end\n"
		"func exists(string name) as bool\n"
		"\treturn env.exists(name)\n"
		"end\n"
		"func set(string name, string value) as bool\n"
		"\treturn env.set(name, value)\n"
		"end\n"
		"func cwd() as string\n"
		"\treturn env.cwd()\n"
		"end\n"
		"func temp() as string\n"
		"\treturn env.temp()\n"
		"end\n"
		"func home() as string\n"
		"\treturn env.home()\n"
		"end\n"
		"\n"
		"export read\n"
		"export exists\n"
		"export set\n"
		"export cwd\n"
		"export temp\n"
		"export home\n";
}
