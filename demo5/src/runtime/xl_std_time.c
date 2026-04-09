#include "../common/xl_common.h"

const char* xlStdTimeModuleSource(void)
{
	return
		"const int 每秒毫秒 = 1000\n"
		"const int 每天秒数 = 86400\n"
		"\n"
		"export 每秒毫秒\n"
		"export 每天秒数\n";
}
