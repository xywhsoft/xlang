#include "../common/xl_common.h"

const char* xlStdNetTlsModuleSource(void)
{
	return
		"func versions() as array<string>\n"
		"end\n"
		"func is_tls_url(string url) as bool\n"
		"end\n"
		"func default_port(string scheme) as int\n"
		"end\n"
		"func sni_host(string url) as string\n"
		"end\n"
		"func fingerprint_sha256(string data) as string\n"
		"end\n"
		"func client_config(string host, bool verify) as dict<string>\n"
		"end\n"
		"\n"
		"export versions\n"
		"export is_tls_url\n"
		"export default_port\n"
		"export sni_host\n"
		"export fingerprint_sha256\n"
		"export client_config\n";
}
