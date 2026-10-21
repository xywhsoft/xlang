/* 与 release/examples/extlibs/netrc.xl 对照；不打印密码。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_netrc.h"
#include <stdio.h>
#include <string.h>

static bool read_auth(xvalue* pDocument, const char* sHost,
	xstrview* pLogin, xstrview* pPassword)
{
	xvalue* pHosts = pDocument != NULL ?
		xrtValueObjectGet(pDocument, xrtStrView("hosts")) : NULL;
	xvalue* pAuth = pHosts != NULL ?
		xrtValueObjectGet(pHosts, xrtStrView(sHost)) : NULL;
	xvalue* pLoginValue = pAuth != NULL ?
		xrtValueObjectGet(pAuth, xrtStrView("login")) : NULL;
	xvalue* pPasswordValue = pAuth != NULL ?
		xrtValueObjectGet(pAuth, xrtStrView("password")) : NULL;
	return pLoginValue != NULL && pPasswordValue != NULL &&
		xrtValueGetString(pLoginValue, pLogin) &&
		xrtValueGetString(pPasswordValue, pPassword);
}

static bool same_text(xstrview Left, xstrview Right)
{
	return Left.Size == Right.Size && memcmp(Left.Data, Right.Data, Left.Size) == 0;
}

static bool has_deploy_macro(xvalue* pDocument)
{
	xvalue* pMacros = pDocument != NULL ?
		xrtValueObjectGet(pDocument, xrtStrView("macros")) : NULL;
	xvalue* pLines = pMacros != NULL ?
		xrtValueObjectGet(pMacros, xrtStrView("deploy")) : NULL;
	xstrview Line;
	return pMacros != NULL && xrtValueCount(pMacros) == 1u &&
		pLines != NULL && xrtValueCount(pLines) == 1u &&
		xrtValueGetString(xrtValueArrayGet(pLines, 0u), &Line) &&
		same_text(Line, xrtStrView(" echo deploy"));
}

int main(int argc, char** argv)
{
	const char* sSource = "default login guest password fallback\n"
		"machine api.example.com login alice password secret\n"
		"macdef deploy\n echo deploy\n\n";
	xrtnetrcerror Error;
	xvalue* pDocument;
	xvalue* pHosts;
	xvalue* pLoaded = NULL;
	xvalue* pRoundtrip = NULL;
	xbuffer* pSerialized = NULL;
	xstrview Login, Password, FallbackLogin, FallbackPassword;
	xstrview LoadedLogin, LoadedPassword, LoadedFallbackLogin, LoadedFallbackPassword;
	int iStatus = 1;
	if ( argc != 1 && argc != 2 ) return 2;
	pDocument = xrtNetrcParseMemory(sSource, strlen(sSource), NULL, &Error);
	pHosts = pDocument != NULL ? xrtValueObjectGet(pDocument, xrtStrView("hosts")) : NULL;
	if ( pHosts == NULL || xrtValueCount(pHosts) != 2u ||
		!read_auth(pDocument, "api.example.com", &Login, &Password) ||
		!read_auth(pDocument, "default", &FallbackLogin, &FallbackPassword) ||
		!has_deploy_macro(pDocument) ) goto done;
	pSerialized = xrtNetrcStringify(pDocument, NULL, &Error);
	if ( pSerialized == NULL ) goto done;
	pRoundtrip = xrtNetrcParseMemory(pSerialized->Data, pSerialized->Size, NULL, &Error);
	if ( !has_deploy_macro(pRoundtrip) ) goto done;
	{
		xstrview RoundtripLogin, RoundtripPassword;
		if ( !read_auth(pRoundtrip, "api.example.com", &RoundtripLogin,
			&RoundtripPassword) || !same_text(RoundtripLogin, Login) ||
			!same_text(RoundtripPassword, Password) ) goto done;
	}
	if ( argc == 2 ) {
		/* 核对密码往返，但只向控制台输出用户名。 */
		if ( !xrtNetrcSave(argv[1], pDocument, NULL, &Error) ) goto done;
		pLoaded = xrtNetrcLoad(argv[1], NULL, &Error);
		if ( !read_auth(pLoaded, "api.example.com", &LoadedLogin, &LoadedPassword) ||
			!read_auth(pLoaded, "default", &LoadedFallbackLogin, &LoadedFallbackPassword) ||
			!same_text(LoadedLogin, Login) || !same_text(LoadedPassword, Password) ||
			!same_text(LoadedFallbackLogin, FallbackLogin) ||
			!same_text(LoadedFallbackPassword, FallbackPassword) ||
			!has_deploy_macro(pLoaded) ) goto done;
	}
	printf("netrc: %.*s@api.example.com\n", (int)Login.Size, Login.Data);
	printf("netrc fallback: %.*s\n", (int)FallbackLogin.Size, FallbackLogin.Data);
	puts("netrc entries: 2 hosts, 1 macro");
	iStatus = 0;
done:
	if ( iStatus != 0 ) fputs("netrc example: parse or round-trip failed\n", stderr);
	xrtBufferDestroy(pSerialized);
	xrtValueRelease(pRoundtrip);
	xrtValueRelease(pLoaded);
	xrtValueRelease(pDocument);
	return iStatus;
}

#include "../xrt_netrc_common.c"
#include "../xrt_netrc_parser.c"
#include "../xrt_netrc_writer.c"
