#include "../common/xl_common.h"

const char* xlStdIterModuleSource(void)
{
	return
		"func count(array values) as int\n"
		"end\n"
		"func count(list values) as int\n"
		"end\n"
		"func count(set values) as int\n"
		"end\n"
		"func count(dict values) as int\n"
		"end\n"
		"func keys(dict values) as array<string>\n"
		"end\n"
		"func values(dict values) as array<var>\n"
		"end\n"
		"func items(set values) as array<var>\n"
		"end\n"
		"\n"
		"export count\n"
		"export keys\n"
		"export values\n"
		"export items\n";
}
