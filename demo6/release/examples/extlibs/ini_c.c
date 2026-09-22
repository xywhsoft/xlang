#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/ini/include/xrt_ini.h"

#include <stdio.h>



int main(void)
{
	const char sSource[] =
		"[server]\n"
		"host = localhost\n"
		"port = 8080\n";
	xrtiniconfig* pConfig;
	int64 iPort = 0;
	int bOk;
	pConfig = xrtIniParseMemory(sSource, sizeof(sSource) - 1u, NULL);
	bOk = pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_OK &&
		xrtIniGetInt(pConfig, (str)"server", (str)"port", &iPort) && iPort == 8080;
	if ( bOk ) {
		printf("server=%s:%lld\n", xrtIniGetRaw(pConfig, (str)"server", (str)"host"),
			(long long)iPort);
	}
	xrtIniDestroy(pConfig);
	if ( bOk ) { puts("ini C example: PASS"); }
	return bOk ? 0 : 2;
}

/* 源码型扩展库的 C 范例保持 `xl run ini_c.c` 可直接运行。 */
#include "../../extlibs/ini/xrt_ini_model.c"
#include "../../extlibs/ini/xrt_ini_parser.c"
#include "../../extlibs/ini/xrt_ini_writer.c"
