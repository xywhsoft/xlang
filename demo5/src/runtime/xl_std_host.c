#include "../common/xl_common.h"

const char* xlStdHostModuleSource(void)
{
	return
		"func name() as string\n"
		"end\n"
		"func ip() as string\n"
		"end\n"
		"func user() as string\n"
		"end\n"
		"func os() as string\n"
		"end\n"
		"func arch() as string\n"
		"end\n"
		"func executable() as string\n"
		"end\n"
		"\n"
		"export name\n"
		"export ip\n"
		"export user\n"
		"export os\n"
		"export arch\n"
		"export executable\n";
}
