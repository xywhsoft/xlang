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
	xvalue pConfig;
	xbuffer pText;
	if ( xrtInit() == NULL ) { return 1; }
	pConfig = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u, NULL, &tError);
	if ( pConfig == NULL ) {
		fprintf(stderr, "%s\n", tError.sMessage);
		xrtUnit();
		return 2;
	}
	printf("%s\n", (const char*)xvoTableGetText(pConfig, "title", 0u));
	printf("%s\n", (const char*)xvoTableGetText(
		xvoTableGetValue(pConfig, "server", 0u), "host", 0u));
	xvoTableSetValue(pConfig, "released", 0u,
		xrtTomlCreateDate(2026, 7, 16), TRUE);
	pText = xrtTomlStringify(pConfig, NULL, &tError);
	if ( pText != NULL ) { printf("%.*s", (int)pText->Length, pText->Buffer); }
	xrtBufferDestroy(pText);
	xvoUnref(pConfig);
	xrtUnit();
	puts("toml C example: PASS");
	return 0;
}

/* 单文件范例由 xl 注入 XRT；正式 package 通过依赖清单编译。 */
#include "../../extlibs/_tomlc17/tomlc17_xrt.c"
#include "../../extlibs/toml/xrt_toml_types.c"
#include "../../extlibs/toml/xrt_toml_parser.c"
#include "../../extlibs/toml/xrt_toml_writer.c"
