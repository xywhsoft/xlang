#include "../include/xrt_xml_dom.h"

#include <stdio.h>
#include <string.h>



static int testParseAndRoundTrip(void)
{
	const char* sXml = "<?app mode?><root xmlns:p=\"urn:test\" id=\"7\">head<p:item>one</p:item><!--note--></root>";
	xrtxmloptions tOptions;
	xrtxmldomwriteoptions tWrite;
	xrtxmlerror tError;
	xrtxmldomnode* pDocument;
	xrtxmldomnode* pRoot;
	xrtxmldomnode* pItem;
	xrtxmldomnode* pItemText;
	str sText;
	xbuffer pEncoded;
	int bOk;
	xrtXmlOptionsInit(&tOptions);
	pDocument = xrtXmlDomParseMemory(sXml, strlen(sXml), &tOptions, &tError);
	pRoot = pDocument != NULL ? xrtXmlDomDocumentElement(pDocument) : NULL;
	pItem = pRoot != NULL ? xrtXmlDomNodeChild(pRoot, 1u) : NULL;
	pItemText = pItem != NULL ? xrtXmlDomNodeChild(pItem, 0u) : NULL;
	sText = pRoot != NULL ? xrtXmlDomTextContent(pRoot) : NULL;
	bOk = pDocument != NULL && pRoot != NULL && pItem != NULL &&
		xrtXmlDomNodeType(pDocument) == XRT_XML_DOM_DOCUMENT_NODE &&
		xrtXmlDomNodeChildCount(pDocument) == 2u &&
		strcmp((const char*)xrtXmlDomNodeName(pItem), "p:item") == 0 &&
		strcmp((const char*)xrtXmlDomNodeNamespaceUri(pItem), "urn:test") == 0 &&
		strcmp((const char*)xrtXmlDomGetAttribute(pRoot, (str)"id"), "7") == 0 &&
		sText != NULL && strcmp((const char*)sText, "headone") == 0;
	xrtFree(sText);
	bOk = bOk && pItemText != NULL &&
		xrtXmlDomSetAttribute(pItem, (str)"quote", (str)"a\"b") &&
		xrtXmlDomNodeSetValue(pItemText, (str)"changed & safe");
	xrtXmlDomWriteOptionsInit(&tWrite);
	tWrite.bPretty = TRUE;
	tWrite.bXmlDeclaration = TRUE;
	pEncoded = bOk ? xrtXmlDomStringify(pDocument, &tWrite, &tError) : NULL;
	bOk = bOk && pEncoded != NULL &&
		xrtBufferAppend(pEncoded, "\0", 1u, XBUF_BINARY) &&
		strstr((const char*)pEncoded->Buffer, "xmlns:p=\"urn:test\"") != NULL &&
		strstr((const char*)pEncoded->Buffer, "changed &amp; safe") != NULL &&
		strstr((const char*)pEncoded->Buffer, "quote=\"a&quot;b\"") != NULL;
	xrtBufferDestroy(pEncoded);
	xrtXmlDomNodeRelease(pItemText);
	xrtXmlDomNodeRelease(pItem);
	xrtXmlDomNodeRelease(pRoot);
	xrtXmlDomNodeRelease(pDocument);
	return bOk;
}

static int testConstructionAndMutation(void)
{
	xrtxmldomnode* pDocument = xrtXmlDomCreateDocument();
	xrtxmldomnode* pRoot = xrtXmlDomCreateElement(pDocument, (str)"root");
	xrtxmldomnode* pLeft = xrtXmlDomCreateElement(pDocument, (str)"left");
	xrtxmldomnode* pRight = xrtXmlDomCreateElement(pDocument, (str)"right");
	xrtxmldomnode* pText = xrtXmlDomCreateText(pDocument, (str)"value");
	xrtxmldomnode* pSecondRoot = xrtXmlDomCreateElement(pDocument, (str)"other");
	xrtxmldomnode* pParent;
	int bOk = pDocument != NULL && pRoot != NULL && pLeft != NULL &&
		pRight != NULL && pText != NULL && pSecondRoot != NULL;
	bOk = bOk && xrtXmlDomAppendChild(pDocument, pRoot) &&
		xrtXmlDomAppendChild(pRoot, pLeft) &&
		xrtXmlDomAppendChild(pRoot, pRight) &&
		xrtXmlDomAppendChild(pLeft, pText);
	/* 节点移动保持身份，不复制节点。 */
	bOk = bOk && xrtXmlDomAppendChild(pRight, pText) &&
		xrtXmlDomNodeChildCount(pLeft) == 0u &&
		xrtXmlDomNodeChildCount(pRight) == 1u &&
		!xrtXmlDomAppendChild(pLeft, pRoot) &&
		!xrtXmlDomAppendChild(pDocument, pSecondRoot);
	pParent = xrtXmlDomNodeParent(pRoot);
	bOk = bOk && pParent == pDocument;
	xrtXmlDomNodeRelease(pParent);
	xrtXmlDomNodeRelease(pSecondRoot);
	xrtXmlDomNodeRelease(pText);
	xrtXmlDomNodeRelease(pRight);
	xrtXmlDomNodeRelease(pLeft);
	xrtXmlDomNodeRelease(pRoot);
	xrtXmlDomNodeRelease(pDocument);
	return bOk;
}

static int testInvalidInputs(void)
{
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	xrtxmldomnode* pDocument;
	int bOk;
	xrtXmlOptionsInit(&tOptions);
	pDocument = xrtXmlDomParseMemory("<root><x></root>",
		strlen("<root><x></root>"), &tOptions, &tError);
	bOk = pDocument == NULL && tError.iCode == XRT_XML_ERROR_FORMAT;
	xrtXmlDomNodeRelease(pDocument);
	tOptions.tLimits.iMaxDepth = 2u;
	pDocument = xrtXmlDomParseMemory("<a><b><c/></b></a>",
		strlen("<a><b><c/></b></a>"), &tOptions, &tError);
	bOk = bOk && pDocument == NULL && tError.iCode == XRT_XML_ERROR_LIMIT;
	xrtXmlDomNodeRelease(pDocument);
	return bOk;
}

int main(void)
{
	int bOk;
	if ( xrtInit() == NULL ) { return 1; }
	bOk = testParseAndRoundTrip() && testConstructionAndMutation() &&
		testInvalidInputs();
	xrtUnit();
	if ( bOk ) { puts("test_xml_dom: PASS"); }
	return bOk ? 0 : 2;
}

/* 单文件测试入口；包导入时由清单分别编译源码。 */
#include "../xrt_xml_dom_tree.c"
#include "../xrt_xml_dom_parser.c"
#include "../xrt_xml_dom_writer.c"
#include "../../xml.sax/xrt_xml_common.c"
#include "../../xml.sax/xrt_xml_sax.c"
#include "../../_expat/vendor/expat/lib/xmlparse.c"
#include "../../_expat/vendor/expat/lib/xmlrole.c"
#include "../../_expat/vendor/expat/lib/xmltok.c"
#include "../../_expat/expat_random.c"
