#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/toml/include/xrt_toml.h"

#include <stdio.h>
#include <string.h>


static bool dates_match(xvalue* pConfig)
{
	xrttomldatetime tValue;
	if ( pConfig == NULL ) { return false; }
	return xrtTomlGetDateTime(xrtValueObjectGet(pConfig,
		xrtStrView("released")), &tValue) && tValue.iKind == XRT_TOML_DATE &&
		xrtTomlGetDateTime(xrtValueObjectGet(pConfig,
		xrtStrView("window")), &tValue) && tValue.iKind == XRT_TOML_TIME &&
		tValue.iMicrosecond == 123456 &&
		xrtTomlGetDateTime(xrtValueObjectGet(pConfig,
		xrtStrView("updated")), &tValue) && tValue.iKind == XRT_TOML_DATETIME &&
		tValue.iMicrosecond == 250000 &&
		xrtTomlGetDateTime(xrtValueObjectGet(pConfig,
		xrtStrView("review")), &tValue) &&
		tValue.iKind == XRT_TOML_OFFSET_DATETIME &&
		tValue.iTimezoneMinutes == 330;
}


int main(int argc, char **argv)
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
	xvalue* pRoundtrip = NULL;
	xbuffer* pText;
	xstrview tTitle;
	xstrview tHost;
	bool bOk;
	if ( argc != 1 && argc != 2 ) { return 2; }
	pConfig = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u, NULL, &tError);
	if ( pConfig == NULL ) {
		fprintf(stderr, "%s\n", tError.sMessage);
		return 2;
	}
	pServer = xrtValueObjectGet(pConfig, xrtStrView("server"));
	if ( !xrtValueGetString(xrtValueObjectGet(pConfig, xrtStrView("title")), &tTitle) ||
		!xrtValueGetString(xrtValueObjectGet(pServer, xrtStrView("host")), &tHost) ||
		!xrtValueObjectSetNew(pConfig, xrtStrView("released"),
			xrtTomlCreateDate(2026, 7, 16)) ||
		!xrtValueObjectSetNew(pConfig, xrtStrView("window"),
			xrtTomlCreateTime(9, 15, 30, 123456)) ||
		!xrtValueObjectSetNew(pConfig, xrtStrView("updated"),
			xrtTomlCreateDateTime(2026, 7, 16, 9, 45, 30, 250000)) ||
		!xrtValueObjectSetNew(pConfig, xrtStrView("review"),
			xrtTomlCreateOffsetDateTime(2026, 7, 16, 8, 30, 0, 0, 330)) ) {
		xrtValueRelease(pConfig);
		return 3;
	}
	printf("%.*s\n", (int)tTitle.Size, tTitle.Data);
	printf("%.*s\n", (int)tHost.Size, tHost.Data);
	pText = xrtTomlStringify(pConfig, NULL, &tError);
	bOk = pText != NULL;
	if ( pText != NULL ) {
		pRoundtrip = xrtTomlParseMemory(pText->Data, pText->Size, NULL, &tError);
		bOk = dates_match(pRoundtrip);
		printf("%.*s", (int)pText->Size, pText->Data);
		if ( bOk && argc == 2 ) {
			xvalue* pLoaded = NULL;
			xstrview tLoadedTitle;
			bOk = xrtTomlSave(argv[1], pConfig, NULL, &tError);
			if ( bOk ) {
				pLoaded = xrtTomlLoad(argv[1], NULL, &tError);
				bOk = pLoaded != NULL &&
					xrtValueGetString(xrtValueObjectGet(pLoaded, xrtStrView("title")),
						&tLoadedTitle) &&
					tLoadedTitle.Size == tTitle.Size &&
					memcmp(tLoadedTitle.Data, tTitle.Data, tTitle.Size) == 0 &&
					dates_match(pLoaded);
			}
			xrtValueRelease(pLoaded);
		}
	}
	putchar('\n');
	xrtBufferDestroy(pText);
	xrtValueRelease(pRoundtrip);
	xrtValueRelease(pConfig);
	if ( bOk ) puts("toml example: PASS");
	else fputs("toml example: save or read-back failed\n", stderr);
	return bOk ? 0 : 4;
}

/* 单文件范例由 xl 注入 XRT；正式 package 通过依赖清单编译。 */
#include "../../extlibs/_tomlc17/tomlc17_xrt.c"
#include "../../extlibs/toml/xrt_toml_types.c"
#include "../../extlibs/toml/xrt_toml_parser.c"
#include "../../extlibs/toml/xrt_toml_writer.c"
