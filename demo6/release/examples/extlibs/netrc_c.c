#include "../../extlibs/netrc/include/xrt_netrc.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
	const char* sSource =
		"machine api.example.com login agent password token-123\n"
		"default login guest password guest\n";
	xrtnetrcerror tError;
	xvalue pDocument;
	xvalue pHosts;
	xvalue pAuth;
	xbuffer pText;
	xrtInit();
	pDocument = xrtNetrcParseMemory(sSource, strlen(sSource), NULL, &tError);
	if ( pDocument == NULL ) {
		fprintf(stderr, "%s\n", tError.sMessage);
		xrtUnit();
		return 1;
	}
	pHosts = xvoTableGetValue(pDocument, "hosts", 5u);
	pAuth = xvoTableGetValue(pHosts, "api.example.com", 15u);
	printf("%s@api.example.com\n", (const char*)xvoTableGetText(pAuth,
		"login", 5u));
	pText = xrtNetrcStringify(pDocument, NULL, &tError);
	if ( pText == NULL ) {
		xvoUnref(pDocument);
		xrtUnit();
		return 2;
	}
	fwrite(pText->Buffer, 1u, pText->Length, stdout);
	putchar('\n');
	puts("netrc example: PASS");
	xrtBufferDestroy(pText);
	xvoUnref(pDocument);
	xrtUnit();
	return 0;
}

/*
	范例可由 `xl run netrc_c.c` 单文件执行。
	正式导入扩展库时，构建器会按照包清单分别编译这些实现文件。
*/
#include "../../extlibs/netrc/xrt_netrc_common.c"
#include "../../extlibs/netrc/xrt_netrc_parser.c"
#include "../../extlibs/netrc/xrt_netrc_writer.c"
