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
	xbuffer pEncoded;
	xrtplistdocument* pParsed;
	xrtplistnode* pParsedRoot;
	int bOk = pDocument != NULL && pRoot != NULL && pArray != NULL;
	bOk = bOk && xrtPlistSetRoot(pDocument, pRoot) &&
		xrtPlistDictSet(pRoot, "name", 4u,
			xrtPlistCreateString(pDocument, (str)"xlang 你好")) &&
		xrtPlistDictSet(pRoot, "data", 4u,
			xrtPlistCreateData(pDocument, sData, sizeof(sData))) &&
		xrtPlistArrayAppend(pArray, xrtPlistCreateInt(pDocument, -7)) &&
		xrtPlistArrayAppend(pArray, xrtPlistCreateBool(pDocument, TRUE)) &&
		xrtPlistDictSet(pRoot, "items", 5u, pArray);
	if ( iFormat == XRT_PLIST_FORMAT_BINARY ) {
		bOk = bOk && xrtPlistDictSet(pRoot, "uid", 3u,
			xrtPlistCreateUid(pDocument, UINT64_MAX)) &&
			xrtPlistDictSet(pRoot, "none", 4u,
				xrtPlistCreateNull(pDocument));
	}
	xrtPlistOptionsInit(&tOptions);
	tOptions.iFormat = iFormat;
	tOptions.bPretty = TRUE;
	tOptions.bSortKeys = TRUE;
	pEncoded = bOk ? xrtPlistStringify(pDocument, &tOptions, &tError) : NULL;
	pParsed = pEncoded != NULL ? xrtPlistParseMemory(pEncoded->Buffer,
		pEncoded->Length, NULL, &tError) : NULL;
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
		bOk = bOk && xrtBufferAppend(pEncoded, "\0", 1u, XBUF_BINARY) &&
			strstr((const char*)pEncoded->Buffer,
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
	if ( xrtInit() == NULL ) { return 1; }
	bOk = testFormat(XRT_PLIST_FORMAT_XML) &&
		testFormat(XRT_PLIST_FORMAT_BINARY) && testIndentedXmlData() &&
		testInvalidInputs();
	xrtUnit();
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
