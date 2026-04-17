#include "../common/xl_common.h"

const char* xlStdCryptoModuleSource(void)
{
	return
		"func hmac_sha256(string key, string text) as string\n"
		"end\n"
		"func hmac_sha512(string key, string text) as string\n"
		"end\n"
		"\n"
		"export hmac_sha256\n"
		"export hmac_sha512\n";
}
