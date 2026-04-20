#include "../common/xl_common.h"

const char* xlStdNetUdpModuleSource(void)
{
	return
		"func bind(string host, int port) as point\n"
		"end\n"
		"func send_to(point socket, string host, int port, string text) as int\n"
		"end\n"
		"func recv_from(point socket, int size, int timeout_ms) as dict<string>\n"
		"end\n"
		"func local_port(point socket) as int\n"
		"end\n"
		"func close(point socket) as bool\n"
		"end\n"
		"\n"
		"export bind\n"
		"export send_to\n"
		"export recv_from\n"
		"export local_port\n"
		"export close\n";
}
