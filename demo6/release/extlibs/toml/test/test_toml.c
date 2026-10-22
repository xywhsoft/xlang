/* 独立 C 测试和包导入使用相同的完整 XRT 类型表。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_toml.h"
#include "../../_tomlc17/src/tomlc17.h"

#include <stdio.h>
#include <locale.h>
#include <math.h>
#include <string.h>



static int check(int bCondition, const char* sMessage)
{
	if ( bCondition ) { return 1; }
	fprintf(stderr, "test_toml: %s\n", sMessage);
	return 0;
}

typedef struct {
	size_t Written;
	int Closed;
} toml_close_probe;

static bool tomlProbeWrite(ptr pContext, const void* pData, size_t iSize,
	size_t* pWritten)
{
	toml_close_probe* pProbe = (toml_close_probe*)pContext;
	(void)pData;
	pProbe->Written += iSize;
	*pWritten = iSize;
	return true;
}

static bool tomlProbeMemoryWrite(ptr pContext, const void* pData, size_t iSize,
	size_t* pWritten)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
	*pWritten = 0u;
	xrtSetErrorInfo(XERR_MEMORY, "toml.test", 902, "injected write allocation failure");
	return false;
}

static bool tomlProbeClose(ptr pContext)
{
	toml_close_probe* pProbe = (toml_close_probe*)pContext;
	pProbe->Closed += 1;
	xrtSetErrorInfo(XERR_IO, "toml.test", 901, "injected close failure");
	return false;
}

static bool tomlProbeRead(ptr pContext, ptr pData, size_t iSize,
	size_t* pRead)
{
	static const char sSource[] = "value = 1\n";
	toml_close_probe* pProbe = (toml_close_probe*)pContext;
	size_t iRemain = sizeof(sSource) - 1u - pProbe->Written;
	*pRead = iSize < iRemain ? iSize : iRemain;
	memcpy(pData, sSource + pProbe->Written, *pRead);
	pProbe->Written += *pRead;
	return true;
}

static int testOwnedReaderCloseFailure(void)
{
	xreaderops tOps = {0};
	toml_close_probe tProbe = {0};
	xrttomlerror tError;
	xreader* pReader;
	xvalue* pValue;
	int bOk;
	tOps.Read = tomlProbeRead;
	tOps.Close = tomlProbeClose;
	pReader = xrtReaderCreate(&tOps, &tProbe);
	if ( !check(pReader != NULL, "cannot construct close-failing reader") ) {
		return 0;
	}
	pValue = xrtTomlParse(pReader, true, NULL, &tError);
	bOk = check(pValue == NULL && tError.iCode == XRT_TOML_ERROR_IO &&
		tProbe.Written == sizeof("value = 1\n") - 1u && tProbe.Closed == 1,
		"owned reader close failure was reported as success");
	xrtValueRelease(pValue);
	xrtClearError();
	return bOk;
}

static int testOwnedWriterCloseFailure(void)
{
	static const char sSource[] = "value = 1\n";
	xwriterops tOps = {0};
	toml_close_probe tProbe = {0};
	xrttomlerror tError;
	xvalue* pValue = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		NULL, &tError);
	xwriter* pWriter;
	bool bResult;
	int bOk;
	if ( !check(pValue != NULL, "cannot construct owned-writer close test") ) {
		return 0;
	}
	tOps.Write = tomlProbeWrite;
	tOps.Close = tomlProbeClose;
	pWriter = xrtWriterCreate(&tOps, &tProbe);
	if ( !check(pWriter != NULL, "cannot construct close-failing writer") ) {
		xrtValueRelease(pValue);
		return 0;
	}
	bResult = xrtTomlWrite(pValue, pWriter, true, NULL, &tError);
	bOk = check(!bResult && tError.iCode == XRT_TOML_ERROR_IO &&
		tProbe.Written == sizeof(sSource) - 1u && tProbe.Closed == 1,
		"owned writer close failure was reported as success");
	xrtValueRelease(pValue);
	xrtClearError();
	return bOk;
}

static int testWriterMemoryFailure(void)
{
	static const char sSource[] = "value = 1\n";
	xwriterops tOps = {0};
	xrttomlerror tError;
	xvalue* pValue = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		NULL, &tError);
	xwriter* pWriter;
	int bOk;
	if ( !check(pValue != NULL, "cannot construct memory-failing writer test") ) {
		return 0;
	}
	tOps.Write = tomlProbeMemoryWrite;
	pWriter = xrtWriterCreate(&tOps, NULL);
	if ( !check(pWriter != NULL, "cannot construct memory-failing writer") ) {
		xrtValueRelease(pValue);
		return 0;
	}
	bOk = check(!xrtTomlWrite(pValue, pWriter, false, NULL, &tError) &&
		tError.iCode == XRT_TOML_ERROR_MEMORY,
		"writer allocation failure was reported as IO");
	xrtWriterDestroy(pWriter);
	xrtSetErrorInfo(XERR_MEMORY, "toml.test", 903, "stale memory error");
	bOk &= check(!xrtTomlWrite(pValue, NULL, false, NULL, &tError) &&
		tError.iCode == XRT_TOML_ERROR_IO,
		"stale memory error changed the write failure category");
	xrtValueRelease(pValue);
	xrtClearError();
	return bOk;
}

static int testBorrowedStreamsStayOpen(void)
{
	static const char sSource[] = "value = 1\n";
	xreaderops tReaderOps = {0};
	xwriterops tWriterOps = {0};
	toml_close_probe tReadProbe = {0}, tWriteProbe = {0};
	xrttomlerror tError;
	xreader* pReader;
	xwriter* pWriter;
	xvalue* pValue;
	int bOk = 1;
	tReaderOps.Read = tomlProbeRead;
	tReaderOps.Close = tomlProbeClose;
	pReader = xrtReaderCreate(&tReaderOps, &tReadProbe);
	if ( !check(pReader != NULL, "cannot construct borrowed reader") ) { return 0; }
	pValue = xrtTomlParse(pReader, false, NULL, &tError);
	bOk &= check(pValue != NULL && tError.iCode == XRT_TOML_OK &&
		tReadProbe.Closed == 0, "TOML parser closed a borrowed reader");
	bOk &= check(!xrtReaderDestroy(pReader) && tReadProbe.Closed == 1,
		"borrowed reader was not left to its caller");
	xrtClearError();
	tWriterOps.Write = tomlProbeWrite;
	tWriterOps.Close = tomlProbeClose;
	pWriter = xrtWriterCreate(&tWriterOps, &tWriteProbe);
	if ( !check(pWriter != NULL, "cannot construct borrowed writer") ) {
		xrtValueRelease(pValue);
		return 0;
	}
	bOk &= check(xrtTomlWrite(pValue, pWriter, false, NULL, &tError) &&
		tError.iCode == XRT_TOML_OK && tWriteProbe.Closed == 0 &&
		tWriteProbe.Written == sizeof(sSource) - 1u,
		"TOML writer closed a borrowed stream");
	bOk &= check(!xrtWriterDestroy(pWriter) && tWriteProbe.Closed == 1,
		"borrowed writer was not left to its caller");
	xrtValueRelease(pValue);
	xrtClearError();
	return bOk;
}

static int testParserAllocator(void)
{
	toml_option_t Options = toml_default_option();
	toml_result_t Result;
	bool bInjected;
	if ( !check(Options.mem_realloc == xrtRealloc &&
		Options.mem_free == xrtFree,
		"tomlc17 default allocator is not XRT") ||
		!check(xrtMemDebugEnabled() || xrtMemDebugEnable(true),
		"cannot enable XRT memory debugger") ||
		!check(xrtMemDebugFailAfter(0),
		"cannot inject tomlc17 allocation failure") ) { return 0; }
	Result = toml_parse("value = \"text\"\n", 15);
	bInjected = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	toml_free(Result);
	xrtClearError();
	return check(bInjected && !Result.ok,
		"tomlc17 parser bypasses XRT allocation faults");
}

/* 日期字段写入失败时，SetTake 未接管的临时值不能遗留。 */
static int testDateAllocationOwnership(void)
{
	xmemdebugsnapshot tBefore;
	xmemdebugsnapshot tAfter;
	uint64 iPoint;
	int bSawFailure = 0;
	int bSawSuccess = 0;
	for ( iPoint = 0u; iPoint < 80u; iPoint++ ) {
		xvalue* pDate;
		bool bTriggered;
		xrtClearError();
		xrtMemDebugSnapshot(&tBefore);
		if ( !xrtMemDebugFailAfter(iPoint) ) { return 0; }
		pDate = xrtTomlCreateOffsetDateTime(2026, 10, 22, 8, 9, 10,
			123456, 330);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) {
			bSawFailure = 1;
			if ( pDate != NULL || xrtErrorKind(xrtGetError()) != XERR_MEMORY ) {
				xrtValueRelease(pDate); return 0;
			}
		} else {
			xrttomldatetime tDate;
			bSawSuccess = 1;
			if ( pDate == NULL || !xrtTomlGetDateTime(pDate, &tDate) ||
				tDate.iKind != XRT_TOML_OFFSET_DATETIME ) {
				xrtValueRelease(pDate); return 0;
			}
		}
		xrtValueRelease(pDate);
		xrtClearError();
		xrtMemDebugSnapshot(&tAfter);
		if ( tAfter.LiveCount != tBefore.LiveCount ||
			tAfter.LiveBytes != tBefore.LiveBytes ) {
			fprintf(stderr, "TOML date allocation point %llu leaked: %zu -> %zu\n",
				(unsigned long long)iPoint, tBefore.LiveCount, tAfter.LiveCount);
			return 0;
		}
	}
	return bSawFailure && bSawSuccess;
}

static int testParseAllocationFailures(void)
{
	static const char sSource[] = "value = \"text\"\n";
	int iInjected = 0;
	for ( int i = 0; i < 32; ++i ) {
		xrttomlerror tError;
		xvalue* pValue;
		bool bTriggered;
		xrtClearError();
		if ( !check(xrtMemDebugFailAfter((uint64)i),
			"cannot inject TOML parse allocation failure") ) { return 0; }
		pValue = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
			NULL, &tError);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) {
			iInjected++;
			if ( !check(pValue == NULL &&
				tError.iCode == XRT_TOML_ERROR_MEMORY,
				"TOML allocation failure was not reported as MemoryError") ) {
				xrtValueRelease(pValue);
				return 0;
			}
		} else if ( !check(pValue != NULL && tError.iCode == XRT_TOML_OK,
			"TOML parse failed without allocation injection") ) {
			xrtValueRelease(pValue);
			return 0;
		}
		xrtValueRelease(pValue);
	}
	xrtClearError();
	if ( !check(iInjected >= 2, "TOML parser allocations were not exercised") ) {
		return 0;
	}
	{
		xrttomlerror tError;
		xvalue* pValue;
		xrtSetErrorInfo(XERR_MEMORY, "toml.test", 1, "previous failure");
		pValue = xrtTomlParseMemory("value = [\n", 10u, NULL, &tError);
		if ( !check(pValue == NULL && tError.iCode == XRT_TOML_ERROR_FORMAT,
			"previous memory error contaminated TOML format diagnosis") ) {
			xrtValueRelease(pValue);
			xrtClearError();
			return 0;
		}
		xrtValueRelease(pValue);
		xrtClearError();
	}
	return 1;
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

	xrtClearError();
	pEncoded = xrtTomlStringify(pRoot, NULL, &tError);
	bOk &= check(pEncoded != NULL && strstr((const char*)pEncoded->Data,
		"[[users]]") != NULL, "deterministic encoder omitted array table");
	bOk &= check(xrtGetError() == NULL,
		"successful stringify left a stale XRT error");
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

static int testExcessFraction(void)
{
	static const char sSource[] =
		"local = 12:34:56.123456999\n"
		"datetime = 2026-07-16T12:34:56.9999999\n"
		"offset = 2026-07-16T12:34:56.123456789+08:00\n";
	xrttomlerror tError;
	xrttomldatetime tTime;
	xvalue* pRoot = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		NULL, &tError);
	xbuffer* pOutput = NULL;
	int bOk = check(pRoot != NULL && tError.iCode == XRT_TOML_OK,
		"cannot parse fractional seconds");
	if ( pRoot != NULL ) {
		/* TOML 要求超出实现精度的数字截断，不能进位。 */
		bOk &= check(xrtTomlGetDateTime(xrtValueObjectGet(pRoot,
			xrtStrView("local")), &tTime) && tTime.iMicrosecond == 123456,
			"local time fraction was rounded or lost");
		bOk &= check(xrtTomlGetDateTime(xrtValueObjectGet(pRoot,
			xrtStrView("datetime")), &tTime) && tTime.iMicrosecond == 999999,
			"local datetime fraction was rounded");
		bOk &= check(xrtTomlGetDateTime(xrtValueObjectGet(pRoot,
			xrtStrView("offset")), &tTime) && tTime.iMicrosecond == 123456,
			"offset datetime fraction was rounded or lost");
		pOutput = xrtTomlStringify(pRoot, NULL, &tError);
		bOk &= check(pOutput != NULL &&
			strstr((const char*)pOutput->Data,
				"local = 12:34:56.123456\n") != NULL &&
			strstr((const char*)pOutput->Data,
				"datetime = 2026-07-16T12:34:56.999999\n") != NULL,
			"writer did not expose the documented microsecond precision");
	}
	xrtBufferDestroy(pOutput);
	xrtValueRelease(pRoot);
	return bOk;
}

static int testErrorsAndLimits(void)
{
	xrttomlerror tError;
	xrttomloptions tOptions;
	xvalue* pValue;
	xvalue* pAgain;
	xbuffer* pOutput;
	int bOk = 1;
	const char* sDuplicates[] = {
		"a = 1\na = 2\n",
		"a = 1\n[a]\nx = 2\n",
		"[a]\nx = 1\n[a]\ny = 2\n",
		"a = { b = 1 }\na.c = 2\n",
		"a.b = 1\na = { c = 2 }\n",
		"a = { b = 1 }\n[a.c]\nd = 2\n",
		"a.b = 1\n[a]\nc = 2\n",
		"[a]\nb.c = 1\n[a.b]\nd = 2\n",
		"fruits = []\n[[fruits]]\n",
		"[[fruits]]\n[fruits]\n",
		"[fruit.physical]\n[[fruit]]\n"
	};
	const char* sOutsideVersion10[] = {
		"a = { b = 1, }\n",
		"a = { b = 1,\n c = 2 }\n",
		"a = 07:32\n",
		"a = 1979-05-27T07:32\n",
		"中文 = 1\n",
		"a = \"\\x33\"\n",
		"a = \"\\e\"\n"
	};
	size_t i;
	/* 重复声明或扩写内联表都不能交出部分配置。 */
	for ( i = 0u; i < sizeof(sDuplicates) / sizeof(sDuplicates[0]); ++i ) {
		pValue = xrtTomlParseMemory(sDuplicates[i], strlen(sDuplicates[i]),
			NULL, &tError);
		bOk &= check(pValue == NULL &&
			tError.iCode == XRT_TOML_ERROR_FORMAT && tError.iLine > 0u,
			"duplicate TOML key or table was accepted");
		xrtValueRelease(pValue);
	}
	/* 隐式父表尚未显式声明时，允许在子表之后补上父表。 */
	pValue = xrtTomlParseMemory("[x.y.z]\n[x]\nname = 1\n",
		sizeof("[x.y.z]\n[x]\nname = 1\n") - 1u,
		NULL, &tError);
	bOk &= check(pValue != NULL && tError.iCode == XRT_TOML_OK,
		"out-of-order super-table was rejected");
	xrtValueRelease(pValue);
	/* 包以 TOML 1.0 为目标：拒绝私有解析器超出该版本的宽松写法。 */
	for ( i = 0u; i < sizeof(sOutsideVersion10) / sizeof(sOutsideVersion10[0]); ++i ) {
		pValue = xrtTomlParseMemory(sOutsideVersion10[i], strlen(sOutsideVersion10[i]),
			NULL, &tError);
		bOk &= check(pValue == NULL &&
			tError.iCode == XRT_TOML_ERROR_FORMAT && tError.iLine > 0u,
			"syntax outside TOML 1.0 was accepted");
		xrtValueRelease(pValue);
	}
	pValue = xrtTomlParseMemory("\"中文\" = 1\n", sizeof("\"中文\" = 1\n") - 1u,
		NULL, &tError);
	bOk &= check(pValue != NULL && tError.iCode == XRT_TOML_OK &&
		xrtValueObjectGet(pValue, xrtStrView("中文")) != NULL,
		"quoted Unicode key must remain valid TOML 1.0");
	pOutput = pValue != NULL ? xrtTomlStringify(pValue, NULL, &tError) : NULL;
	pAgain = pOutput != NULL ? xrtTomlParseMemory(pOutput->Data,
		pOutput->Size, NULL, &tError) : NULL;
	bOk &= check(pOutput != NULL && pOutput->Size > 0u &&
		pOutput->Data[0] == '"' && pAgain != NULL &&
		xrtValueObjectGet(pAgain, xrtStrView("中文")) != NULL,
		"Unicode key writer must produce readable quoted TOML 1.0");
	xrtValueRelease(pAgain);
	xrtBufferDestroy(pOutput);
	xrtValueRelease(pValue);
	pValue = xrtTomlParseMemory("a = [1,", 7u, NULL, &tError);
	bOk &= check(pValue == NULL && tError.iCode == XRT_TOML_ERROR_FORMAT &&
		tError.iLine != 0u, "invalid input must report a typed location error");
	xrtValueRelease(pValue);
	{
		static const char sInvalidUtf8[] = "a = 1\r\nb = \"\xff\"\n";
		pValue = xrtTomlParseMemory(sInvalidUtf8,
			sizeof(sInvalidUtf8) - 1u, NULL, &tError);
		bOk &= check(pValue == NULL &&
			tError.iCode == XRT_TOML_ERROR_FORMAT &&
			tError.iLine == 2u && tError.iColumn == 6u &&
			tError.iOffset == 12u,
			"invalid UTF-8 must report its byte location");
		xrtValueRelease(pValue);
	}
	xrtTomlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 4u;
	pValue = xrtTomlParseMemory("value = 1", 9u, &tOptions, &tError);
	bOk &= check(pValue == NULL && tError.iCode == XRT_TOML_ERROR_LIMIT,
		"input limit was not enforced");
	xrtValueRelease(pValue);
	pValue = xrtTomlLoad("toml-missing-parent-019ff64d/config.toml", NULL,
		&tError);
	bOk &= check(pValue == NULL && tError.iCode == XRT_TOML_ERROR_IO,
		"missing TOML file must report an I/O error");
	xrtValueRelease(pValue);
	return bOk;
}

static int testDateTimeProbe(void)
{
	xvalue* pPlain = xrtValueObject();
	xvalue* pDate;
	int iKind = -1;
	int bOk = 1;
	if ( pPlain == NULL ) { return check(0, "cannot create plain TOML object"); }
	xrtClearError();
	bOk &= check(!xrtTomlIsDateTimeValue(pPlain, &iKind) && iKind == 0 &&
		xrtGetError() == NULL,
		"plain object date probe must not set an XRT error");
	pDate = xrtTomlCreateDate(2026, 10, 14);
	bOk &= check(pDate != NULL && xrtTomlIsDateTimeValue(pDate, &iKind) &&
		iKind == XRT_TOML_DATE && xrtGetError() == NULL,
		"valid date probe must not retain an XRT error");
	if ( pDate != NULL ) {
		xvalue* pCopy = xrtValueDeepClone(pDate);
		bOk &= check(pCopy != NULL && xrtTomlIsDateTimeValue(pCopy, NULL),
			"deep-cloned date lost its type identity");
		xrtValueRelease(pCopy);
	}
	xrtValueRelease(pDate);
	xrtValueRelease(pPlain);
	return bOk;
}

static int testDateShapeCollision(void)
{
	static const char sSource[] =
		"[record]\n"
		"__toml_type = \"date\"\n"
		"year = 2026\nmonth = 10\nday = 14\n"
		"hour = 0\nminute = 0\nsecond = 0\n"
		"microsecond = 0\noffsetMinutes = 0\n";
	xrttomlerror tError;
	xvalue* pRoot = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		NULL, &tError);
	xvalue* pRecord = pRoot != NULL ? xrtValueObjectGet(pRoot,
		xrtStrView("record")) : NULL;
	xbuffer* pOutput = NULL;
	xvalue* pAgain = NULL;
	int bOk = check(pRecord != NULL,
		"cannot parse ordinary table with date-shaped fields");
	if ( bOk ) {
		bOk &= check(!xrtTomlIsDateTimeValue(pRecord, NULL),
			"ordinary TOML table was mistaken for a date");
		pOutput = xrtTomlStringify(pRoot, NULL, &tError);
		bOk &= check(pOutput != NULL,
			"cannot write ordinary table with date-shaped fields");
		if ( pOutput != NULL ) {
			pAgain = xrtTomlParseMemory(pOutput->Data, pOutput->Size,
				NULL, &tError);
			pRecord = pAgain != NULL ? xrtValueObjectGet(pAgain,
				xrtStrView("record")) : NULL;
			bOk &= check(pRecord != NULL &&
				!xrtTomlIsDateTimeValue(pRecord, NULL),
				"date-shaped table changed type after write/read");
		}
	}
	xrtValueRelease(pAgain);
	xrtBufferDestroy(pOutput);
	xrtValueRelease(pRoot);
	return bOk;
}

static int testDateTimeValidation(void)
{
	xrttomldatetime DateTime = { 2026, 2, 28, 0, 0, 0, 0, 0,
		XRT_TOML_DATE, 0 };
	xvalue* pDate = xrtTomlCreateDate(2026, 2, 28);
	xvalue* pHour = xrtValueInt(1);
	str sText;
	int bOk = 1;
	if ( pDate == NULL || pHour == NULL ) {
		xrtValueRelease(pDate);
		xrtValueRelease(pHour);
		return check(0, "cannot construct TOML validation test");
	}
	// 原始 C 记录和可变 xvalue 对象必须遵守同一日期契约。
	DateTime.iDay = 30;
	sText = xrtTomlDateTimeToString(&DateTime);
	bOk &= check(sText == NULL, "invalid raw TOML date was formatted");
	xrtFree(sText);
	DateTime.iDay = 28;
	DateTime.iHour = 1;
	sText = xrtTomlDateTimeToString(&DateTime);
	bOk &= check(sText == NULL, "date with hidden time fields was formatted");
	xrtFree(sText);
	DateTime.iHour = 0;
	DateTime.iReserved = 1;
	sText = xrtTomlDateTimeToString(&DateTime);
	bOk &= check(sText == NULL, "date with reserved bits was formatted");
	xrtFree(sText);
	bOk &= check(xrtValueObjectSetTake(pDate, xrtStrView("hour"), &pHour) &&
		!xrtTomlIsDateTimeValue(pDate, NULL),
		"date object with hidden time fields was accepted");
	xrtValueRelease(pHour);
	xrtValueRelease(pDate);
	return bOk;
}

typedef struct {
	const char* Text;
	size_t Size;
	size_t Position;
	xerrkind FailKind;
} toml_test_reader;

static bool testReadUnknownSize(ptr pContext, ptr pBuffer, size_t iRequest,
	size_t* pRead)
{
	toml_test_reader* pState = (toml_test_reader*)pContext;
	if ( pState->FailKind != XERR_NONE ) {
		xrtSetErrorInfo(pState->FailKind, "toml.test", 1,
			"injected reader failure");
		return false;
	}
	size_t iRemain = pState->Size - pState->Position;
	*pRead = iRequest < iRemain ? iRequest : iRemain;
	memcpy(pBuffer, pState->Text + pState->Position, *pRead);
	pState->Position += *pRead;
	return true;
}

static int testUnknownSizeReader(void)
{
	static const char sSource[] = "value = 1\n";
	toml_test_reader tState = { sSource, sizeof(sSource) - 1u, 0u, XERR_NONE };
	xreaderops tOps = { 0 };
	xrttomloptions tOptions;
	xrttomlerror tError;
	xreader* pReader;
	xvalue* pValue;
	int bOk = 1;
	tOps.Read = testReadUnknownSize;
	xrtTomlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 4u;
	pReader = xrtReaderCreate(&tOps, &tState);
	pValue = xrtTomlParse(pReader, true, &tOptions, &tError);
	bOk &= check(pValue == NULL && tError.iCode == XRT_TOML_ERROR_LIMIT,
		"unknown-size reader limit must report a range error");
	xrtValueRelease(pValue);
	xrtClearError();
	tState.Position = 0u;
	pReader = xrtReaderCreate(&tOps, &tState);
	pValue = xrtTomlParse(pReader, true, NULL, &tError);
	bOk &= check(pValue != NULL && tError.iCode == XRT_TOML_OK &&
		xrtGetError() == NULL, "unknown-size reader must parse without a stale error");
	xrtValueRelease(pValue);
	for ( int i = 0; i < 2; ++i ) {
		tState.Position = 0u;
		tState.FailKind = i == 0 ? XERR_IO : XERR_MEMORY;
		pReader = xrtReaderCreate(&tOps, &tState);
		pValue = xrtTomlParse(pReader, true, NULL, &tError);
		bOk &= check(pValue == NULL && tError.iCode ==
			(i == 0 ? XRT_TOML_ERROR_IO : XRT_TOML_ERROR_MEMORY),
			"unknown-size reader failure was misclassified");
		xrtValueRelease(pValue);
		xrtClearError();
	}
	return bOk;
}

typedef struct {
	int Reads;
	int Sizes;
	int Tells;
	int Closes;
} toml_limit_probe;

static bool tomlLimitRead(ptr pContext, ptr pData, size_t iSize, size_t* pRead)
{
	toml_limit_probe* pProbe = (toml_limit_probe*)pContext;
	(void)pData;
	(void)iSize;
	(void)pRead;
	pProbe->Reads++;
	xrtSetErrorInfo(XERR_IO, "toml.test", 902, "oversized reader was read");
	return false;
}

static bool tomlLimitSize(ptr pContext, uint64* pSize)
{
	toml_limit_probe* pProbe = (toml_limit_probe*)pContext;
	pProbe->Sizes++;
	*pSize = 10u;
	return true;
}

static bool tomlLimitTell(ptr pContext, uint64* pPosition)
{
	toml_limit_probe* pProbe = (toml_limit_probe*)pContext;
	pProbe->Tells++;
	*pPosition = 0u;
	return true;
}

static bool tomlLimitClose(ptr pContext)
{
	toml_limit_probe* pProbe = (toml_limit_probe*)pContext;
	pProbe->Closes++;
	return true;
}

/* 已知输入超限时不能先读正文；拥有型 Reader 仍须关闭。 */
static int testKnownSizeLimitBeforeRead(void)
{
	xreaderops tOps = {0};
	toml_limit_probe tProbe = {0};
	xrttomloptions tOptions;
	xrttomlerror tError;
	xreader* pReader;
	xvalue* pValue;
	int bOk;
	tOps.Read = tomlLimitRead;
	tOps.Size = tomlLimitSize;
	tOps.Tell = tomlLimitTell;
	tOps.Close = tomlLimitClose;
	xrtTomlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 4u;
	pReader = xrtReaderCreate(&tOps, &tProbe);
	if ( !check(pReader != NULL, "cannot construct known-size reader") ) { return 0; }
	pValue = xrtTomlParse(pReader, true, &tOptions, &tError);
	bOk = check(pValue == NULL && tError.iCode == XRT_TOML_ERROR_LIMIT &&
		tProbe.Reads == 0 && tProbe.Sizes == 1 && tProbe.Tells == 1 &&
		tProbe.Closes == 1,
		"known-size TOML limit was not checked before reading");
	xrtValueRelease(pValue);
	xrtClearError();
	return bOk;
}

static int testKnownSizeReaderPosition(void)
{
	static const char sSource[] = "skip\nvalue = 1\n";
	xrttomloptions tOptions;
	xrttomlerror tError;
	xreader* pReader = xrtReaderFromMemory((xbytesview){
		(cbytes)sSource, sizeof(sSource) - 1u });
	xvalue* pValue;
	int bOk;
	if ( !check(pReader != NULL &&
		xrtReaderSeek(pReader, sizeof("skip\n") - 1u,
			XSEEK_START, NULL), "cannot position TOML reader") ) {
		xrtReaderDestroy(pReader);
		return 0;
	}
	xrtTomlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = sizeof("value = 1\n") - 1u;
	pValue = xrtTomlParse(pReader, true, &tOptions, &tError);
	bOk = check(pValue != NULL && tError.iCode == XRT_TOML_OK,
		"TOML input limit incorrectly counted bytes before Reader position");
	xrtValueRelease(pValue);
	return bOk;
}

static int testEmbeddedNulInput(void)
{
	static const char* const Cases[] = {
		"value = 1\n\0hidden = 2\n",
		"value = 1\n# note\0hidden = 2\n"
	};
	static const size_t Sizes[] = {
		sizeof("value = 1\n\0hidden = 2\n") - 1u,
		sizeof("value = 1\n# note\0hidden = 2\n") - 1u
	};
	int bOk = 1;
	for ( size_t i = 0; i < sizeof(Cases) / sizeof(Cases[0]); ++i ) {
		xrttomlerror tError;
		xvalue* pValue = xrtTomlParseMemory(Cases[i], Sizes[i], NULL, &tError);
		bOk &= check(pValue == NULL && tError.iCode == XRT_TOML_ERROR_FORMAT,
			"embedded NUL was accepted as a valid TOML prefix");
		xrtValueRelease(pValue);
	}
	return bOk;
}

static int testWriterRejectsInvalidUtf8(void)
{
	const char sInvalid[] = { (char)0xff };
	xrttomlerror tError;
	xvalue* pRoot = xrtValueObject();
	xvalue* pText = xrtValueString((xstrview){ sInvalid, sizeof(sInvalid) });
	xbuffer* pOutput;
	int bOk;
	if ( pRoot == NULL || pText == NULL ||
		!xrtValueObjectSetTake(pRoot, xrtStrView("name"), &pText) ) {
		xrtValueRelease(pText);
		xrtValueRelease(pRoot);
		return check(0, "cannot construct invalid UTF-8 writer test");
	}
	pOutput = xrtTomlStringify(pRoot, NULL, &tError);
	bOk = check(pOutput == NULL && tError.iCode == XRT_TOML_ERROR_FORMAT,
		"writer accepted invalid UTF-8 string value");
	xrtBufferDestroy(pOutput);
	xrtValueRelease(pRoot);
	xrtClearError();
	pRoot = xrtValueObject();
	pText = xrtValueInt(1);
	if ( pRoot == NULL || pText == NULL ||
		!xrtValueObjectSetTake(pRoot,
			(xstrview){ sInvalid, sizeof(sInvalid) }, &pText) ) {
		xrtValueRelease(pText);
		xrtValueRelease(pRoot);
		return check(0, "cannot construct invalid UTF-8 key test");
	}
	pOutput = xrtTomlStringify(pRoot, NULL, &tError);
	bOk &= check(pOutput == NULL && tError.iCode == XRT_TOML_ERROR_FORMAT,
		"writer accepted invalid UTF-8 key");
	xrtBufferDestroy(pOutput);
	xrtValueRelease(pRoot);
	return bOk;
}

static int testSave(void)
{
	const char sInvalid[] = { (char)0xff };
	xrttomlerror tError;
	str sPath = NULL;
	xfile File = xrtFileTemp(NULL, "xlang-toml-", ".toml", &sPath);
	xvalue* pRoot = xrtValueObject();
	xvalue* pText = xrtValueString(xrtStrView("before"));
	xvalue* pLoaded = NULL;
	xstrview Text;
	int bOk = 1;
	if ( File == NULL || sPath == NULL || pRoot == NULL || pText == NULL ) {
		bOk = check(0, "cannot create TOML save test");
		goto cleanup;
	}
	if ( !xrtClose(File) ) { File = NULL; bOk = 0; goto cleanup; }
	File = NULL;
	if ( !xrtValueObjectSetTake(pRoot, xrtStrView("name"), &pText) ||
		!xrtTomlSave(sPath, pRoot, NULL, &tError) ) {
		bOk = check(0, "cannot save valid TOML document");
		goto cleanup;
	}
	for ( int i = 0; i < 32; ++i ) {
		bool bSaved, bInjected;
		if ( !check(xrtMemDebugFailAfter((uint64)i),
			"cannot inject TOML save allocation failure") ) {
			bOk = 0;
			goto cleanup;
		}
		bSaved = xrtTomlSave(sPath, pRoot, NULL, &tError);
		bInjected = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		bOk &= check(bInjected ? (!bSaved &&
			tError.iCode == XRT_TOML_ERROR_MEMORY) :
			(bSaved && tError.iCode == XRT_TOML_OK),
			"TOML save misclassified allocation failure");
		xrtClearError();
		if ( !bOk ) goto cleanup;
	}
	pText = xrtValueString((xstrview){ sInvalid, sizeof(sInvalid) });
	if ( pText == NULL ||
		!xrtValueObjectSetTake(pRoot, xrtStrView("name"), &pText) ) {
		bOk = check(0, "cannot create invalid TOML save test");
		goto cleanup;
	}
	bOk &= check(!xrtTomlSave(sPath, pRoot, NULL, &tError) &&
		tError.iCode == XRT_TOML_ERROR_FORMAT,
		"invalid document save was accepted");
	pLoaded = xrtTomlLoad(sPath, NULL, &tError);
	bOk &= check(pLoaded != NULL &&
		xrtValueGetString(xrtValueObjectGet(pLoaded, xrtStrView("name")), &Text) &&
		Text.Size == 6u && memcmp(Text.Data, "before", 6u) == 0,
		"failed save replaced the previous TOML document");
cleanup:
	if ( File != NULL ) { xrtClose(File); }
	if ( sPath != NULL ) { xrtFileDelete(sPath); }
	xrtFree(sPath);
	xrtValueRelease(pLoaded);
	xrtValueRelease(pText);
	xrtValueRelease(pRoot);
	return bOk;
}

static int testWriterValidUtf8(void)
{
	xrttomlerror tError;
	xvalue* pRoot = xrtValueObject();
	xvalue* pText = xrtValueString(xrtStrView("北京"));
	xvalue* pEmpty = xrtValueInt(7);
	xvalue* pAgain = NULL;
	xbuffer* pOutput = NULL;
	xstrview Text;
	int64 iNumber = 0;
	int bOk = 1;
	if ( pRoot == NULL || pText == NULL || pEmpty == NULL ||
		!xrtValueObjectSetTake(pRoot, xrtStrView("城市"), &pText) ||
		!xrtValueObjectSetTake(pRoot, (xstrview){ NULL, 0u }, &pEmpty) ) {
		bOk = check(0, "cannot construct UTF-8 writer test");
		goto cleanup;
	}
	pOutput = xrtTomlStringify(pRoot, NULL, &tError);
	if ( pOutput != NULL ) {
		pAgain = xrtTomlParseMemory(pOutput->Data, pOutput->Size, NULL, &tError);
	}
	bOk &= check(pAgain != NULL &&
		xrtValueGetString(xrtValueObjectGet(pAgain, xrtStrView("城市")), &Text) &&
		Text.Size == strlen("北京") && memcmp(Text.Data, "北京", Text.Size) == 0 &&
		xrtValueGetInt(xrtValueObjectGet(pAgain, (xstrview){ NULL, 0u }),
			&iNumber) && iNumber == 7,
		"valid UTF-8 and empty TOML keys failed to round-trip");
cleanup:
	xrtValueRelease(pAgain);
	xrtBufferDestroy(pOutput);
	xrtValueRelease(pText);
	xrtValueRelease(pEmpty);
	xrtValueRelease(pRoot);
	return bOk;
}

static int testWriterDecimalLocale(void)
{
	static const char sNumbers[] =
		"small = 1e-12\nminusZero = -0.0\npositiveInf = +inf\nnotNumber = nan\n";
	static const char* arrLocales[] = {
		"German_Germany.1252", "French_France.1252",
		"de_DE.UTF-8", "fr_FR.UTF-8"
	};
	const char* sCurrent = setlocale(LC_NUMERIC, NULL);
	char sPrevious[128];
	xrttomlerror tError;
	xvalue* pRoot = NULL;
	xvalue* pRatio = NULL;
	xvalue* pAgain = NULL;
	xbuffer* pOutput = NULL;
	double fValue = 0.0;
	int bOk = 1;
	int bChanged = 0;
	if ( sCurrent == NULL || strlen(sCurrent) >= sizeof(sPrevious) ) {
		return check(0, "cannot preserve numeric locale");
	}
	strcpy(sPrevious, sCurrent);
	for ( size_t i = 0; i < sizeof(arrLocales) / sizeof(arrLocales[0]); ++i ) {
		if ( setlocale(LC_NUMERIC, arrLocales[i]) != NULL ) {
			bChanged = 1;
			break;
		}
	}
	pRoot = xrtValueObject();
	pRatio = xrtValueFloat(1.5);
	if ( pRoot == NULL || pRatio == NULL ||
		!xrtValueObjectSetTake(pRoot, xrtStrView("ratio"), &pRatio) ) {
		bOk = check(0, "cannot construct decimal-locale writer test");
		goto cleanup;
	}
	pRatio = xrtValueFloat(1.0);
	if ( pRatio == NULL ||
		!xrtValueObjectSetTake(pRoot, xrtStrView("whole"), &pRatio) ) {
		bOk = check(0, "cannot construct whole-float writer test");
		goto cleanup;
	}
	pOutput = xrtTomlStringify(pRoot, NULL, &tError);
	bOk &= check(pOutput != NULL &&
		strstr((const char*)pOutput->Data, "ratio = 1.5\n") != NULL &&
		strstr((const char*)pOutput->Data, "whole = 1.0\n") != NULL,
		"TOML writer emitted a locale-dependent decimal separator");
	if ( pOutput != NULL ) {
		pAgain = xrtTomlParseMemory(pOutput->Data, pOutput->Size, NULL, &tError);
	}
	bOk &= check(pAgain != NULL &&
		xrtValueGetFloat(xrtValueObjectGet(pAgain, xrtStrView("ratio")), &fValue) &&
		fValue == 1.5 &&
		xrtValueGetFloat(xrtValueObjectGet(pAgain, xrtStrView("whole")), &fValue) &&
		fValue == 1.0, "TOML decimal did not parse back as float");
	xrtValueRelease(pAgain);
	pAgain = xrtTomlParseMemory(sNumbers, sizeof(sNumbers) - 1u,
		NULL, &tError);
	bOk &= check(pAgain != NULL &&
		xrtValueGetFloat(xrtValueObjectGet(pAgain, xrtStrView("small")), &fValue) &&
		fValue == 1e-12 &&
		xrtValueGetFloat(xrtValueObjectGet(pAgain, xrtStrView("minusZero")), &fValue) &&
		fValue == 0.0 && signbit(fValue) &&
		xrtValueGetFloat(xrtValueObjectGet(pAgain, xrtStrView("positiveInf")), &fValue) &&
		isinf(fValue) && fValue > 0.0 &&
		xrtValueGetFloat(xrtValueObjectGet(pAgain, xrtStrView("notNumber")), &fValue) &&
		isnan(fValue), "TOML float grammar changed under numeric locale");
cleanup:
	xrtValueRelease(pAgain);
	xrtBufferDestroy(pOutput);
	xrtValueRelease(pRatio);
	xrtValueRelease(pRoot);
	setlocale(LC_NUMERIC, sPrevious);
	if ( bChanged ) { puts("test_toml: non-C numeric locale exercised"); }
	return bOk;
}

static int testFloatBoundary(void)
{
	static const char sFinite[] = "value = 1.7976931348623157e308\n";
	static const char sOverflow[] = "value = 1e999\n";
	xrttomlerror tError;
	xvalue* pRoot = xrtTomlParseMemory(sFinite, sizeof(sFinite) - 1u,
		NULL, &tError);
	double fValue = 0.0;
	int bOk = check(pRoot != NULL &&
		xrtValueGetFloat(xrtValueObjectGet(pRoot, xrtStrView("value")), &fValue) &&
		isfinite(fValue), "largest finite TOML float was rejected");
	xrtValueRelease(pRoot);
	pRoot = xrtTomlParseMemory(sOverflow, sizeof(sOverflow) - 1u,
		NULL, &tError);
	bOk &= check(pRoot == NULL && tError.iCode == XRT_TOML_ERROR_FORMAT,
		"finite TOML literal overflowed to infinity");
	xrtValueRelease(pRoot);
	return bOk;
}

static int testInvalidOptions(void)
{
	static const char sSource[] = "value = 1\n";
	xrttomloptions tOptions;
	xrttomlerror tError;
	xvalue* pRoot;
	xbuffer* pOutput;
	int bOk = 1;

	xrtTomlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 1u;
	tOptions.iVersion++;
	pRoot = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		&tOptions, &tError);
	bOk &= check(pRoot == NULL && tError.iCode == XRT_TOML_ERROR_ARGUMENT,
		"invalid TOML options version silently changed input limit");
	xrtValueRelease(pRoot);
	tOptions.iVersion = XRT_TOML_OPTIONS_VERSION;
	tOptions.iSize = sizeof(tOptions) - 1u;
	pRoot = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		&tOptions, &tError);
	bOk &= check(pRoot == NULL && tError.iCode == XRT_TOML_ERROR_ARGUMENT,
		"invalid TOML options size silently changed input limit");
	xrtValueRelease(pRoot);
	tOptions.iSize = sizeof(tOptions);
	tOptions.tLimits.iVersion++;
	pRoot = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		&tOptions, &tError);
	bOk &= check(pRoot == NULL && tError.iCode == XRT_TOML_ERROR_ARGUMENT,
		"invalid TOML resource limits silently changed input limit");
	xrtValueRelease(pRoot);
	tOptions.tLimits.iVersion = XRT_RESOURCE_LIMITS_VERSION;
	tOptions.tLimits.iReserved = 1u;
	pRoot = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		&tOptions, &tError);
	bOk &= check(pRoot == NULL && tError.iCode == XRT_TOML_ERROR_ARGUMENT,
		"TOML parser accepted a reserved resource field");
	xrtValueRelease(pRoot);
	tOptions.tLimits.iReserved = 0u;
	pRoot = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		&tOptions, &tError);
	bOk &= check(pRoot == NULL && tError.iCode == XRT_TOML_ERROR_LIMIT,
		"valid TOML input limit was ignored");
	xrtValueRelease(pRoot);

	/* TOML 不使用压缩倍率；未知资源标志必须在解析前拒绝。 */
	xrtTomlOptionsInit(&tOptions);
	bOk &= check(tOptions.tLimits.iMaxCompressionRatio == 0u,
		"TOML options inherited an unused compression ratio limit");
	tOptions.tLimits.iFlags = 1u;
	pRoot = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		&tOptions, &tError);
	bOk &= check(pRoot == NULL && tError.iCode == XRT_TOML_ERROR_ARGUMENT,
		"TOML parser accepted an unsupported resource flag");
	xrtValueRelease(pRoot);

	pRoot = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		NULL, &tError);
	if ( pRoot == NULL ) { return 0; }
	xrtTomlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxOutputBytes = 1u;
	tOptions.iVersion++;
	pOutput = xrtTomlStringify(pRoot, &tOptions, &tError);
	bOk &= check(pOutput == NULL && tError.iCode == XRT_TOML_ERROR_ARGUMENT,
		"invalid TOML writer options silently changed output limit");
	xrtBufferDestroy(pOutput);
	tOptions.iVersion = XRT_TOML_OPTIONS_VERSION;
	tOptions.tLimits.iReserved = 1u;
	pOutput = xrtTomlStringify(pRoot, &tOptions, &tError);
	bOk &= check(pOutput == NULL && tError.iCode == XRT_TOML_ERROR_ARGUMENT,
		"TOML writer accepted a reserved resource field");
	xrtBufferDestroy(pOutput);
	tOptions.tLimits.iReserved = 0u;
	pOutput = xrtTomlStringify(pRoot, &tOptions, &tError);
	bOk &= check(pOutput == NULL && tError.iCode == XRT_TOML_ERROR_LIMIT,
		"valid TOML output limit was ignored");
	xrtBufferDestroy(pOutput);
	xrtTomlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxCompressionRatio = 1u;
	pOutput = xrtTomlStringify(pRoot, &tOptions, &tError);
	bOk &= check(pOutput == NULL && tError.iCode == XRT_TOML_ERROR_ARGUMENT,
		"TOML writer accepted an unused compression ratio limit");
	xrtBufferDestroy(pOutput);
	xrtValueRelease(pRoot);
	return bOk;
}

static int testWriterItemAndEntryLimits(void)
{
	static const struct {
		const char* Source;
		uint64 MaxItem;
		uint64 MaxEntries;
		const char* Message;
	} Cases[] = {
		{ "x = \"abcd\"\n", 3u, 0u, "TOML writer ignored string item limit" },
		{ "abcd = 1\n", 3u, 0u, "TOML writer ignored key item limit" },
		{ "x = [1, 2, 3]\n", 0u, 2u, "TOML writer ignored array entry limit" },
		{ "a = 1\nb = 2\nc = 3\n", 0u, 2u,
			"TOML writer ignored table entry limit" },
		{ "x = [{a = 1, b = 2, c = 3}, 1]\n", 0u, 2u,
			"TOML writer ignored inline table entry limit" },
		{ "[[x]]\na = 1\n[[x]]\na = 2\n[[x]]\na = 3\n", 0u, 2u,
			"TOML writer ignored array-of-tables entry limit" }
	};
	int bOk = 1;
	size_t i;
	for ( i = 0u; i < sizeof(Cases) / sizeof(Cases[0]); ++i ) {
		xrttomloptions tOptions;
		xrttomlerror tError;
		xvalue* pRoot = xrtTomlParseMemory(Cases[i].Source,
			strlen(Cases[i].Source), NULL, &tError);
		xbuffer* pOutput;
		if ( !check(pRoot != NULL, "cannot prepare TOML writer limit test") ) {
			return 0;
		}
		xrtTomlOptionsInit(&tOptions);
		tOptions.tLimits.iMaxItemBytes = Cases[i].MaxItem;
		tOptions.tLimits.iMaxEntries = Cases[i].MaxEntries;
		pOutput = xrtTomlStringify(pRoot, &tOptions, &tError);
		bOk &= check(pOutput == NULL && tError.iCode == XRT_TOML_ERROR_LIMIT,
			Cases[i].Message);
		xrtBufferDestroy(pOutput);
		if ( Cases[i].MaxItem != 0u ) { tOptions.tLimits.iMaxItemBytes++; }
		if ( Cases[i].MaxEntries != 0u ) { tOptions.tLimits.iMaxEntries++; }
		pOutput = xrtTomlStringify(pRoot, &tOptions, &tError);
		bOk &= check(pOutput != NULL && tError.iCode == XRT_TOML_OK,
			"TOML writer rejected a value at the exact configured limit");
		xrtBufferDestroy(pOutput);
		pOutput = xrtTomlStringify(pRoot, NULL, &tError);
		bOk &= check(pOutput != NULL && tError.iCode == XRT_TOML_OK,
			"TOML writer did not recover after limit rejection");
		xrtBufferDestroy(pOutput);
		xrtValueRelease(pRoot);
	}
	return bOk;
}

static int testWriterArrayTableShapeLimits(void)
{
	static const char sSource[] = "[[x]]\n";
	xrttomloptions tOptions;
	xrttomlerror tError;
	xvalue* pRoot = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		NULL, &tError);
	xvalue* pParsed;
	xbuffer* pOutput;
	int bOk = check(pRoot != NULL, "cannot prepare array-of-tables limit test");
	if ( !bOk ) { return 0; }

	xrtTomlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxNodes = 2u;
	pParsed = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		&tOptions, &tError);
	bOk &= check(pParsed == NULL && tError.iCode == XRT_TOML_ERROR_LIMIT,
		"TOML parser did not count array-of-tables array node");
	xrtValueRelease(pParsed);
	pOutput = xrtTomlStringify(pRoot, &tOptions, &tError);
	bOk &= check(pOutput == NULL && tError.iCode == XRT_TOML_ERROR_LIMIT,
		"TOML writer did not count array-of-tables array node");
	xrtBufferDestroy(pOutput);
	tOptions.tLimits.iMaxNodes = 3u;
	pOutput = xrtTomlStringify(pRoot, &tOptions, &tError);
	bOk &= check(pOutput != NULL && tError.iCode == XRT_TOML_OK,
		"TOML writer rejected exact array-of-tables node limit");
	xrtBufferDestroy(pOutput);

	xrtTomlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxDepth = 2u;
	pParsed = xrtTomlParseMemory(sSource, sizeof(sSource) - 1u,
		&tOptions, &tError);
	bOk &= check(pParsed == NULL && tError.iCode == XRT_TOML_ERROR_LIMIT,
		"TOML parser did not count array-of-tables depth");
	xrtValueRelease(pParsed);
	pOutput = xrtTomlStringify(pRoot, &tOptions, &tError);
	bOk &= check(pOutput == NULL && tError.iCode == XRT_TOML_ERROR_LIMIT,
		"TOML writer did not count array-of-tables depth");
	xrtBufferDestroy(pOutput);
	tOptions.tLimits.iMaxDepth = 3u;
	pOutput = xrtTomlStringify(pRoot, &tOptions, &tError);
	bOk &= check(pOutput != NULL && tError.iCode == XRT_TOML_OK,
		"TOML writer rejected exact array-of-tables depth limit");
	xrtBufferDestroy(pOutput);
	xrtValueRelease(pRoot);
	return bOk;
}

int main(void)
{
	int bOk;
	bOk = testParserAllocator();
	bOk &= testDateAllocationOwnership();
	bOk &= testParseAllocationFailures();
	bOk &= testParseAndRoundTrip();
	bOk &= testExcessFraction();
	bOk &= testErrorsAndLimits();
	bOk &= testDateTimeProbe();
	bOk &= testDateShapeCollision();
	bOk &= testDateTimeValidation();
	bOk &= testUnknownSizeReader();
	bOk &= testKnownSizeLimitBeforeRead();
	bOk &= testKnownSizeReaderPosition();
	bOk &= testEmbeddedNulInput();
	bOk &= testWriterRejectsInvalidUtf8();
	bOk &= testSave();
	bOk &= testWriterValidUtf8();
	bOk &= testWriterDecimalLocale();
	bOk &= testFloatBoundary();
	bOk &= testInvalidOptions();
	bOk &= testWriterItemAndEntryLimits();
	bOk &= testWriterArrayTableShapeLimits();
	bOk &= testOwnedWriterCloseFailure();
	bOk &= testWriterMemoryFailure();
	bOk &= testOwnedReaderCloseFailure();
	bOk &= testBorrowedStreamsStayOpen();
	if ( bOk ) { puts("test_toml: PASS"); }
	return bOk ? 0 : 2;
}

/* 单文件测试由 xl 注入 XRT；正式 package 仍按清单独立编译这些实现。 */
#include "../../_tomlc17/tomlc17_xrt.c"
#include "../xrt_toml_types.c"
#include "../xrt_toml_parser.c"
#include "../xrt_toml_writer.c"
