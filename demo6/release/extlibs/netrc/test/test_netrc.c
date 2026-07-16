#include "../include/xrt_netrc.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
	const char* sSource =
		"machine api.example.com login alice password secret account ops\n"
		"default login guest password guest\n"
		"macdef deploy\n echo deploy\n\n";
	xrtnetrcerror tError;
	xvalue pDocument;
	xvalue pHosts;
	xvalue pAuth;
	xbuffer pText;
	xvalue pInvalid;
	xrtnetrcoptions tOptions;
	int iResult = 1;
	xrtInit();
	pDocument = xrtNetrcParseMemory(sSource, strlen(sSource), NULL, &tError);
	if ( pDocument == NULL ) {
		fprintf(stderr, "parse failed: %s\n", tError.sMessage);
		goto cleanup;
	}
	pHosts = xvoTableGetValue(pDocument, "hosts", 5u);
	pAuth = xvoTableGetValue(pHosts, "api.example.com", 15u);
	if ( pAuth == NULL || strcmp((const char*)xvoTableGetText(pAuth,
		"login", 5u), "alice") != 0 ) { goto cleanup; }
	pText = xrtNetrcStringify(pDocument, NULL, &tError);
	if ( pText == NULL || !xrtBufferAppend(pText, "\0", 1u, XBUF_BINARY) ||
		strstr((const char*)pText->Buffer,
		"machine api.example.com") == NULL ) {
		xrtBufferDestroy(pText);
		goto cleanup;
	}
	xrtBufferDestroy(pText);
	pInvalid = xrtNetrcParseMemory(
		"machine duplicate login a\nmachine duplicate login b\n",
		strlen("machine duplicate login a\nmachine duplicate login b\n"),
		NULL, &tError);
	if ( pInvalid != NULL || tError.iCode != XRT_NETRC_ERROR_DUPLICATE ) {
		xvoUnref(pInvalid);
		goto cleanup;
	}
	xrtNetrcOptionsInit(&tOptions);
	tOptions.tLimits.iMaxNodes = 1u;
	pInvalid = xrtNetrcParseMemory(sSource, strlen(sSource), &tOptions, &tError);
	if ( pInvalid != NULL || tError.iCode != XRT_NETRC_ERROR_LIMIT ) {
		xvoUnref(pInvalid);
		goto cleanup;
	}
	{
		const uint8 sInvalidUtf8[] = { 'm', 'a', 'c', 'h', 'i', 'n', 'e', ' ', 0xffu };
		pInvalid = xrtNetrcParseMemory(sInvalidUtf8, sizeof(sInvalidUtf8), NULL, &tError);
		if ( pInvalid != NULL || tError.iCode != XRT_NETRC_ERROR_UTF8 ) {
			xvoUnref(pInvalid);
			goto cleanup;
		}
	}
	iResult = 0;
	puts("test_netrc: PASS");

cleanup:
	xvoUnref(pDocument);
	xrtUnit();
	return iResult;
}

/*
	测试文件可由 `xl run test_netrc.c` 直接执行。
	正式导入扩展库时，构建器会按照包清单分别编译这些实现文件。
*/
#include "../xrt_netrc_common.c"
#include "../xrt_netrc_parser.c"
#include "../xrt_netrc_writer.c"
