#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/ini/include/xrt_ini.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
	const char sSource[] =
		"[server]\n"
		"host = localhost\n"
		"port = 8080\n"
		"url = http://${host}:${port}/api\n";
	xrtinioptions tOptions;
	xrtiniconfig* pConfig;
	int64 iPort = 0;
	int bOk;
	str sUrl;
	xrtIniOptionsInit(&tOptions);
	tOptions.bInterpolation = true;
	pConfig = xrtIniParseMemory(sSource, sizeof(sSource) - 1u, &tOptions);
	bOk = pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_OK &&
		xrtIniGetInt(pConfig, (str)"server", (str)"port", &iPort) && iPort == 8080 &&
		xrtIniSet(pConfig, (str)"server", (str)"port", (str)"9090") &&
		xrtIniGetInt(pConfig, (str)"server", (str)"port", &iPort) && iPort == 9090;
	sUrl = bOk ? xrtIniGet(pConfig, (str)"server", (str)"url") : NULL;
	bOk = bOk && sUrl != NULL &&
		strcmp((const char*)sUrl, "http://localhost:9090/api") == 0;
	if ( bOk ) {
		printf("%s:%lld\n", xrtIniGetRaw(pConfig, (str)"server", (str)"host"),
			(long long)iPort);
		puts((const char*)sUrl);
	}
	xrtFree(sUrl);
	xrtIniDestroy(pConfig);
	if ( bOk ) { puts("ini example: PASS"); }
	return bOk ? 0 : 2;
}

/* 源码型扩展库的 C 范例保持 `xl run ini_c.c` 可直接运行。 */
#include "../../extlibs/ini/xrt_ini_model.c"
#include "../../extlibs/ini/xrt_ini_parser.c"
#include "../../extlibs/ini/xrt_ini_writer.c"
