/* 独立 C 测试和包导入使用相同的完整 XRT 类型表。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_plist.h"

#include <locale.h>
#include <stdio.h>
#include <string.h>



static int testFormat(int iFormat)
{
	xrtplistdocument* pDocument = xrtPlistCreate();
	xrtplistnode* pRoot = xrtPlistCreateDict(pDocument);
	xrtplistnode* pArray = xrtPlistCreateArray(pDocument);
	const uint8 sData[] = { 0u, 255u, 'X', 'L' };
	xrtplistoptions tOptions;
	xrtplisterror tError;
	xbuffer* pEncoded;
	xrtplistdocument* pParsed;
	xrtplistnode* pParsedRoot;
	int bOk = pDocument != NULL && pRoot != NULL && pArray != NULL;
	bOk = bOk && xrtPlistSetRoot(pDocument, pRoot) &&
		xrtPlistDictSet(pRoot, "name", 4u,
			xrtPlistCreateString(pDocument, (str)"xlang 你好")) &&
		xrtPlistDictSet(pRoot, "line\rkey", 8u,
			xrtPlistCreateString(pDocument, (str)"A\rB")) &&
		xrtPlistDictSet(pRoot, "data", 4u,
			xrtPlistCreateData(pDocument, sData, sizeof(sData))) &&
		xrtPlistArrayAppend(pArray, xrtPlistCreateInt(pDocument, -7)) &&
		xrtPlistArrayAppend(pArray, xrtPlistCreateBool(pDocument, true)) &&
		xrtPlistDictSet(pRoot, "items", 5u, pArray);
	if ( iFormat == XRT_PLIST_FORMAT_BINARY ) {
		bOk = bOk && xrtPlistDictSet(pRoot, "uid", 3u,
			xrtPlistCreateUid(pDocument, UINT64_MAX)) &&
			xrtPlistDictSet(pRoot, "none", 4u,
				xrtPlistCreateNull(pDocument));
	}
	xrtPlistOptionsInit(&tOptions);
	tOptions.iFormat = iFormat;
	tOptions.bPretty = true;
	tOptions.bSortKeys = true;
	pEncoded = bOk ? xrtPlistStringify(pDocument, &tOptions, &tError) : NULL;
	pParsed = pEncoded != NULL ? xrtPlistParseMemory(pEncoded->Data,
		pEncoded->Size, NULL, &tError) : NULL;
	pParsedRoot = pParsed != NULL ? xrtPlistRoot(pParsed) : NULL;
	bOk = bOk && pEncoded != NULL && pParsedRoot != NULL &&
		xrtPlistNodeType(pParsedRoot) == XRT_PLIST_DICT &&
		xrtPlistByteSize(xrtPlistDictGet(pParsedRoot, "line\rkey", 8u)) == 3u &&
		memcmp(xrtPlistBytes(xrtPlistDictGet(pParsedRoot, "line\rkey", 8u)),
			"A\rB", 3u) == 0 &&
		xrtPlistByteSize(xrtPlistDictGet(pParsedRoot, "data", 4u)) == 4u &&
		xrtPlistInt(xrtPlistArrayGet(
			xrtPlistDictGet(pParsedRoot, "items", 5u), 0u)) == -7;
	if ( iFormat == XRT_PLIST_FORMAT_BINARY ) {
		bOk = bOk && xrtPlistUid(xrtPlistDictGet(pParsedRoot,
			"uid", 3u)) == UINT64_MAX;
	} else {
		bOk = bOk && xrtBufferAppendByte(pEncoded, 0u) &&
			strstr((const char*)pEncoded->Data,
			"<!DOCTYPE plist PUBLIC") != NULL;
	}
	xrtPlistDestroy(pParsed);
	xrtBufferDestroy(pEncoded);
	xrtPlistDestroy(pDocument);
	return bOk;
}

static int testIndentedXmlData(void)
{
	const char* sXml = "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict>"
		"<key>payload</key><data>\n\tAAH+/w==\n\t</data></dict></plist>";
	xrtplisterror tError;
	xrtplistdocument* pDocument = xrtPlistParseMemory(sXml, strlen(sXml),
		NULL, &tError);
	xrtplistnode* pData = pDocument != NULL ? xrtPlistDictGet(
		xrtPlistRoot(pDocument), "payload", 7u) : NULL;
	int bOk = pData != NULL && xrtPlistByteSize(pData) == 4u &&
		((const uint8*)xrtPlistBytes(pData))[0] == 0u &&
		((const uint8*)xrtPlistBytes(pData))[3] == 255u;
	xrtPlistDestroy(pDocument);
	return bOk;
}

/* 整数解析既要严格判定边界，也不能在去空白分配失败时崩溃。 */
static int testXmlIntegerFailures(void)
{
	static const char* sValues[] = {
		"  +42 \n", "-9223372036854775808", "9223372036854775807",
		"9223372036854775808", "12x"
	};
	static const int64 iExpected[] = { 42, INT64_MIN, INT64_MAX, 0, 0 };
	static const char sXml[] = "<plist version=\"1.0\"><integer> 42 </integer></plist>";
	char sInput[160];
	xrtplisterror tError;
	xrtplistdocument* pDocument;
	size_t i;
	uint64 iPoint;
	int bSawFailure = 0;
	int bSawSuccess = 0;
	for ( i = 0u; i < sizeof(sValues) / sizeof(sValues[0]); i++ ) {
		int iLength = snprintf(sInput, sizeof(sInput),
			"<plist version=\"1.0\"><integer>%s</integer></plist>", sValues[i]);
		if ( iLength < 0 || (size_t)iLength >= sizeof(sInput) ) { return 0; }
		pDocument = xrtPlistParseMemory(sInput, (uint64)iLength, NULL, &tError);
		if ( i < 3u ) {
			if ( pDocument == NULL || tError.iCode != XRT_PLIST_OK ||
				xrtPlistInt(xrtPlistRoot(pDocument)) != iExpected[i] ) {
				xrtPlistDestroy(pDocument); return 0;
			}
		} else if ( pDocument != NULL || tError.iCode != XRT_PLIST_ERROR_FORMAT ) {
			xrtPlistDestroy(pDocument); return 0;
		}
		xrtPlistDestroy(pDocument);
	}
	for ( iPoint = 0u; iPoint < 128u; iPoint++ ) {
		int bTriggered;
		if ( !xrtMemDebugFailAfter(iPoint) ) { return 0; }
		pDocument = xrtPlistParseMemory(sXml, sizeof(sXml) - 1u,
			NULL, &tError);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pDocument == NULL ) {
			bSawFailure = 1;
			if ( !bTriggered || tError.iCode != XRT_PLIST_ERROR_MEMORY ) {
				fprintf(stderr, "plist XML integer allocation point %llu: %d %s\n",
					(unsigned long long)iPoint, tError.iCode, tError.sMessage);
				xrtPlistDestroy(pDocument); return 0;
			}
		} else {
			if ( !bTriggered ) { bSawSuccess = 1; }
			if ( tError.iCode != XRT_PLIST_OK ||
				xrtPlistInt(xrtPlistRoot(pDocument)) != 42 ) {
				xrtPlistDestroy(pDocument); return 0;
			}
		}
		xrtPlistDestroy(pDocument);
	}
	return bSawFailure && bSawSuccess;
}

/* 相邻标量分支也必须在分配失败时保留 MEMORY，而非误报 FORMAT。 */
static int testXmlScalarFailures(void)
{
	static const struct {
		const char* sInput;
		int iType;
	} sCases[] = {
		{ "<plist><string>value</string></plist>", XRT_PLIST_STRING },
		{ "<plist><real>1.25</real></plist>", XRT_PLIST_REAL },
		{ "<plist><date>2026-10-17T00:00:00Z</date></plist>", XRT_PLIST_DATE },
		{ "<plist><data>AAE=</data></plist>", XRT_PLIST_DATA }
	};
	size_t i;
	uint64 iPoint;
	xrtplisterror tError;
	xrtplistdocument* pDocument;
	static const char sEmptyData[] = "<plist><data> \n </data></plist>";
	static const char sInvalidData[] = "<plist><data>AA$=</data></plist>";
	pDocument = xrtPlistParseMemory(sEmptyData, sizeof(sEmptyData) - 1u,
		NULL, &tError);
	if ( pDocument == NULL || tError.iCode != XRT_PLIST_OK ||
		xrtPlistNodeType(xrtPlistRoot(pDocument)) != XRT_PLIST_DATA ||
		xrtPlistByteSize(xrtPlistRoot(pDocument)) != 0u ) {
		xrtPlistDestroy(pDocument); return 0;
	}
	xrtPlistDestroy(pDocument);
	pDocument = xrtPlistParseMemory(sInvalidData, sizeof(sInvalidData) - 1u,
		NULL, &tError);
	if ( pDocument != NULL || tError.iCode != XRT_PLIST_ERROR_FORMAT ) {
		xrtPlistDestroy(pDocument); return 0;
	}
	for ( i = 0u; i < sizeof(sCases) / sizeof(sCases[0]); i++ ) {
		int bSawFailure = 0;
		int bSawSuccess = 0;
		for ( iPoint = 0u; iPoint < 128u; iPoint++ ) {
			xrtplisterror tError;
			xrtplistdocument* pDocument;
			int bTriggered;
			if ( !xrtMemDebugFailAfter(iPoint) ) { return 0; }
			pDocument = xrtPlistParseMemory(sCases[i].sInput,
				strlen(sCases[i].sInput), NULL, &tError);
			bTriggered = xrtMemDebugFailTriggered();
			xrtMemDebugFailClear();
			if ( pDocument == NULL ) {
				bSawFailure = 1;
				if ( !bTriggered || tError.iCode != XRT_PLIST_ERROR_MEMORY ) {
					fprintf(stderr, "plist XML scalar %u allocation point %llu: %d %s\n",
						(unsigned)i, (unsigned long long)iPoint,
						tError.iCode, tError.sMessage);
					return 0;
				}
			} else {
				if ( !bTriggered ) { bSawSuccess = 1; }
				if ( tError.iCode != XRT_PLIST_OK ||
					xrtPlistNodeType(xrtPlistRoot(pDocument)) != sCases[i].iType ) {
					xrtPlistDestroy(pDocument); return 0;
				}
			}
			xrtPlistDestroy(pDocument);
		}
		if ( !bSawFailure || !bSawSuccess ) { return 0; }
	}
	return 1;
}

/* XML DOM 返回的子节点持有文档引用，解析结束后必须全部释放。 */
static int testXmlDocumentOwnership(void)
{
	static const struct {
		const char* sXml;
		int bValid;
	} sCases[] = {
		{ "<plist><dict><key>items</key><array><string>a</string>"
			"<dict><key>n</key><integer>2</integer></dict></array></dict></plist>", 1 },
		{ "<plist><array><string>a</string>bad</array></plist>", 0 },
		{ "<plist><string>a</string><string>b</string></plist>", 0 },
		{ "<plist><dict><string>not a key</string><integer>1</integer></dict></plist>", 0 }
	};
	xmemdebugsnapshot tBefore;
	xmemdebugsnapshot tAfter;
	size_t i;
	uint64 iPoint;
	for ( i = 0u; i < sizeof(sCases) / sizeof(sCases[0]); i++ ) {
		xrtplisterror tError;
		xrtplistdocument* pDocument;
		xrtMemDebugSnapshot(&tBefore);
		pDocument = xrtPlistParseMemory(sCases[i].sXml,
			strlen(sCases[i].sXml), NULL, &tError);
		if ( (pDocument != NULL) != sCases[i].bValid ) {
			xrtPlistDestroy(pDocument); return 0;
		}
		xrtPlistDestroy(pDocument);
		xrtMemDebugSnapshot(&tAfter);
		if ( tAfter.LiveCount != tBefore.LiveCount ) {
			fprintf(stderr, "plist XML ownership case %u: %llu -> %llu\n",
				(unsigned)i, (unsigned long long)tBefore.LiveCount,
				(unsigned long long)tAfter.LiveCount);
			return 0;
		}
	}
	for ( iPoint = 0u; iPoint < 256u; iPoint++ ) {
		xrtplisterror tError;
		xrtplistdocument* pDocument;
		xrtMemDebugSnapshot(&tBefore);
		if ( !xrtMemDebugFailAfter(iPoint) ) { return 0; }
		pDocument = xrtPlistParseMemory(sCases[0].sXml,
			strlen(sCases[0].sXml), NULL, &tError);
		xrtMemDebugFailClear();
		xrtPlistDestroy(pDocument);
		xrtMemDebugSnapshot(&tAfter);
		if ( tAfter.LiveCount != tBefore.LiveCount ) {
			fprintf(stderr, "plist XML ownership allocation %llu: %llu -> %llu\n",
				(unsigned long long)iPoint,
				(unsigned long long)tBefore.LiveCount,
				(unsigned long long)tAfter.LiveCount);
			return 0;
		}
	}
	return 1;
}

/* XML plist 的小数点是格式的一部分，不随 LC_NUMERIC 改变。 */
static int testXmlRealLocale(void)
{
	static const char* sLocales[] = {
		"French_France.1252", "fr-FR", "German_Germany.1252", "de-DE",
		"fr_FR.UTF-8", "de_DE.UTF-8"
	};
	const char* sCurrent = setlocale(LC_NUMERIC, NULL);
	const char* sExternal = "<plist version=\"1.0\"><real>1.5</real></plist>";
	const char* sComma = "<plist version=\"1.0\"><real>1,5</real></plist>";
	char sSaved[128];
	size_t i;
	int bOk = 0;
	xrtplistdocument* pDocument;
	xrtplistdocument* pParsed;
	xrtplistdocument* pBad;
	xrtplistoptions tOptions;
	xrtplisterror tError;
	xbuffer* pOutput;
	if ( sCurrent == NULL || strlen(sCurrent) >= sizeof(sSaved) ) { return 0; }
	strcpy(sSaved, sCurrent);
	for ( i = 0; i < sizeof(sLocales) / sizeof(sLocales[0]); i++ ) {
		if ( setlocale(LC_NUMERIC, sLocales[i]) != NULL &&
			strcmp(localeconv()->decimal_point, ",") == 0 ) { break; }
	}
	if ( i == sizeof(sLocales) / sizeof(sLocales[0]) ) {
		setlocale(LC_NUMERIC, sSaved);
		return 1;
	}
	pDocument = xrtPlistCreate();
	xrtPlistOptionsInit(&tOptions);
	tOptions.iFormat = XRT_PLIST_FORMAT_XML;
	pOutput = pDocument != NULL && xrtPlistSetRoot(pDocument,
		xrtPlistCreateReal(pDocument, 1.5)) ?
		xrtPlistStringify(pDocument, &tOptions, &tError) : NULL;
	pParsed = xrtPlistParseMemory(sExternal, strlen(sExternal), NULL, &tError);
	pBad = xrtPlistParseMemory(sComma, strlen(sComma), NULL, &tError);
	bOk = pOutput != NULL && xrtBufferAppendByte(pOutput, 0u) &&
		strstr((const char*)pOutput->Data, "<real>1.5</real>") != NULL &&
		pParsed != NULL && xrtPlistNodeType(xrtPlistRoot(pParsed)) == XRT_PLIST_REAL &&
		xrtPlistReal(xrtPlistRoot(pParsed)) == 1.5 &&
		pBad == NULL && tError.iCode == XRT_PLIST_ERROR_FORMAT;
	xrtPlistDestroy(pParsed);
	xrtPlistDestroy(pBad);
	xrtBufferDestroy(pOutput);
	xrtPlistDestroy(pDocument);
	setlocale(LC_NUMERIC, sSaved);
	return bOk;
}



/* 用外部规范字节验证 binary plist 日期的秒单位，避免同源读写互相掩盖错误。 */
static int testBinaryDateUnit(void)
{
	static const uint8 sExpectedObject[] = {
		0x33u, 0x3fu, 0xf0u, 0u, 0u, 0u, 0u, 0u, 0u
	};
	static const uint8 sExpectedMicrosecond[] = {
		0x33u, 0x3eu, 0xb0u, 0xc6u, 0xf7u, 0xa0u, 0xb5u, 0xedu, 0x8du
	};
	uint8 sExternal[50] = {0};
	xdatetime tDateTime;
	xtime tEpoch;
	xrtplistdocument* pParsed;
	xrtplistdocument* pDocument;
	xrtplistoptions tOptions;
	xrtplisterror tError;
	xbuffer* pEncoded;
	int bOk;

	memset(&tDateTime, 0, sizeof(tDateTime));
	tDateTime.Year = 2001;
	tDateTime.Month = 1;
	tDateTime.Day = 1;
	if ( !xrtTimeMake(&tDateTime, &tEpoch) ) { return 0; }

	memcpy(sExternal, "bplist00", 8u);
	memcpy(sExternal + 8u, sExpectedObject, sizeof(sExpectedObject));
	sExternal[17] = 8u;
	sExternal[24] = 1u;
	sExternal[25] = 1u;
	sExternal[33] = 1u;
	sExternal[49] = 17u;
	pParsed = xrtPlistParseMemory(sExternal, sizeof(sExternal), NULL, &tError);
	bOk = pParsed != NULL &&
		xrtPlistDate(xrtPlistRoot(pParsed)) == tEpoch + XRT_TIME_SECOND;
	xrtPlistDestroy(pParsed);

	pDocument = xrtPlistCreate();
	bOk = bOk && pDocument != NULL && xrtPlistSetRoot(pDocument,
		xrtPlistCreateDate(pDocument, tEpoch + XRT_TIME_SECOND));
	xrtPlistOptionsInit(&tOptions);
	tOptions.iFormat = XRT_PLIST_FORMAT_BINARY;
	pEncoded = bOk ? xrtPlistStringify(pDocument, &tOptions, &tError) : NULL;
	bOk = bOk && pEncoded != NULL && pEncoded->Size >= 17u &&
		memcmp(pEncoded->Data + 8u, sExpectedObject,
			sizeof(sExpectedObject)) == 0;
	xrtBufferDestroy(pEncoded);
	xrtPlistDestroy(pDocument);

	/* Python plistlib 生成的 2001-01-01T00:00:00.000001 二进制对象。 */
	memcpy(sExternal + 8u, sExpectedMicrosecond, sizeof(sExpectedMicrosecond));
	pParsed = xrtPlistParseMemory(sExternal, sizeof(sExternal), NULL, &tError);
	bOk = bOk && pParsed != NULL &&
		xrtPlistDate(xrtPlistRoot(pParsed)) == tEpoch + 1;
	xrtPlistDestroy(pParsed);
	pDocument = xrtPlistCreate();
	bOk = bOk && pDocument != NULL && xrtPlistSetRoot(pDocument,
		xrtPlistCreateDate(pDocument, tEpoch + 1));
	pEncoded = bOk ? xrtPlistStringify(pDocument, &tOptions, &tError) : NULL;
	bOk = bOk && pEncoded != NULL && pEncoded->Size >= 17u &&
		memcmp(pEncoded->Data + 8u, sExpectedMicrosecond,
			sizeof(sExpectedMicrosecond)) == 0;
	xrtBufferDestroy(pEncoded);
	xrtPlistDestroy(pDocument);
	return bOk;
}

/* XML plist 只表示整秒，不能悄悄丢弃 XRT time 的微秒部分。 */
static int testXmlDatePrecision(void)
{
	xrtplistdocument* pDocument = xrtPlistCreate();
	xrtplistdocument* pRestored;
	xrtplistoptions tOptions;
	xrtplisterror tError;
	xbuffer* pOutput;
	int bOk = pDocument != NULL && xrtPlistSetRoot(pDocument,
		xrtPlistCreateDate(pDocument, 1));

	xrtPlistOptionsInit(&tOptions);
	tOptions.iFormat = XRT_PLIST_FORMAT_XML;
	pOutput = bOk ? xrtPlistStringify(pDocument, &tOptions, &tError) : NULL;
	bOk = bOk && pOutput == NULL && tError.iCode == XRT_PLIST_ERROR_TYPE;
	xrtBufferDestroy(pOutput);
	tOptions.iFormat = XRT_PLIST_FORMAT_BINARY;
	pOutput = bOk ? xrtPlistStringify(pDocument, &tOptions, &tError) : NULL;
	pRestored = pOutput != NULL ? xrtPlistParseMemory(pOutput->Data,
		pOutput->Size, NULL, &tError) : NULL;
	bOk = bOk && pRestored != NULL &&
		xrtPlistDate(xrtPlistRoot(pRestored)) == 1;
	xrtPlistDestroy(pRestored);
	xrtBufferDestroy(pOutput);
	xrtPlistDestroy(pDocument);
	return bOk;
}

static int testBinaryDatePrecisionLimit(void)
{
	xrtplistdocument* pDocument = xrtPlistCreate();
	xrtplistoptions tOptions;
	xrtplisterror tError;
	xbuffer* pOutput;
	int bOk = pDocument != NULL && xrtPlistSetRoot(pDocument,
		xrtPlistCreateDate(pDocument, INT64_C(9000000000000000001)));

	xrtPlistOptionsInit(&tOptions);
	tOptions.iFormat = XRT_PLIST_FORMAT_BINARY;
	pOutput = bOk ? xrtPlistStringify(pDocument, &tOptions, &tError) : NULL;
	bOk = bOk && pOutput == NULL && tError.iCode == XRT_PLIST_ERROR_TYPE;
	xrtBufferDestroy(pOutput);
	xrtPlistDestroy(pDocument);
	return bOk;
}



/* 用与 Python plistlib 相同的单对象布局检查短正整数和负整数。 */
static int testBinaryIntegers(void)
{
	static const struct {
		int64 iValue;
		uint8 sObject[9];
		uint8 iSize;
	} sCases[] = {
		{ 200, { 0x10u, 0xc8u }, 2u },
		{ 40000, { 0x11u, 0x9cu, 0x40u }, 3u },
		{ UINT32_MAX, { 0x12u, 0xffu, 0xffu, 0xffu, 0xffu }, 5u },
		{ -7, { 0x13u, 0xffu, 0xffu, 0xffu, 0xffu,
			0xffu, 0xffu, 0xffu, 0xf9u }, 9u },
		{ INT64_MIN, { 0x13u, 0x80u }, 9u }
	};
	uint32 i;
	for ( i = 0u; i < sizeof(sCases) / sizeof(sCases[0]); i++ ) {
		uint8 sExternal[50] = {0};
		uint32 iTable = 8u + sCases[i].iSize;
		xrtplisterror tError;
		xrtplistoptions tOptions;
		xrtplistdocument* pDocument;
		xrtplistdocument* pParsed;
		xbuffer* pEncoded;
		int bOk;
		memcpy(sExternal, "bplist00", 8u);
		memcpy(sExternal + 8u, sCases[i].sObject, sCases[i].iSize);
		sExternal[iTable] = 8u;
		sExternal[iTable + 1u + 6u] = 1u;
		sExternal[iTable + 1u + 7u] = 1u;
		sExternal[iTable + 1u + 15u] = 1u;
		sExternal[iTable + 1u + 31u] = (uint8)iTable;
		pParsed = xrtPlistParseMemory(sExternal, iTable + 33u,
			NULL, &tError);
		bOk = pParsed != NULL &&
			xrtPlistInt(xrtPlistRoot(pParsed)) == sCases[i].iValue;
		xrtPlistDestroy(pParsed);
		pDocument = xrtPlistCreate();
		bOk = bOk && pDocument != NULL && xrtPlistSetRoot(pDocument,
			xrtPlistCreateInt(pDocument, sCases[i].iValue));
		xrtPlistOptionsInit(&tOptions);
		tOptions.iFormat = XRT_PLIST_FORMAT_BINARY;
		pEncoded = bOk ? xrtPlistStringify(pDocument, &tOptions,
			&tError) : NULL;
		bOk = bOk && pEncoded != NULL &&
			pEncoded->Size >= 8u + sCases[i].iSize &&
			memcmp(pEncoded->Data + 8u, sCases[i].sObject,
				sCases[i].iSize) == 0;
		xrtBufferDestroy(pEncoded);
		xrtPlistDestroy(pDocument);
		if ( !bOk ) { return 0; }
	}
	return 1;
}



static int testInvalidInputs(void)
{
	static const char sExternalEntity[] =
		"<!DOCTYPE plist [<!ENTITY x SYSTEM \"file:///etc/passwd\">]>"
		"<plist version=\"1.0\"><string>&x;</string></plist>";
	uint8 sCycle[43] = { 'b', 'p', 'l', 'i', 's', 't', '0', '0' };
	xrtplistoptions tOptions;
	xrtplisterror tError;
	xrtplistdocument* pDocument;
	xrtplistdocument* pWritable;
	xrtplistnode* pDict;
	int bOk = 1;
	/* 一个 array 对象引用自身：对象、offset table、32 字节 trailer。 */
	sCycle[8] = 0xa1u;
	sCycle[9] = 0u;
	sCycle[10] = 8u;
	sCycle[17] = 1u;
	sCycle[18] = 1u;
	sCycle[26] = 1u;
	sCycle[42] = 10u;
	pDocument = xrtPlistParseMemory(sCycle, sizeof(sCycle), NULL, &tError);
	bOk = bOk && pDocument == NULL && tError.iCode == XRT_PLIST_ERROR_FORMAT;
	xrtPlistDestroy(pDocument);

	sCycle[10] = 0xffu;
	pDocument = xrtPlistParseMemory(sCycle, sizeof(sCycle), NULL, &tError);
	bOk = bOk && pDocument == NULL && tError.iCode == XRT_PLIST_ERROR_FORMAT;
	xrtPlistDestroy(pDocument);

	pDocument = xrtPlistParseMemory(sExternalEntity, sizeof(sExternalEntity) - 1u,
		NULL, &tError);
	bOk = bOk && pDocument == NULL && tError.iCode == XRT_PLIST_ERROR_FORMAT;
	xrtPlistDestroy(pDocument);

	xrtPlistOptionsInit(&tOptions);
	tOptions.tLimits.iMaxDepth = 2u;
	pDocument = xrtPlistParseMemory(
		"<plist><array><array><array/></array></array></plist>",
		strlen("<plist><array><array><array/></array></array></plist>"),
		&tOptions, &tError);
	bOk = bOk && pDocument == NULL && tError.iCode == XRT_PLIST_ERROR_LIMIT;
	xrtPlistDestroy(pDocument);

	pWritable = xrtPlistCreate();
	pDict = xrtPlistCreateDict(pWritable);
	bOk = bOk && pWritable != NULL && pDict != NULL &&
		!xrtPlistDictSet(pDict, "x", SIZE_MAX,
			xrtPlistCreateInt(pWritable, 1));
	xrtPlistDestroy(pWritable);
	return bOk;
}

static int testSaveAllocations(str sPath, const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, const char* sCase)
{
	unsigned int iFailure;
	xrtplisterror tError;
	for ( iFailure = 0u; iFailure < 64u; ++iFailure ) {
		bool bSaved, bTriggered;
		if ( !xrtMemDebugFailAfter(iFailure) ) { return 0; }
		bSaved = xrtPlistSave(sPath, pDocument, pOptions, &tError);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ? (bSaved || tError.iCode != XRT_PLIST_ERROR_MEMORY) :
			(!bSaved || tError.iCode != XRT_PLIST_OK) ) {
			fprintf(stderr, "plist %s save allocation %u misclassified: %d\n",
				sCase, iFailure, tError.iCode);
			return 0;
		}
		xrtClearError();
		if ( !bTriggered ) { return 1; }
	}
	return 0;
}

/* 保存失败不替换已有文件，成功路径生成可重新解析的 plist。 */
static int testSaveFile(void)
{
	static const char sBefore[] = "previous-content";
	str sPath = NULL;
	xfile File = xrtFileTemp(NULL, "xlang-plist-", ".plist", &sPath);
	xrtplistdocument* pDocument = xrtPlistCreate();
	xrtplistoptions tOptions;
	xrtplisterror tError;
	bytes pSaved = NULL;
	size_t iSize = 0u;
	int bOk = File != NULL && sPath != NULL && pDocument != NULL;
	if ( File != NULL ) { bOk = xrtClose(File) && bOk; }
	if ( bOk ) {
		bOk = xrtPlistSetRoot(pDocument,
			xrtPlistCreateString(pDocument, (str)"saved")) &&
			xrtFileWriteAll(sPath, (xbytesview){
			(const uint8*)sBefore, sizeof(sBefore) - 1u });
	}
	xrtPlistOptionsInit(&tOptions);
	tOptions.iFormat = XRT_PLIST_FORMAT_XML;
	tOptions.tLimits.iMaxOutputBytes = 1u;
	if ( bOk ) {
		bOk = !xrtPlistSave(sPath, pDocument, &tOptions, &tError) &&
			tError.iCode != XRT_PLIST_OK;
		pSaved = xrtFileReadAll(sPath, &iSize);
		bOk = bOk && pSaved != NULL && iSize == sizeof(sBefore) - 1u &&
			memcmp(pSaved, sBefore, iSize) == 0;
		xrtFree(pSaved);
		pSaved = NULL;
	}
	tOptions.tLimits.iMaxOutputBytes = 0u;
	if ( bOk ) {
		bOk = xrtPlistSave(sPath, pDocument, &tOptions, &tError);
		pSaved = bOk ? xrtFileReadAll(sPath, &iSize) : NULL;
		bOk = bOk && pSaved != NULL && iSize > 0u &&
			strstr((const char*)pSaved, "<plist") != NULL;
	}
	xrtFree(pSaved);
	if ( bOk ) { bOk = testSaveAllocations(sPath, pDocument, &tOptions, "scalar"); }
	if ( bOk ) {
		xrtplistnode* pDict = xrtPlistCreateDict(pDocument);
		xrtplistnode* pValue = xrtPlistCreateString(pDocument, (str)"saved");
		bOk = pDict != NULL && pValue != NULL &&
			xrtPlistDictSet(pDict, "name", 4u, pValue) &&
			xrtPlistSetRoot(pDocument, pDict) &&
			testSaveAllocations(sPath, pDocument, &tOptions, "dict");
	}
	xrtPlistDestroy(pDocument);
	if ( sPath != NULL ) { xrtFileDelete(sPath); }
	xrtFree(sPath);
	return bOk;
}

typedef struct {
	const uint8* pData;
	size_t iSize;
	size_t iOffset;
} xplisttestreader;

static bool testReadUnknown(ptr pContext, ptr pBuffer,
	size_t iRequest, size_t* pRead)
{
	xplisttestreader* pStream = (xplisttestreader*)pContext;
	size_t iRemain = pStream->iSize - pStream->iOffset;
	size_t iCount = iRemain < iRequest ? iRemain : iRequest;
	memcpy(pBuffer, pStream->pData + pStream->iOffset, iCount);
	pStream->iOffset += iCount;
	*pRead = iCount;
	return true;
}

static bool testSizeOnly(ptr pContext, uint64* pSize)
{
	*pSize = ((xplisttestreader*)pContext)->iSize;
	return true;
}

/* 输入上限约束的是当前位置之后的内容，不是底层 Reader 的总长度。 */
static int testSeekedReaderLimit(void)
{
	static const char sInput[] = "XXXXXXXXXX<plist><string>ok</string></plist>";
	const uint64 iPrefix = 10u;
	const uint64 iRemaining = sizeof(sInput) - 1u - iPrefix;
	xrtplistoptions Options;
	xrtplisterror Error;
	int i;
	xrtPlistOptionsInit(&Options);
	for ( i = 0; i < 2; ++i ) {
		xreader* pReader = xrtReaderFromMemory((xbytesview){
			(cbytes)sInput, sizeof(sInput) - 1u });
		xrtplistdocument* pDocument;
		uint64 iPosition = 0u;
		if ( pReader == NULL || !xrtReaderSeek(pReader, (int64)iPrefix,
			XSEEK_START, &iPosition) || iPosition != iPrefix ) {
			xrtReaderDestroy(pReader);
			return 0;
		}
		Options.tLimits.iMaxInputBytes = i == 0 ? iRemaining : iRemaining - 1u;
		pDocument = xrtPlistParse(pReader, true, &Options, &Error);
		if ( (i == 0 && (pDocument == NULL || Error.iCode != XRT_PLIST_OK ||
			xrtPlistNodeType(xrtPlistRoot(pDocument)) != XRT_PLIST_STRING)) ||
			(i == 1 && (pDocument != NULL || Error.iCode != XRT_PLIST_ERROR_LIMIT)) ) {
			fprintf(stderr, "plist seeked reader limit case %d: %d %s\n",
				i, Error.iCode, Error.sMessage);
			xrtPlistDestroy(pDocument);
			return 0;
		}
		xrtPlistDestroy(pDocument);
	}
	{
		const xreaderops Ops = { testReadUnknown, NULL, NULL, testSizeOnly, NULL };
		xplisttestreader Source = { (const uint8*)sInput,
			sizeof(sInput) - 1u, (size_t)iPrefix };
		xreader* pReader = xrtReaderCreate(&Ops, &Source);
		xrtplistdocument* pDocument;
		if ( pReader == NULL ) { return 0; }
		Options.tLimits.iMaxInputBytes = iRemaining;
		pDocument = xrtPlistParse(pReader, true, &Options, &Error);
		if ( pDocument == NULL || Error.iCode != XRT_PLIST_OK ) {
			xrtPlistDestroy(pDocument);
			return 0;
		}
		xrtPlistDestroy(pDocument);
	}
	return 1;
}

static int testReadErrors(void)
{
	static const char sXml[] = "<plist version=\"1.0\"><string>ok</string></plist>";
	xplisttestreader tStream = { (const uint8*)sXml, sizeof(sXml) - 1u, 0u };
	xreaderops Ops = {0};
	xrtplistoptions Options;
	xrtplisterror Error;
	xrtplistdocument* pDocument;
	xreader* pReader;
	xfile File;
	str sMissing = NULL;
	int bOk = 1;

	Ops.Read = testReadUnknown; /* 无 Size：只能在读取中发现超限。 */
	xrtPlistOptionsInit(&Options);
	Options.tLimits.iMaxInputBytes = 4u;
	pReader = xrtReaderCreate(&Ops, &tStream);
	if ( pReader == NULL ) { return 0; }
	pDocument = xrtPlistParse(pReader, true, &Options, &Error);
	if ( Error.iCode != XRT_PLIST_ERROR_LIMIT ) {
		fprintf(stderr, "plist stream limit code: %d\n", Error.iCode);
	}
	bOk = pDocument == NULL && Error.iCode == XRT_PLIST_ERROR_LIMIT;
	xrtPlistDestroy(pDocument);

	xrtPlistOptionsInit(&Options);
	pReader = xrtReaderFromMemory((xbytesview){
		(const uint8*)sXml, sizeof(sXml) - 1u });
	if ( pReader == NULL ) { return 0; }
	xrtClearError();
	bOk = xrtMemDebugFailAfter(0u) && bOk;
	pDocument = xrtPlistParse(pReader, true, &Options, &Error);
	xrtMemDebugFailClear();
	if ( Error.iCode != XRT_PLIST_ERROR_MEMORY ) {
		fprintf(stderr, "plist stream memory code: %d\n", Error.iCode);
	}
	bOk = pDocument == NULL && Error.iCode == XRT_PLIST_ERROR_MEMORY && bOk;
	xrtPlistDestroy(pDocument);

	xrtClearError();
	bOk = xrtMemDebugFailAfter(0u) && bOk;
	pDocument = xrtPlistParseMemory(sXml, sizeof(sXml) - 1u,
		&Options, &Error);
	xrtMemDebugFailClear();
	if ( Error.iCode != XRT_PLIST_ERROR_MEMORY ) {
		fprintf(stderr, "plist reader creation code: %d\n", Error.iCode);
	}
	bOk = pDocument == NULL && Error.iCode == XRT_PLIST_ERROR_MEMORY && bOk;
	xrtPlistDestroy(pDocument);

	File = xrtFileTemp(NULL, "xlang-plist-missing-", ".plist", &sMissing);
	if ( File == NULL || sMissing == NULL ) {
		if ( File != NULL ) { xrtClose(File); }
		xrtFree(sMissing);
		return 0;
	}
	bOk = xrtClose(File) && xrtFileDelete(sMissing) && bOk;
	pDocument = xrtPlistLoad(sMissing, &Options, &Error);
	if ( Error.iCode != XRT_PLIST_ERROR_IO ) {
		fprintf(stderr, "plist missing file code: %d\n", Error.iCode);
	}
	bOk = pDocument == NULL && Error.iCode == XRT_PLIST_ERROR_IO && bOk;
	xrtPlistDestroy(pDocument);
	xrtFree(sMissing);

	pDocument = xrtPlistParseMemory(sXml, sizeof(sXml) - 1u,
		&Options, &Error);
	bOk = pDocument != NULL && Error.iCode == XRT_PLIST_OK && bOk;
	xrtPlistDestroy(pDocument);
	Options.tLimits.iMaxInputBytes = 0u;
	pDocument = xrtPlistParseMemory(sXml, sizeof(sXml) - 1u,
		&Options, &Error);
	if ( pDocument == NULL ) {
		fprintf(stderr, "plist zero-limit code: %d\n", Error.iCode);
	}
	bOk = pDocument != NULL && Error.iCode == XRT_PLIST_OK && bOk;
	xrtPlistDestroy(pDocument);
	if ( !bOk ) { fputs("plist read error classification failed\n", stderr); }
	return bOk;
}

typedef struct {
	xplisttestreader Reader;
	size_t Written;
	int Closed;
} xplistcloseprobe;

static bool testCloseProbeRead(ptr pContext, ptr pData,
	size_t iSize, size_t* pRead)
{
	xplistcloseprobe* pProbe = (xplistcloseprobe*)pContext;
	return testReadUnknown(&pProbe->Reader, pData, iSize, pRead);
}

static bool testCloseProbeWrite(ptr pContext, const void* pData,
	size_t iSize, size_t* pWritten)
{
	xplistcloseprobe* pProbe = (xplistcloseprobe*)pContext;
	(void)pData;
	pProbe->Written += iSize;
	*pWritten = iSize;
	return true;
}

static bool testCloseProbeMemoryWrite(ptr pContext, const void* pData,
	size_t iSize, size_t* pWritten)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
	*pWritten = 0u;
	xrtSetErrorInfo(XERR_MEMORY, "test.plist", 0,
		"injected write allocation failure");
	return false;
}

static bool testCloseProbeFail(ptr pContext)
{
	xplistcloseprobe* pProbe = (xplistcloseprobe*)pContext;
	++pProbe->Closed;
	xrtSetErrorInfo(XERR_IO, "test.plist", 0, "injected close failure");
	return false;
}

static int testStreamCloseFailures(void)
{
	static const char sXml[] = "<plist version=\"1.0\"><string>ok</string></plist>";
	xreaderops ReaderOps = {0};
	xwriterops WriterOps = {0};
	xplistcloseprobe ReadProbe = {0}, WriteProbe = {0};
	xrtplisterror Error;
	xrtplistoptions Options;
	xrtplistdocument* pDocument;
	xreader* pReader;
	xwriter* pWriter;
	int bOk = 1;

	ReaderOps.Read = testCloseProbeRead;
	ReaderOps.Close = testCloseProbeFail;
	ReadProbe.Reader.pData = (const uint8*)sXml;
	ReadProbe.Reader.iSize = sizeof(sXml) - 1u;
	pReader = xrtReaderCreate(&ReaderOps, &ReadProbe);
	if ( pReader == NULL ) { return 0; }
	pDocument = xrtPlistParse(pReader, true, NULL, &Error);
	bOk &= pDocument == NULL && Error.iCode == XRT_PLIST_ERROR_IO &&
		ReadProbe.Closed == 1 && ReadProbe.Reader.iOffset == ReadProbe.Reader.iSize;
	if ( !bOk ) { fputs("plist owned reader close failure was lost\n", stderr); }
	xrtPlistDestroy(pDocument);
	xrtClearError();

	ReadProbe.Reader.iOffset = 0u;
	ReadProbe.Closed = 0;
	pReader = xrtReaderCreate(&ReaderOps, &ReadProbe);
	if ( pReader == NULL ) { return 0; }
	pDocument = xrtPlistParse(pReader, false, NULL, &Error);
	bOk &= pDocument != NULL && Error.iCode == XRT_PLIST_OK && ReadProbe.Closed == 0;
	bOk &= !xrtReaderDestroy(pReader) && ReadProbe.Closed == 1;
	xrtClearError();

	WriterOps.Write = testCloseProbeWrite;
	WriterOps.Close = testCloseProbeFail;
	pWriter = xrtWriterCreate(&WriterOps, &WriteProbe);
	if ( pWriter == NULL ) { xrtPlistDestroy(pDocument); return 0; }
	xrtSetErrorInfo(XERR_MEMORY, "test.plist", 0, "stale memory error");
	bOk &= !xrtPlistWrite(pDocument, pWriter, true, NULL, &Error) &&
		Error.iCode == XRT_PLIST_ERROR_IO && WriteProbe.Written != 0u &&
		WriteProbe.Closed == 1;
	if ( !bOk ) { fputs("plist owned writer close failure was lost\n", stderr); }
	xrtClearError();

	WriteProbe.Written = 0u;
	WriteProbe.Closed = 0;
	pWriter = xrtWriterCreate(&WriterOps, &WriteProbe);
	if ( pWriter == NULL ) { xrtPlistDestroy(pDocument); return 0; }
	bOk &= xrtPlistWrite(pDocument, pWriter, false, NULL, &Error) &&
		Error.iCode == XRT_PLIST_OK && WriteProbe.Closed == 0;
	bOk &= !xrtWriterDestroy(pWriter) && WriteProbe.Closed == 1;
	xrtClearError();
	WriterOps.Write = testCloseProbeMemoryWrite;
	WriterOps.Close = NULL;
	pWriter = xrtWriterCreate(&WriterOps, NULL);
	if ( pWriter == NULL ) { xrtPlistDestroy(pDocument); return 0; }
	bOk &= !xrtPlistWrite(pDocument, pWriter, true, NULL, &Error) &&
		Error.iCode == XRT_PLIST_ERROR_MEMORY;
	xrtClearError();
	WriterOps.Write = testCloseProbeWrite;
	WriterOps.Close = testCloseProbeFail;
	xrtPlistDestroy(pDocument);

	/* 关闭也失败时，保留之前的格式或限额错误。 */
	ReadProbe.Reader.pData = (const uint8*)"<plist>";
	ReadProbe.Reader.iSize = sizeof("<plist>") - 1u;
	ReadProbe.Reader.iOffset = 0u;
	ReadProbe.Closed = 0;
	pReader = xrtReaderCreate(&ReaderOps, &ReadProbe);
	if ( pReader == NULL ) { return 0; }
	pDocument = xrtPlistParse(pReader, true, NULL, &Error);
	bOk &= pDocument == NULL && Error.iCode == XRT_PLIST_ERROR_FORMAT &&
		ReadProbe.Closed == 1;
	xrtPlistDestroy(pDocument);
	xrtClearError();

	pDocument = xrtPlistCreate();
	if ( pDocument == NULL || !xrtPlistSetRoot(pDocument,
		xrtPlistCreateString(pDocument, (str)"ok")) ) {
		xrtPlistDestroy(pDocument); return 0;
	}
	xrtPlistOptionsInit(&Options);
	Options.tLimits.iMaxOutputBytes = 3u;
	WriteProbe.Written = 0u;
	WriteProbe.Closed = 0;
	pWriter = xrtWriterCreate(&WriterOps, &WriteProbe);
	if ( pWriter == NULL ) { xrtPlistDestroy(pDocument); return 0; }
	bOk &= !xrtPlistWrite(pDocument, pWriter, true, &Options, &Error) &&
		Error.iCode == XRT_PLIST_ERROR_LIMIT && WriteProbe.Written == 0u &&
		WriteProbe.Closed == 1;
	xrtPlistDestroy(pDocument);
	xrtClearError();
	return bOk;
}

static int testInvalidOptions(void)
{
	static const char sXml[] = "<plist version=\"1.0\"><string>ok</string></plist>";
	xrtplistoptions Options;
	xrtplisterror Error;
	xrtplistdocument* pDocument;
	xbuffer* pOutput;
	int bOk = 1;

	xrtPlistOptionsInit(&Options);
	Options.tLimits.iMaxInputBytes = 3u;
	Options.iVersion++;
	pDocument = xrtPlistParseMemory(sXml, sizeof(sXml) - 1u,
		&Options, &Error);
	bOk &= pDocument == NULL && Error.iCode == XRT_PLIST_ERROR_ARGUMENT;
	xrtPlistDestroy(pDocument);
	Options.iVersion = XRT_PLIST_OPTIONS_VERSION;
	Options.iSize = sizeof(Options) - 1u;
	pDocument = xrtPlistParseMemory(sXml, sizeof(sXml) - 1u,
		&Options, &Error);
	bOk &= pDocument == NULL && Error.iCode == XRT_PLIST_ERROR_ARGUMENT;
	xrtPlistDestroy(pDocument);
	Options.iSize = sizeof(Options);
	Options.tLimits.iVersion++;
	pDocument = xrtPlistParseMemory(sXml, sizeof(sXml) - 1u,
		&Options, &Error);
	bOk &= pDocument == NULL && Error.iCode == XRT_PLIST_ERROR_ARGUMENT;
	xrtPlistDestroy(pDocument);
	Options.tLimits.iVersion = XRT_RESOURCE_LIMITS_VERSION;
	pDocument = xrtPlistParseMemory(sXml, sizeof(sXml) - 1u,
		&Options, &Error);
	bOk &= pDocument == NULL && Error.iCode == XRT_PLIST_ERROR_LIMIT;
	xrtPlistDestroy(pDocument);
	xrtPlistOptionsInit(&Options);
	Options.tLimits.iReserved = 1u;
	pDocument = xrtPlistParseMemory(sXml, sizeof(sXml) - 1u,
		&Options, &Error);
	bOk &= pDocument == NULL && Error.iCode == XRT_PLIST_ERROR_ARGUMENT;
	xrtPlistDestroy(pDocument);
	Options.tLimits.iReserved = 0u;
	Options.tLimits.iFlags = XRT_RESOURCE_ALLOW_EXTERNAL_ENTITIES;
	pDocument = xrtPlistParseMemory(sXml, sizeof(sXml) - 1u,
		&Options, &Error);
	bOk &= pDocument == NULL && Error.iCode == XRT_PLIST_ERROR_ARGUMENT;
	xrtPlistDestroy(pDocument);

	pDocument = xrtPlistCreate();
	if ( pDocument == NULL || !xrtPlistSetRoot(pDocument,
		xrtPlistCreateString(pDocument, (str)"ok")) ) {
		xrtPlistDestroy(pDocument);
		return 0;
	}
	xrtPlistOptionsInit(&Options);
	Options.tLimits.iMaxOutputBytes = 3u;
	Options.iVersion++;
	pOutput = xrtPlistStringify(pDocument, &Options, &Error);
	bOk &= pOutput == NULL && Error.iCode == XRT_PLIST_ERROR_ARGUMENT;
	xrtBufferDestroy(pOutput);
	Options.iVersion = XRT_PLIST_OPTIONS_VERSION;
	pOutput = xrtPlistStringify(pDocument, &Options, &Error);
	bOk &= pOutput == NULL && Error.iCode == XRT_PLIST_ERROR_LIMIT;
	xrtBufferDestroy(pOutput);
	xrtPlistOptionsInit(&Options);
	Options.tLimits.iReserved = 1u;
	pOutput = xrtPlistStringify(pDocument, &Options, &Error);
	bOk &= pOutput == NULL && Error.iCode == XRT_PLIST_ERROR_ARGUMENT;
	xrtBufferDestroy(pOutput);
	Options.tLimits.iReserved = 0u;
	Options.tLimits.iFlags = XRT_RESOURCE_ALLOW_EXTERNAL_ENTITIES;
	pOutput = xrtPlistStringify(pDocument, &Options, &Error);
	bOk &= pOutput == NULL && Error.iCode == XRT_PLIST_ERROR_ARGUMENT;
	xrtBufferDestroy(pOutput);
	xrtPlistDestroy(pDocument);
	return bOk;
}

int main(void)
{
	if ( !testFormat(XRT_PLIST_FORMAT_XML) ) { fputs("plist XML roundtrip failed\n", stderr); return 2; }
	if ( !testFormat(XRT_PLIST_FORMAT_BINARY) ) { fputs("plist binary roundtrip failed\n", stderr); return 2; }
	if ( !testIndentedXmlData() ) { fputs("plist XML data failed\n", stderr); return 2; }
	if ( !testXmlIntegerFailures() ) { fputs("plist XML integer failed\n", stderr); return 2; }
	if ( !testXmlScalarFailures() ) { fputs("plist XML scalar failed\n", stderr); return 2; }
	if ( !testXmlDocumentOwnership() ) { fputs("plist XML ownership failed\n", stderr); return 2; }
	if ( !testXmlRealLocale() ) { fputs("plist XML locale failed\n", stderr); return 2; }
	if ( !testBinaryDateUnit() ) { fputs("plist binary date unit failed\n", stderr); return 2; }
	if ( !testXmlDatePrecision() ) { fputs("plist XML date precision failed\n", stderr); return 2; }
	if ( !testBinaryDatePrecisionLimit() ) { fputs("plist binary date precision failed\n", stderr); return 2; }
	if ( !testBinaryIntegers() ) { fputs("plist binary integers failed\n", stderr); return 2; }
	if ( !testInvalidInputs() ) { fputs("plist invalid inputs failed\n", stderr); return 2; }
	if ( !testSaveFile() ) { fputs("plist atomic save failed\n", stderr); return 2; }
	if ( !testReadErrors() ) { fputs("plist read errors failed\n", stderr); return 2; }
	if ( !testSeekedReaderLimit() ) { fputs("plist seeked reader limit failed\n", stderr); return 2; }
	if ( !testStreamCloseFailures() ) { fputs("plist stream close failed\n", stderr); return 2; }
	if ( !testInvalidOptions() ) { fputs("plist invalid options failed\n", stderr); return 2; }
	puts("test_plist: PASS");
	return 0;
}

/* 单文件测试入口；包构建时由清单分别编译源码。 */
#include "../xrt_plist_tree.c"
#include "../xrt_plist_io.c"
#include "../xrt_plist_xml.c"
#include "../xrt_plist_binary.c"
#include "../xrt_plist_xml_writer.c"
#include "../xrt_plist_binary_writer.c"
#include "../../xml.dom/xrt_xml_dom_tree.c"
#include "../../xml.dom/xrt_xml_dom_parser.c"
#include "../../xml.dom/xrt_xml_dom_writer.c"
#include "../../xml.sax/xrt_xml_common.c"
#include "../../xml.sax/xrt_xml_sax.c"
#include "../../_expat/vendor/expat/lib/xmlparse.c"
#include "../../_expat/vendor/expat/lib/xmlrole.c"
#include "../../_expat/vendor/expat/lib/xmltok.c"
#include "../../_expat/expat_random.c"
