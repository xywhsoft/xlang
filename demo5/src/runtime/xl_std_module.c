#include "../common/xl_common.h"

const char* xlStdModuleModuleSource(void)
{
	return
		"func key(module value) as string\n"
		"end\n"
		"func abi_id(module value) as string\n"
		"end\n"
		"func init_symbol(module value) as string\n"
		"end\n"
		"func exports_symbol(module value) as string\n"
		"end\n"
		"func descriptor_symbol(module value) as string\n"
		"end\n"
		"func export_count(module value) as int\n"
		"end\n"
		"func exports(module value) as array<var>\n"
		"end\n"
		"func has_export(module value, string name) as bool\n"
		"end\n"
		"func export_symbol(module value, string name) as string\n"
		"end\n"
		"\n"
		"export key\n"
		"export abi_id\n"
		"export init_symbol\n"
		"export exports_symbol\n"
		"export descriptor_symbol\n"
		"export export_count\n"
		"export exports\n"
		"export has_export\n"
		"export export_symbol\n";
}
