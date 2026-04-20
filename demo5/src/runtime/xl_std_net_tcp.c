#include "../common/xl_common.h"

const char* xlStdNetTcpModuleSource(void)
{
	return
		"func listen(string host, int port) as point\n"
		"end\n"
		"func accept(point listener, int timeout_ms) as point\n"
		"end\n"
		"func connect(string host, int port) as point\n"
		"end\n"
		"func send(point socket, string text) as int\n"
		"end\n"
		"func recv(point socket, int size, int timeout_ms) as string\n"
		"end\n"
		"func local_port(point socket) as int\n"
		"end\n"
		"func close(point socket) as bool\n"
		"end\n"
		"\n"
		"export listen\n"
		"export accept\n"
		"export connect\n"
		"export send\n"
		"export recv\n"
		"export local_port\n"
		"export close\n";
}
