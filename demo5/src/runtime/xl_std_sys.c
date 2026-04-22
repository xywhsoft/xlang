#include "../common/xl_common.h"

const char* xlStdSysLibModuleSource(void)
{
	return
		"func loadlib(string path) as var\n"
		"end\n"
		"func dlopen(string path) as var\n"
		"end\n"
		"func dlsym(var self, string name) as point\n"
		"end\n"
		"func dlclose(var self) as bool\n"
		"end\n"
		"func var.proc(var self, string name) as point\n"
		"end\n"
		"func var.close(var self) as bool\n"
		"end\n"
		"func var.path(var self) as string\n"
		"end\n"
		"func var.loaded(var self) as bool\n"
		"end\n"
		"\n"
		"export loadlib\n"
		"export dlopen\n"
		"export dlsym\n"
		"export dlclose\n"
		"export proc\n"
		"export close\n"
		"export path\n"
		"export loaded\n";
}
