#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/netrc/include/xrt_netrc.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
	const char* sSource =
		"machine api.example.com login agent password token-123\n"
		"default login guest password guest\n";
	xrtnetrcerror tError;
	xvalue* pDocument;
	xvalue* pHosts;
	xvalue* pAuth;
	xbuffer* pText;
	xstrview tLogin;
	pDocument = xrtNetrcParseMemory(sSource, strlen(sSource), NULL, &tError);
	if ( pDocument == NULL ) {
		fprintf(stderr, "%s\n", tError.sMessage);
		return 1;
	}
	pHosts = xrtValueObjectGet(pDocument, xrtStrView("hosts"));
	pAuth = xrtValueObjectGet(pHosts, xrtStrView("api.example.com"));
	if ( !xrtValueGetString(xrtValueObjectGet(pAuth, xrtStrView("login")),
		&tLogin) ) {
		xrtValueRelease(pDocument);
		return 2;
	}
	printf("%.*s@api.example.com\n", (int)tLogin.Size, tLogin.Data);
	pText = xrtNetrcStringify(pDocument, NULL, &tError);
	if ( pText == NULL ) {
		xrtValueRelease(pDocument);
		return 3;
	}
	fwrite(pText->Data, 1u, pText->Size, stdout);
	putchar('\n');
	puts("netrc example: PASS");
	xrtBufferDestroy(pText);
	xrtValueRelease(pDocument);
	return 0;
}

/*
	范例可由 `xl run netrc_c.c` 单文件执行。
	正式导入扩展库时，构建器会按照包清单分别编译这些实现文件。
*/
#include "../../extlibs/netrc/xrt_netrc_common.c"
#include "../../extlibs/netrc/xrt_netrc_parser.c"
#include "../../extlibs/netrc/xrt_netrc_writer.c"
