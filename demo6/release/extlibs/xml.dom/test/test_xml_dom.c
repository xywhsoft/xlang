/* 测试端启用完整声明；包实现和独立运行时按需裁剪。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_xml_dom.h"

#include <stdio.h>
#include <string.h>


/* 逐点失败后整棵未完成的 DOM 必须释放，下一次解析仍可成功。 */
static int testParseAllocationFailures(void)
{
	const char* sXml = "<root xmlns:p=\"urn:test\" id=\"7\">"
		"head<p:item name=\"one\">text</p:item><!--note--></root>";
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	xmemdebugsnapshot tBefore;
	xmemdebugsnapshot tAfter;
	unsigned int iFailure;
	bool bCompleted = false;

	if ( !xrtMemDebugEnabled() && !xrtMemDebugEnable(true) ) { return 0; }
	xrtXmlOptionsInit(&tOptions);
	if ( !xrtMemDebugFailAfter(1000u) ) { return 0; }
	xrtMemDebugFailClear();
	xrtClearError();
	xrtMemDebugSnapshot(&tBefore);
	for ( iFailure = 0u; iFailure < 256u; iFailure++ ) {
		xrtxmldomnode* pDocument;
		bool bTriggered;
		if ( !xrtMemDebugFailAfter(iFailure) ) { return 0; }
		pDocument = xrtXmlDomParseMemory(sXml, strlen(sXml),
			&tOptions, &tError);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) {
			if ( pDocument != NULL || tError.iCode != XRT_XML_ERROR_MEMORY ) {
				fprintf(stderr, "DOM allocation failure %u returned code %d\n",
					iFailure, tError.iCode);
				xrtXmlDomNodeRelease(pDocument);
				return 0;
			}
		} else {
			if ( pDocument == NULL || tError.iCode != XRT_XML_OK ) {
				xrtXmlDomNodeRelease(pDocument);
				return 0;
			}
			bCompleted = true;
		}
		xrtXmlDomNodeRelease(pDocument);
		xrtClearError();
		xrtMemDebugSnapshot(&tAfter);
		if ( tAfter.LiveCount != tBefore.LiveCount ||
			tAfter.LiveBytes != tBefore.LiveBytes ) {
			fprintf(stderr, "DOM allocation failure %u leaked: "
				"live %zu -> %zu, bytes %zu -> %zu\n", iFailure,
				tBefore.LiveCount, tAfter.LiveCount,
				tBefore.LiveBytes, tAfter.LiveBytes);
			return 0;
		}
		if ( bCompleted ) { break; }
	}
	return bCompleted && iFailure > 0u;
}


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
	xrtxmldomnode* pRoundTrip = NULL;
	xrtxmldomnode* pRoundRoot = NULL;
	xrtxmldomnode* pRoundItem = NULL;
	xrtxmldomnode* pRoundText = NULL;
	str sText;
	xbuffer* pEncoded;
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
		xrtXmlDomSetAttribute(pItem, (str)"space", (str)"A\nB\tC\rD") &&
		xrtXmlDomNodeSetValue(pItemText, (str)"changed & safe\rline");
	xrtXmlDomWriteOptionsInit(&tWrite);
	tWrite.bPretty = true;
	tWrite.bXmlDeclaration = true;
	pEncoded = bOk ? xrtXmlDomStringify(pDocument, &tWrite, &tError) : NULL;
	bOk = bOk && pEncoded != NULL &&
		xrtBufferAppendByte(pEncoded, 0u) &&
		strstr((const char*)pEncoded->Data, "xmlns:p=\"urn:test\"") != NULL &&
		strstr((const char*)pEncoded->Data, "head<p:item") != NULL &&
		strstr((const char*)pEncoded->Data,
			"</p:item><!--note-->") != NULL &&
		strstr((const char*)pEncoded->Data, "changed &amp; safe&#xD;line") != NULL &&
		strstr((const char*)pEncoded->Data, "quote=\"a&quot;b\"") != NULL &&
		strstr((const char*)pEncoded->Data,
			"space=\"A&#xA;B&#x9;C&#xD;D\"") != NULL;
	pRoundTrip = bOk ? xrtXmlDomParseMemory(pEncoded->Data,
		pEncoded->Size - 1u, &tOptions, &tError) : NULL;
	pRoundRoot = pRoundTrip != NULL ? xrtXmlDomDocumentElement(pRoundTrip) : NULL;
	pRoundItem = pRoundRoot != NULL ? xrtXmlDomNodeChild(pRoundRoot, 1u) : NULL;
	pRoundText = pRoundItem != NULL ? xrtXmlDomNodeChild(pRoundItem, 0u) : NULL;
	bOk = bOk && pRoundText != NULL &&
		strcmp((const char*)xrtXmlDomGetAttribute(pRoundItem, (str)"space"),
			"A\nB\tC\rD") == 0 &&
		strcmp((const char*)xrtXmlDomNodeValue(pRoundText),
			"changed & safe\rline") == 0;
	xrtXmlDomNodeRelease(pRoundText);
	xrtXmlDomNodeRelease(pRoundItem);
	xrtXmlDomNodeRelease(pRoundRoot);
	xrtXmlDomNodeRelease(pRoundTrip);
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
	bool bAllocated;
	xrtXmlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 4u;
	if ( !xrtMemDebugEnabled() && !xrtMemDebugEnable(true) ) { return 0; }
	if ( !xrtMemDebugFailAfter(0u) ) { return 0; }
	pDocument = xrtXmlDomParseMemory("<root/>", 7u, &tOptions, &tError);
	bAllocated = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	bOk = pDocument == NULL && tError.iCode == XRT_XML_ERROR_LIMIT &&
		!bAllocated;
	xrtXmlDomNodeRelease(pDocument);
	pDocument = xrtXmlDomParseMemory("<root/>", UINT64_MAX, &tOptions, &tError);
	bOk = bOk && pDocument == NULL && tError.iCode == XRT_XML_ERROR_LIMIT;
	xrtXmlDomNodeRelease(pDocument);
	xrtXmlOptionsInit(&tOptions);
	pDocument = xrtXmlDomParseMemory("<root><x></root>",
		strlen("<root><x></root>"), &tOptions, &tError);
	bOk = bOk && pDocument == NULL && tError.iCode == XRT_XML_ERROR_FORMAT;
	xrtXmlDomNodeRelease(pDocument);
	tOptions.tLimits.iMaxDepth = 2u;
	pDocument = xrtXmlDomParseMemory("<a><b><c/></b></a>",
		strlen("<a><b><c/></b></a>"), &tOptions, &tError);
	bOk = bOk && pDocument == NULL && tError.iCode == XRT_XML_ERROR_LIMIT;
	xrtXmlDomNodeRelease(pDocument);
	xrtXmlOptionsInit(&tOptions);
	{
		static const char sDtd[] =
			"<!DOCTYPE root [<!ENTITY value 'secret'>]>"
			"<root>&value;</root>";
		pDocument = xrtXmlDomParseMemory(sDtd, sizeof(sDtd) - 1u,
			&tOptions, &tError);
		bOk = bOk && pDocument == NULL && tError.iCode == XRT_XML_ERROR_DTD;
		xrtXmlDomNodeRelease(pDocument);
	}
	{
		static const char sExternal[] =
			"<!DOCTYPE root SYSTEM \"file:///xlang-extlib-no-such-entity\"><root/>";
		pDocument = xrtXmlDomParseMemory(sExternal, sizeof(sExternal) - 1u,
			&tOptions, &tError);
		bOk = bOk && pDocument == NULL && tError.iCode == XRT_XML_ERROR_DTD;
		xrtXmlDomNodeRelease(pDocument);
	}
	{
		static const char sEntity[] =
			"<!DOCTYPE root [<!ENTITY x SYSTEM \"file:///xlang-extlib-no-such-entity\">]><root>&x;</root>";
		pDocument = xrtXmlDomParseMemory(sEntity, sizeof(sEntity) - 1u,
			&tOptions, &tError);
		bOk = bOk && pDocument == NULL && tError.iCode == XRT_XML_ERROR_DTD;
		xrtXmlDomNodeRelease(pDocument);
	}
	return bOk;
}

static int testZeroLimits(void)
{
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	xrtxmldomnode* pDocument;
	int bOk;
	xrtXmlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 0u;
	tOptions.tLimits.iMaxOutputBytes = 0u;
	tOptions.tLimits.iMaxItemBytes = 0u;
	tOptions.tLimits.iMaxEntries = 0u;
	tOptions.tLimits.iMaxNodes = 0u;
	tOptions.tLimits.iMaxDepth = 0u;
	pDocument = xrtXmlDomParseMemory("<root>ok</root>", 15u,
		&tOptions, &tError);
	bOk = pDocument != NULL && tError.iCode == XRT_XML_OK;
	xrtXmlDomNodeRelease(pDocument);
	return bOk;
}

static int testWriterBoundaries(void)
{
	const char* sPath = "xrt_xml_dom_writer_boundary.xml";
	xrtxmldomwriteoptions tWrite;
	xrtxmlerror tError;
	xrtxmldomnode* pDocument = xrtXmlDomCreateDocument();
	xrtxmldomnode* pRoot = pDocument != NULL ?
		xrtXmlDomCreateElement(pDocument, (str)"root") : NULL;
	xrtxmldomnode* pText = pDocument != NULL ?
		xrtXmlDomCreateText(pDocument, (str)"ok") : NULL;
	xrtxmldomnode* pUnicode = NULL;
	xrtxmldomnode* pComment = NULL;
	xrtxmldomnode* pInstruction = NULL;
	xbuffer* pEncoded;
	bytes pSaved;
	size_t iSaved = 0u;
	int bOk = pDocument != NULL && pRoot != NULL && pText != NULL;
	bOk = bOk && xrtXmlDomAppendChild(pDocument, pRoot) &&
		xrtXmlDomAppendChild(pRoot, pText);
	if ( bOk ) {
		int bArguments = !xrtXmlDomWrite(pDocument, NULL, false, NULL,
			&tError) && tError.iCode == XRT_XML_ERROR_ARGUMENT;
		bArguments = !xrtXmlDomSave(NULL, pDocument, NULL, &tError) &&
			tError.iCode == XRT_XML_ERROR_ARGUMENT && bArguments;
		bArguments = !xrtXmlDomSave((str)"", pDocument, NULL, &tError) &&
			tError.iCode == XRT_XML_ERROR_ARGUMENT && bArguments;
		if ( !bArguments ) { fputs("DOM output arguments were misclassified\n", stderr); }
		bOk = bArguments;
	}
	pUnicode = xrtXmlDomCreateElement(pDocument,
		(str)"\xe4\xb8\xad\xe6\x96\x87");
	bOk = bOk && xrtXmlDomCreateElement(pDocument, (str)"1root") == NULL &&
		!xrtXmlDomSetAttribute(pRoot, (str)"bad?", (str)"value") &&
		pUnicode != NULL;
	xrtXmlDomWriteOptionsInit(&tWrite);
	tWrite.iMaxOutputBytes = 1u;
	pEncoded = bOk ? xrtXmlDomStringify(pDocument, &tWrite, &tError) : NULL;
	bOk = bOk && pEncoded == NULL && tError.iCode == XRT_XML_ERROR_LIMIT;
	xrtBufferDestroy(pEncoded);
	xrtXmlDomWriteOptionsInit(&tWrite);
	bOk = bOk && xrtXmlDomNodeSetValue(pText, (str)"bad \xef\xbf\xbe");
	pEncoded = bOk ? xrtXmlDomStringify(pDocument, &tWrite, &tError) : NULL;
	bOk = bOk && pEncoded == NULL && tError.iCode == XRT_XML_ERROR_FORMAT;
	xrtBufferDestroy(pEncoded);
	bOk = bOk && xrtFileWriteAll(sPath,
		(xbytesview){ (cbytes)"old", 3u });
	bOk = bOk && !xrtXmlDomSave((str)sPath, pDocument, &tWrite, &tError) &&
		tError.iCode == XRT_XML_ERROR_FORMAT;
	pSaved = xrtFileReadAll(sPath, &iSaved);
	bOk = bOk && pSaved != NULL && iSaved == 3u &&
		memcmp(pSaved, "old", 3u) == 0;
	xrtFree(pSaved);
	bOk = bOk && xrtXmlDomNodeSetValue(pText, (str)"ok");
	bOk = bOk && xrtXmlDomSave((str)sPath, pDocument, &tWrite,
		&tError);
	pSaved = xrtFileReadAll(sPath, &iSaved);
	bOk = bOk && pSaved != NULL && iSaved >= 6u &&
		memcmp(pSaved, "<root>", 6u) == 0;
	xrtFree(pSaved);
	if ( bOk ) {
		xrtxmldomnode* pLoaded;
		bool bTriggered;
		if ( !xrtMemDebugFailAfter(0u) ) { bOk = 0; }
		else {
			pLoaded = xrtXmlDomLoad((str)sPath, NULL, &tError);
			bTriggered = xrtMemDebugFailTriggered();
			xrtMemDebugFailClear();
			bOk = bOk && bTriggered && pLoaded == NULL &&
				tError.iCode == XRT_XML_ERROR_MEMORY;
			xrtXmlDomNodeRelease(pLoaded);
			xrtClearError();
			if ( bOk ) {
				pLoaded = xrtXmlDomLoad((str)sPath, NULL, &tError);
				bOk = pLoaded != NULL && tError.iCode == XRT_XML_OK;
				xrtXmlDomNodeRelease(pLoaded);
			}
		}
		if ( !bOk ) { fputs("DOM file open memory failure was misclassified\n", stderr); }
	}
	if ( bOk ) {
		unsigned int iFailure;
		bool bCompleted = false;
		for ( iFailure = 0u; iFailure < 64u; ++iFailure ) {
			bool bSaved, bTriggered;
			if ( !xrtMemDebugFailAfter(iFailure) ) { bOk = 0; break; }
			bSaved = xrtXmlDomSave((str)sPath, pDocument, &tWrite, &tError);
			bTriggered = xrtMemDebugFailTriggered();
			xrtMemDebugFailClear();
			if ( bTriggered ? (bSaved || tError.iCode != XRT_XML_ERROR_MEMORY) :
				(!bSaved || tError.iCode != XRT_XML_OK) ) {
				fprintf(stderr, "DOM save allocation %u misclassified: %d\n",
					iFailure, tError.iCode);
				bOk = 0;
				break;
			}
			xrtClearError();
			if ( !bTriggered ) { bCompleted = true; break; }
		}
		bOk = bOk && bCompleted;
	}
	(void)xrtFileDelete(sPath);
	pComment = xrtXmlDomCreateComment(pDocument, (str)"ok");
	bOk = bOk && pComment != NULL && xrtXmlDomAppendChild(pRoot, pComment) &&
		xrtXmlDomNodeSetValue(pComment, (str)"bad-");
	pEncoded = bOk ? xrtXmlDomStringify(pDocument, &tWrite, &tError) : NULL;
	bOk = bOk && pEncoded == NULL && tError.iCode == XRT_XML_ERROR_FORMAT;
	xrtBufferDestroy(pEncoded);
	bOk = bOk && xrtXmlDomNodeSetValue(pComment, (str)"ok");
	pInstruction = xrtXmlDomCreateProcessingInstruction(pDocument,
		(str)"xml", (str)"bad");
	bOk = bOk && pInstruction != NULL &&
		xrtXmlDomAppendChild(pRoot, pInstruction);
	pEncoded = bOk ? xrtXmlDomStringify(pDocument, &tWrite, &tError) : NULL;
	bOk = bOk && pEncoded == NULL && tError.iCode == XRT_XML_ERROR_FORMAT;
	xrtBufferDestroy(pEncoded);
	xrtXmlDomNodeRelease(pInstruction);
	xrtXmlDomNodeRelease(pComment);
	xrtXmlDomNodeRelease(pUnicode);
	xrtXmlDomNodeRelease(pText);
	xrtXmlDomNodeRelease(pRoot);
	xrtXmlDomNodeRelease(pDocument);
	return bOk;
}

typedef struct {
	const char* Text;
	size_t Offset;
	int Closed;
} dom_close_reader;

static bool domReadForClose(ptr pContext, ptr pData, size_t iSize, size_t* pRead)
{
	dom_close_reader* pProbe = (dom_close_reader*)pContext;
	size_t iRemain = strlen(pProbe->Text) - pProbe->Offset;
	*pRead = iSize < iRemain ? iSize : iRemain;
	if ( *pRead != 0u ) { memcpy(pData, pProbe->Text + pProbe->Offset, *pRead); }
	pProbe->Offset += *pRead;
	return true;
}

static bool domFailClose(ptr pContext)
{
	dom_close_reader* pProbe = (dom_close_reader*)pContext;
	pProbe->Closed++;
	xrtSetErrorInfo(XERR_IO, "test.xml.dom", 0, "injected close failure");
	return false;
}

static bool domWriteProbe(ptr pContext, const void* pData,
	size_t iSize, size_t* pWritten)
{
	dom_close_reader* pProbe = (dom_close_reader*)pContext;
	(void)pData;
	pProbe->Offset += iSize;
	*pWritten = iSize;
	return true;
}

static bool domWriteMemoryFailure(ptr pContext, const void* pData,
	size_t iSize, size_t* pWritten)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
	*pWritten = 0u;
	xrtSetErrorInfo(XERR_MEMORY, "test.xml.dom", 0,
		"injected write allocation failure");
	return false;
}

static bool testWriteStreamFailures(void)
{
	xwriterops tOps = {0};
	dom_close_reader tProbe = { "", 0u, 0 };
	xrtxmlerror tError;
	xrtxmldomnode* pDocument = xrtXmlDomCreateDocument();
	xrtxmldomnode* pRoot = pDocument != NULL ?
		xrtXmlDomCreateElement(pDocument, (str)"root") : NULL;
	xwriter* pWriter;
	bool bOk = pRoot != NULL && xrtXmlDomAppendChild(pDocument, pRoot);
	if ( !bOk ) {
		xrtXmlDomNodeRelease(pRoot);
		xrtXmlDomNodeRelease(pDocument);
		return false;
	}
	tOps.Write = domWriteProbe;
	tOps.Close = domFailClose;
	pWriter = xrtWriterCreate(&tOps, &tProbe);
	bOk = pWriter != NULL;
	if ( bOk ) {
		xrtSetErrorInfo(XERR_MEMORY, "test.xml.dom", 0, "stale memory error");
		bOk = !xrtXmlDomWrite(pDocument, pWriter, true, NULL, &tError) &&
			tError.iCode == XRT_XML_ERROR_IO && tProbe.Offset != 0u &&
			tProbe.Closed == 1;
	}
	xrtClearError();
	tOps.Write = domWriteMemoryFailure;
	tOps.Close = NULL;
	pWriter = xrtWriterCreate(&tOps, NULL);
	bOk = bOk && pWriter != NULL;
	if ( pWriter != NULL ) {
		bOk = !xrtXmlDomWrite(pDocument, pWriter, true, NULL, &tError) &&
			tError.iCode == XRT_XML_ERROR_MEMORY && bOk;
	}
	xrtClearError();
	xrtXmlDomNodeRelease(pRoot);
	xrtXmlDomNodeRelease(pDocument);
	return bOk;
}

static bool testParseCloseFailure(void)
{
	xreaderops tOps = {0};
	dom_close_reader tProbe = { "<root/>", 0u, 0 };
	xrtxmlerror tError;
	xreader* pReader;
	xrtxmldomnode* pDocument;
	tOps.Read = domReadForClose;
	tOps.Close = domFailClose;
	pReader = xrtReaderCreate(&tOps, &tProbe);
	if ( pReader == NULL ) { return false; }
	pDocument = xrtXmlDomParse(pReader, true, NULL, &tError);
	return pDocument == NULL && tError.iCode == XRT_XML_ERROR_IO &&
		tProbe.Closed == 1 && tProbe.Offset == strlen(tProbe.Text);
}

static int testDeepTreeAllocations(const xrtxmldomnode* pDocument)
{
	xmemdebugsnapshot tBefore;
	xmemdebugsnapshot tAfter;
	xrtxmlerror tError;
	unsigned int iFailure;
	int bWriterCompleted = 0;
	int bTextCompleted = 0;
	if ( !xrtMemDebugEnabled() && !xrtMemDebugEnable(true) ) { return 0; }
	xrtClearError();
	xrtMemDebugSnapshot(&tBefore);
	for ( iFailure = 0u; iFailure < 128u; iFailure++ ) {
		xbuffer* pEncoded;
		str sText;
		bool bWriterFault;
		bool bTextFault;
		if ( !xrtMemDebugFailAfter(iFailure) ) { return 0; }
		pEncoded = xrtXmlDomStringify(pDocument, NULL, &tError);
		bWriterFault = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pEncoded == NULL ) {
			if ( tError.iCode != XRT_XML_ERROR_MEMORY ) { return 0; }
		} else { bWriterCompleted = 1; }
		xrtBufferDestroy(pEncoded);
		if ( !xrtMemDebugFailAfter(iFailure) ) { return 0; }
		sText = xrtXmlDomTextContent(pDocument);
		bTextFault = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( sText != NULL ) { bTextCompleted = 1; }
		xrtFree(sText);
		xrtClearError();
		xrtMemDebugSnapshot(&tAfter);
		if ( tAfter.LiveCount != tBefore.LiveCount ||
			tAfter.LiveBytes != tBefore.LiveBytes ) {
			fprintf(stderr, "deep DOM allocation failure %u leaked: "
				"live %zu -> %zu, bytes %zu -> %zu\n", iFailure,
				tBefore.LiveCount, tAfter.LiveCount,
				tBefore.LiveBytes, tAfter.LiveBytes);
			return 0;
		}
		if ( bWriterCompleted && bTextCompleted &&
			!bWriterFault && !bTextFault ) { break; }
	}
	return bWriterCompleted && bTextCompleted;
}

/* 解析允许调用方提高深度；读取和写出不能因此耗尽 C 调用栈。 */
static int testDeepTree(void)
{
	const uint32 iDepth = 8192u;
	xbuffer* pXml = xrtBufferCreate();
	xbuffer* pEncoded = NULL;
	xrtxmloptions tOptions;
	xrtxmldomwriteoptions tWrite;
	xrtxmlerror tError;
	xrtxmldomnode* pDocument = NULL;
	str sText = NULL;
	uint32 i;
	int bOk = pXml != NULL;
	for ( i = 0u; bOk && i < iDepth; i++ ) {
		bOk = xrtBufferAppend(pXml, (xbytesview){ (cbytes)"<a>", 3u });
	}
	bOk = bOk && xrtBufferAppendByte(pXml, (uint8)'v');
	for ( i = 0u; bOk && i < iDepth; i++ ) {
		bOk = xrtBufferAppend(pXml, (xbytesview){ (cbytes)"</a>", 4u });
	}
	xrtXmlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxDepth = iDepth + 1u;
	pDocument = bOk ? xrtXmlDomParseMemory(pXml->Data, pXml->Size,
		&tOptions, &tError) : NULL;
	bOk = bOk && pDocument != NULL && tError.iCode == XRT_XML_OK;
	sText = bOk ? xrtXmlDomTextContent(pDocument) : NULL;
	bOk = bOk && sText != NULL && strcmp((const char*)sText, "v") == 0;
	pEncoded = bOk ? xrtXmlDomStringify(pDocument, NULL, &tError) : NULL;
	bOk = bOk && pEncoded != NULL && tError.iCode == XRT_XML_OK &&
		pEncoded->Size == pXml->Size &&
		memcmp(pEncoded->Data, pXml->Data, pXml->Size) == 0;
	xrtBufferDestroy(pEncoded);
	xrtXmlDomWriteOptionsInit(&tWrite);
	tWrite.bPretty = true;
	tWrite.iMaxOutputBytes = 1024u;
	pEncoded = bOk ? xrtXmlDomStringify(pDocument, &tWrite, &tError) : NULL;
	bOk = bOk && pEncoded == NULL && tError.iCode == XRT_XML_ERROR_LIMIT;
	bOk = bOk && testDeepTreeAllocations(pDocument);
	if ( !bOk ) { fprintf(stderr, "deep DOM failed, XML error %d\n", tError.iCode); }
	xrtBufferDestroy(pEncoded);
	xrtFree(sText);
	xrtXmlDomNodeRelease(pDocument);
	xrtBufferDestroy(pXml);
	return bOk;
}

int main(void)
{
	int bOk;
	bOk = testParseAllocationFailures() && testParseAndRoundTrip() && testConstructionAndMutation() &&
		testInvalidInputs() && testZeroLimits() && testWriterBoundaries() &&
		testWriteStreamFailures() &&
		testParseCloseFailure() && testDeepTree();
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
