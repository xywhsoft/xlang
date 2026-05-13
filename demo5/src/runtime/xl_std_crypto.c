#include "../common/xl_common.h"

const char* xlStdCryptoModuleSource(void)
{
	return
		"func sha256_hex(string text) as string\n"
		"end\n"
		"func hmac_sha256(string key, string text) as string\n"
		"end\n"
		"func hmac_sha256_hex(string key, string text) as string\n"
		"end\n"
		"func hmac_sha256_hex_key(string hex_key, string text) as string\n"
		"end\n"
		"func hmac_sha512(string key, string text) as string\n"
		"end\n"
		"\n"
		"export sha256_hex\n"
		"export hmac_sha256\n"
		"export hmac_sha256_hex\n"
		"export hmac_sha256_hex_key\n"
		"export hmac_sha512\n";
}
