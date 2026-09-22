/* 独立 C 测试和包导入使用相同的完整 XRT 类型表。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_plist.h"

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



/* 用外部规范字节验证 binary plist 日期的秒单位，避免同源读写互相掩盖错误。 */
static int testBinaryDateUnit(void)
{
	static const uint8 sExpectedObject[] = {
		0x33u, 0x3fu, 0xf0u, 0u, 0u, 0u, 0u, 0u, 0u
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
	return bOk;
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
	bOk = bOk && pDocument == NULL && tError.iCode == XRT_PLIST_ERROR_FORMAT;
	xrtPlistDestroy(pDocument);
	return bOk;
}

int main(void)
{
	int bOk;
	bOk = testFormat(XRT_PLIST_FORMAT_XML) &&
		testFormat(XRT_PLIST_FORMAT_BINARY) && testIndentedXmlData() &&
		testBinaryDateUnit() && testInvalidInputs();
	if ( bOk ) { puts("test_plist: PASS"); }
	return bOk ? 0 : 2;
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
