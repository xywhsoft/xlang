/* 独立 C 测试和包导入使用相同的完整 XRT 类型表。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_xml_etree.h"

#include <stdio.h>
#include <string.h>



int main(void)
{
	const char* sXml = "<root xmlns:p=\"urn:test\">head<p:item id=\"1\">one</p:item>tail<!--note--></root>";
	xrtxmloptions tOptions;
	xrtxmletreewriteoptions tWrite;
	xrtxmlerror tError;
	xrtxmlelement* pRoot;
	xrtxmlelement* pItem;
	xrtxmlelement* pInvalid;
	xbuffer* pText;
	int bOk;
	xrtXmlOptionsInit(&tOptions);
	pRoot = xrtXmlElementParseMemory(sXml, strlen(sXml), &tOptions, &tError);
	pItem = pRoot != NULL ? xrtXmlElementFind(pRoot, (str)"p:item") : NULL;
	bOk = pRoot != NULL && pItem != NULL &&
		strcmp((const char*)xrtXmlElementText(pRoot), "head") == 0 &&
		strcmp((const char*)xrtXmlElementTail(pItem), "tail") == 0 &&
		strcmp((const char*)xrtXmlElementNamespaceUri(pItem), "urn:test") == 0;
	bOk = bOk && xrtXmlElementSetText(pItem, (str)"changed & safe") &&
		xrtXmlElementSetAttribute(pItem, (str)"quote", (str)"a\"b") &&
		xrtXmlElementChildCount(pRoot) == 2u;
	xrtXmlElementWriteOptionsInit(&tWrite);
	pText = bOk ? xrtXmlElementStringify(pRoot, &tWrite, &tError) : NULL;
	bOk = bOk && pText != NULL && xrtBufferAppendByte(pText, 0u) &&
		strstr((const char*)pText->Data, "xmlns:p=\"urn:test\"") != NULL &&
		strstr((const char*)pText->Data, "changed &amp; safe") != NULL;
	pInvalid = xrtXmlElementParseMemory("<root><x></root>",
		strlen("<root><x></root>"), &tOptions, &tError);
	bOk = bOk && pInvalid == NULL && tError.iCode == XRT_XML_ERROR_FORMAT;
	xrtXmlElementRelease(pInvalid);
	tOptions.tLimits.iMaxDepth = 2u;
	pInvalid = xrtXmlElementParseMemory("<a><b><c/></b></a>",
		strlen("<a><b><c/></b></a>"), &tOptions, &tError);
	bOk = bOk && pInvalid == NULL && tError.iCode == XRT_XML_ERROR_LIMIT;
	xrtXmlElementRelease(pInvalid);
	xrtBufferDestroy(pText);
	xrtXmlElementRelease(pItem);
	xrtXmlElementRelease(pRoot);
	if ( bOk ) { puts("test_xml_etree: PASS"); }
	else { fprintf(stderr, "test_xml_etree: %s\n", tError.sMessage); }
	return bOk ? 0 : 2;
}

/* 单文件测试入口；包导入时由清单分别编译源码。 */
#include "../xrt_xml_etree_tree.c"
#include "../xrt_xml_etree_parser.c"
#include "../xrt_xml_etree_writer.c"
#include "../../xml.sax/xrt_xml_common.c"
#include "../../xml.sax/xrt_xml_sax.c"
#include "../../_expat/vendor/expat/lib/xmlparse.c"
#include "../../_expat/vendor/expat/lib/xmlrole.c"
#include "../../_expat/vendor/expat/lib/xmltok.c"
#include "../../_expat/expat_random.c"
