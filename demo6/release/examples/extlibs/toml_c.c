#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/toml/include/xrt_toml.h"

#include <stdio.h>



int main(void)
{
	static const char sSource[] =
		"title = \"xlang service\"\n"
		"started = 2026-07-16T08:30:00Z\n"
		"\n[server]\n"
		"host = \"127.0.0.1\"\n"
		"port = 8080\n";
	xrttomlerror tError;
	xvalue* pConfig;
	xvalue* pServer;
	xbuffer* pText;
	xstrview tTitle;
	xstrview tHost;
	bool bOk;
	pConfig = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u, NULL, &tError);
	if ( pConfig == NULL ) {
		fprintf(stderr, "%s\n", tError.sMessage);
		return 2;
	}
	pServer = xrtValueObjectGet(pConfig, xrtStrView("server"));
	if ( !xrtValueGetString(xrtValueObjectGet(pConfig, xrtStrView("title")), &tTitle) ||
		!xrtValueGetString(xrtValueObjectGet(pServer, xrtStrView("host")), &tHost) ||
		!xrtValueObjectSetNew(pConfig, xrtStrView("released"),
			xrtTomlCreateDate(2026, 7, 16)) ) {
		xrtValueRelease(pConfig);
		return 3;
	}
	printf("%.*s\n", (int)tTitle.Size, tTitle.Data);
	printf("%.*s\n", (int)tHost.Size, tHost.Data);
	pText = xrtTomlStringify(pConfig, NULL, &tError);
	bOk = pText != NULL;
	if ( pText != NULL ) { printf("%.*s", (int)pText->Size, pText->Data); }
	xrtBufferDestroy(pText);
	xrtValueRelease(pConfig);
	puts("toml C example: PASS");
	return bOk ? 0 : 4;
}

/* 单文件范例由 xl 注入 XRT；正式 package 通过依赖清单编译。 */
#include "../../extlibs/_tomlc17/tomlc17_xrt.c"
#include "../../extlibs/toml/xrt_toml_types.c"
#include "../../extlibs/toml/xrt_toml_parser.c"
#include "../../extlibs/toml/xrt_toml_writer.c"
