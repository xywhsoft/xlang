/* 独立 C 测试和包导入使用相同的完整 XRT 类型表。 */
#define XRT_MODULE_ALL
#include "xrt.h"
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
	xvalue* pDocument;
	xvalue* pHosts;
	xvalue* pAuth;
	xbuffer* pText;
	xvalue* pInvalid;
	xrtnetrcoptions tOptions;
	xstrview tLogin;
	int iResult = 1;
	pDocument = xrtNetrcParseMemory(sSource, strlen(sSource), NULL, &tError);
	if ( pDocument == NULL ) {
		fprintf(stderr, "parse failed: %s\n", tError.sMessage);
		goto cleanup;
	}
	pHosts = xrtValueObjectGet(pDocument, xrtStrView("hosts"));
	pAuth = pHosts != NULL ? xrtValueObjectGet(pHosts,
		xrtStrView("api.example.com")) : NULL;
	if ( pAuth == NULL || !xrtValueGetString(xrtValueObjectGet(pAuth,
		xrtStrView("login")), &tLogin) || tLogin.Size != 5u ||
		memcmp(tLogin.Data, "alice", 5u) != 0 ) { goto cleanup; }
	pText = xrtNetrcStringify(pDocument, NULL, &tError);
	if ( pText == NULL || !xrtBufferAppendByte(pText, 0u) ||
		strstr((const char*)pText->Data,
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
		xrtValueRelease(pInvalid);
		goto cleanup;
	}
	xrtNetrcOptionsInit(&tOptions);
	tOptions.tLimits.iMaxNodes = 1u;
	pInvalid = xrtNetrcParseMemory(sSource, strlen(sSource), &tOptions, &tError);
	if ( pInvalid != NULL || tError.iCode != XRT_NETRC_ERROR_LIMIT ) {
		xrtValueRelease(pInvalid);
		goto cleanup;
	}
	{
		const uint8 sInvalidUtf8[] = { 'm', 'a', 'c', 'h', 'i', 'n', 'e', ' ', 0xffu };
		pInvalid = xrtNetrcParseMemory(sInvalidUtf8, sizeof(sInvalidUtf8), NULL, &tError);
		if ( pInvalid != NULL || tError.iCode != XRT_NETRC_ERROR_UTF8 ) {
			xrtValueRelease(pInvalid);
			goto cleanup;
		}
	}
	iResult = 0;
	puts("test_netrc: PASS");

cleanup:
	xrtValueRelease(pDocument);
	return iResult;
}

/*
	测试文件可由 `xl run test_netrc.c` 直接执行。
	正式导入扩展库时，构建器会按照包清单分别编译这些实现文件。
*/
#include "../xrt_netrc_common.c"
#include "../xrt_netrc_parser.c"
#include "../xrt_netrc_writer.c"
