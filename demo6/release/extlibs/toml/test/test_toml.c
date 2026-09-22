/* 独立 C 测试和包导入使用相同的完整 XRT 类型表。 */
#define XRT_MODULE_ALL
#include "xrt.h"
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
	xvalue* pRoot = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u, NULL, &tError);
	xvalue* pDate;
	xvalue* pLocal;
	xvalue* pPrecise;
	xvalue* pUsers;
	xbuffer* pEncoded;
	xvalue* pAgain;
	xrttomldatetime tDateTime;
	xstrview tTitle;
	xstrview tName;
	int iKind = 0;
	int bOk = 1;
	bOk &= check(pRoot != NULL && tError.iCode == XRT_TOML_OK,
		tError.sMessage[0] != 0 ? tError.sMessage : "parse failed");
	if ( pRoot == NULL ) { return 0; }
	bOk &= check(xrtValueGetString(xrtValueObjectGet(pRoot, xrtStrView("title")),
		&tTitle) && tTitle.Size == 12u && memcmp(tTitle.Data, "TOML example", 12u) == 0,
		"string value mismatch");
	{
		int64 iCount = 0;
		bOk &= check(xrtValueGetInt(xrtValueObjectGet(pRoot, xrtStrView("count")),
			&iCount) && iCount == 42, "integer value mismatch");
	}
	pDate = xrtValueObjectGet(pRoot, xrtStrView("date"));
	pLocal = xrtValueObjectGet(pRoot, xrtStrView("local"));
	pPrecise = xrtValueObjectGet(pRoot, xrtStrView("precise"));
	bOk &= check(xrtTomlIsDateTimeValue(pDate, &iKind) &&
		iKind == XRT_TOML_DATE, "local date type mismatch");
	bOk &= check(xrtTomlIsDateTimeValue(pLocal, &iKind) &&
		iKind == XRT_TOML_DATETIME &&
		xrtTomlGetDateTime(pLocal, &tDateTime) && tDateTime.iMicrosecond == 123456,
		"local datetime precision mismatch");
	bOk &= check(xrtTomlIsDateTimeValue(xrtValueObjectGet(pRoot,
		xrtStrView("instant")), &iKind) && iKind == XRT_TOML_OFFSET_DATETIME,
		"offset datetime type mismatch");
	bOk &= check(xrtTomlIsDateTimeValue(pPrecise, &iKind) &&
		iKind == XRT_TOML_OFFSET_DATETIME &&
		xrtTomlGetDateTime(pPrecise, &tDateTime) && tDateTime.iMicrosecond == 250000,
		"fractional offset datetime must remain lossless");
	pUsers = xrtValueObjectGet(pRoot, xrtStrView("users"));
	bOk &= check(xrtValueType(pUsers) == XVALUE_ARRAY && xrtValueCount(pUsers) == 2u &&
		xrtValueGetString(xrtValueObjectGet(xrtValueArrayGet(pUsers, 1u),
			xrtStrView("name")), &tName) && tName.Size == 3u &&
		memcmp(tName.Data, "Bob", 3u) == 0, "array-of-tables mismatch");

	pEncoded = xrtTomlStringify(pRoot, NULL, &tError);
	bOk &= check(pEncoded != NULL && strstr((const char*)pEncoded->Data,
		"[[users]]") != NULL, "deterministic encoder omitted array table");
	pAgain = pEncoded != NULL ? xrtTomlParseMemory(pEncoded->Data,
		pEncoded->Size, NULL, &tError) : NULL;
	if ( pAgain == NULL && pEncoded != NULL ) {
		fprintf(stderr, "encoded TOML:\n%.*s\nparse error: %s\n",
			(int)pEncoded->Size, (const char*)pEncoded->Data, tError.sMessage);
	}
	{
		int64 iCount = 0;
		bOk &= check(pAgain != NULL && xrtValueGetInt(xrtValueObjectGet(pAgain,
			xrtStrView("count")), &iCount) && iCount == 42,
			"encoded TOML did not parse back");
	}
	xrtValueRelease(pAgain);
	xrtBufferDestroy(pEncoded);
	xrtValueRelease(pRoot);
	return bOk;
}

static int testErrorsAndLimits(void)
{
	xrttomlerror tError;
	xrttomloptions tOptions;
	xvalue* pValue;
	int bOk = 1;
	pValue = xrtTomlParseMemory("a = [1,", 7u, NULL, &tError);
	bOk &= check(pValue == NULL && tError.iCode == XRT_TOML_ERROR_FORMAT &&
		tError.iLine != 0u, "invalid input must report a typed location error");
	xrtValueRelease(pValue);
	xrtTomlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 4u;
	pValue = xrtTomlParseMemory("value = 1", 9u, &tOptions, &tError);
	bOk &= check(pValue == NULL && tError.iCode == XRT_TOML_ERROR_LIMIT,
		"input limit was not enforced");
	xrtValueRelease(pValue);
	return bOk;
}

int main(void)
{
	int bOk;
	bOk = testParseAndRoundTrip();
	bOk &= testErrorsAndLimits();
	if ( bOk ) { puts("test_toml: PASS"); }
	return bOk ? 0 : 2;
}

/* 单文件测试由 xl 注入 XRT；正式 package 仍按清单独立编译这些实现。 */
#include "../../_tomlc17/tomlc17_xrt.c"
#include "../xrt_toml_types.c"
#include "../xrt_toml_parser.c"
#include "../xrt_toml_writer.c"
