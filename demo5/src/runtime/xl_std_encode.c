#include "../common/xl_common.h"

const char* xlStdEncodeModuleSource(void)
{
	return
		"func hex_encode(string text) as string\n"
		"end\n"
		"func hex_decode(string text) as string\n"
		"end\n"
		"func base64_encode(string text) as string\n"
		"end\n"
		"func base64_decode(string text) as string\n"
		"end\n"
		"\n"
		"export hex_encode\n"
		"export hex_decode\n"
		"export base64_encode\n"
		"export base64_decode\n";
}
