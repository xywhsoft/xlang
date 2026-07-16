#include "../include/xrt_toml.h"

#include <stdio.h>
#include <string.h>



static int check(int bCondition, const char* sMessage)
{
	if ( bCondition ) { return 1; }
	fprintf(stderr, "test_toml: %s\n", sMessage);
	return 0;
}

static int testParseAndRoundTrip(void)
{
	static const char sSource[] =
		"title = \"TOML example\"\n"
		"enabled = true\n"
		"count = 42\n"
		"ratio = 1.5\n"
		"date = 2026-07-16\n"
		"local = 2026-07-16T12:34:56.123456\n"
		"instant = 2026-07-16T12:34:56+08:00\n"
		"precise = 2026-07-16T12:34:56.25+08:00\n"
		"values = [1, \"two\", false]\n"
		"\n[server]\n"
		"host = \"127.0.0.1\"\n"
		"ports = [8080, 8081]\n"
		"\n[[users]]\nname = \"Alice\"\n"
		"\n[[users]]\nname = \"Bob\"\n";
	xrttomlerror tError;
	xvalue pRoot = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u, NULL, &tError);
	xvalue pDate;
	xvalue pLocal;
	xvalue pPrecise;
	xvalue pUsers;
	xbuffer pEncoded;
	xvalue pAgain;
	int iKind = 0;
	int bOk = 1;
	bOk &= check(pRoot != NULL && tError.iCode == XRT_TOML_OK,
		tError.sMessage[0] != 0 ? tError.sMessage : "parse failed");
	if ( pRoot == NULL ) { return 0; }
	bOk &= check(strcmp((const char*)xvoTableGetText(pRoot, "title", 0u),
		"TOML example") == 0, "string value mismatch");
	bOk &= check(xvoTableGetInt(pRoot, "count", 0u) == 42,
		"integer value mismatch");
	pDate = xvoTableGetValue(pRoot, "date", 0u);
	pLocal = xvoTableGetValue(pRoot, "local", 0u);
	pPrecise = xvoTableGetValue(pRoot, "precise", 0u);
	bOk &= check(xrtTomlIsDateTimeValue(pDate, &iKind) &&
		iKind == XRT_TOML_DATE, "local date type mismatch");
	bOk &= check(xrtTomlIsDateTimeValue(pLocal, &iKind) &&
		iKind == XRT_TOML_DATETIME &&
		xrtTomlGetDateTime(pLocal)->iMicrosecond == 123456,
		"local datetime precision mismatch");
	bOk &= check(xvoType(xvoTableGetValue(pRoot, "instant", 0u)) == XVO_DT_TIME,
		"offset datetime should map to XRT time at second precision");
	bOk &= check(xrtTomlIsDateTimeValue(pPrecise, &iKind) &&
		iKind == XRT_TOML_OFFSET_DATETIME &&
		xrtTomlGetDateTime(pPrecise)->iMicrosecond == 250000,
		"fractional offset datetime must remain lossless");
	pUsers = xvoTableGetValue(pRoot, "users", 0u);
	bOk &= check(xvoIsArray(pUsers) && xvoArrayItemCount(pUsers) == 2u &&
		strcmp((const char*)xvoTableGetText(xvoArrayGetValue(pUsers, 1u),
			"name", 0u), "Bob") == 0, "array-of-tables mismatch");

	pEncoded = xrtTomlStringify(pRoot, NULL, &tError);
	bOk &= check(pEncoded != NULL && strstr((const char*)pEncoded->Buffer,
		"[[users]]") != NULL, "deterministic encoder omitted array table");
	pAgain = pEncoded != NULL ? xrtTomlParseMemory(pEncoded->Buffer,
		pEncoded->Length, NULL, &tError) : NULL;
	if ( pAgain == NULL && pEncoded != NULL ) {
		fprintf(stderr, "encoded TOML:\n%.*s\nparse error: %s\n",
			(int)pEncoded->Length, (const char*)pEncoded->Buffer, tError.sMessage);
	}
	bOk &= check(pAgain != NULL && xvoTableGetInt(pAgain, "count", 0u) == 42,
		"encoded TOML did not parse back");
	xvoUnref(pAgain);
	xrtBufferDestroy(pEncoded);
	xvoUnref(pRoot);
	return bOk;
}

static int testErrorsAndLimits(void)
{
	xrttomlerror tError;
	xrttomloptions tOptions;
	xvalue pValue;
	int bOk = 1;
	pValue = xrtTomlParseMemory("a = [1,", 7u, NULL, &tError);
	bOk &= check(pValue == NULL && tError.iCode == XRT_TOML_ERROR_FORMAT &&
		tError.iLine != 0u, "invalid input must report a typed location error");
	xvoUnref(pValue);
	xrtTomlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 4u;
	pValue = xrtTomlParseMemory("value = 1", 9u, &tOptions, &tError);
	bOk &= check(pValue == NULL && tError.iCode == XRT_TOML_ERROR_LIMIT,
		"input limit was not enforced");
	xvoUnref(pValue);
	return bOk;
}

int main(void)
{
	int bOk;
	if ( xrtInit() == NULL ) { return 1; }
	bOk = testParseAndRoundTrip();
	bOk &= testErrorsAndLimits();
	xrtUnit();
	if ( bOk ) { puts("test_toml: PASS"); }
	return bOk ? 0 : 2;
}

/* 单文件测试由 xl 注入 XRT；正式 package 仍按清单独立编译这些实现。 */
#include "../../_tomlc17/tomlc17_xrt.c"
#include "../xrt_toml_types.c"
#include "../xrt_toml_parser.c"
#include "../xrt_toml_writer.c"
