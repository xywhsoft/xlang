/* 测试端启用完整声明；包实现和独立运行时按需裁剪。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_xml_etree.h"

#include <stdio.h>
#include <string.h>


/* 新属性在任一分配点失败后，树保持原状且释放时无残留分配。 */
static bool testAttributeAllocationAtomicity(void)
{
	xmemdebugsnapshot tBefore;
	xmemdebugsnapshot tAfter;
	unsigned int iFailure;
	bool bCompleted = false;

	xrtClearError();
	xrtMemDebugSnapshot(&tBefore);
	for ( iFailure = 0u; iFailure < 16u; iFailure++ ) {
		xrtxmlelement* pRoot = xrtXmlElementCreate((str)"root");
		bool bSet;
		bool bTriggered;
		if ( pRoot == NULL || !xrtMemDebugFailAfter(iFailure) ) {
			xrtXmlElementRelease(pRoot);
			return false;
		}
		bSet = xrtXmlElementSetAttribute(pRoot, (str)"mode", (str)"safe");
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) {
			if ( bSet || xrtXmlElementAttributeCount(pRoot) != 0u ||
				!xrtXmlElementSetAttribute(pRoot, (str)"mode", (str)"safe") ) {
				xrtXmlElementRelease(pRoot);
				return false;
			}
		} else {
			bCompleted = bSet;
		}
		if ( xrtXmlElementAttributeCount(pRoot) != 1u ||
			strcmp((const char*)xrtXmlElementGetAttribute(pRoot,
				(str)"mode"), "safe") != 0 ) {
			xrtXmlElementRelease(pRoot);
			return false;
		}
		xrtXmlElementRelease(pRoot);
		xrtClearError();
		xrtMemDebugSnapshot(&tAfter);
		if ( tAfter.LiveCount != tBefore.LiveCount ||
			tAfter.LiveBytes != tBefore.LiveBytes ) {
			fprintf(stderr, "XML attribute allocation leak after failure %u: "
				"live %zu -> %zu, bytes %zu -> %zu\n", iFailure,
				tBefore.LiveCount, tAfter.LiveCount,
				tBefore.LiveBytes, tAfter.LiveBytes);
			return false;
		}
		if ( bCompleted ) { break; }
	}
	return bCompleted && iFailure >= 5u;
}


typedef struct {
	const char* Text;
	size_t Offset;
	int Closed;
} etree_close_reader;

static bool etreeReadForClose(ptr pContext, ptr pData, size_t iSize, size_t* pRead)
{
	etree_close_reader* pProbe = (etree_close_reader*)pContext;
	size_t iRemain = strlen(pProbe->Text) - pProbe->Offset;
	*pRead = iSize < iRemain ? iSize : iRemain;
	if ( *pRead != 0u ) { memcpy(pData, pProbe->Text + pProbe->Offset, *pRead); }
	pProbe->Offset += *pRead;
	return true;
}

static bool etreeFailClose(ptr pContext)
{
	etree_close_reader* pProbe = (etree_close_reader*)pContext;
	pProbe->Closed++;
	xrtSetErrorInfo(XERR_IO, "test.xml.etree", 0, "injected close failure");
	return false;
}

static bool etreeWriteProbe(ptr pContext, const void* pData,
	size_t iSize, size_t* pWritten)
{
	etree_close_reader* pProbe = (etree_close_reader*)pContext;
	(void)pData;
	pProbe->Offset += iSize;
	*pWritten = iSize;
	return true;
}

static bool etreeWriteMemoryFailure(ptr pContext, const void* pData,
	size_t iSize, size_t* pWritten)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
	*pWritten = 0u;
	xrtSetErrorInfo(XERR_MEMORY, "test.xml.etree", 0,
		"injected write allocation failure");
	return false;
}

static bool testWriteStreamFailures(void)
{
	xwriterops tOps = {0};
	etree_close_reader tProbe = { "", 0u, 0 };
	xrtxmlerror tError;
	xrtxmlelement* pRoot = xrtXmlElementCreate((str)"root");
	xwriter* pWriter;
	bool bOk = pRoot != NULL;
	if ( !bOk ) { return false; }
	tOps.Write = etreeWriteProbe;
	tOps.Close = etreeFailClose;
	pWriter = xrtWriterCreate(&tOps, &tProbe);
	bOk = pWriter != NULL;
	if ( bOk ) {
		xrtSetErrorInfo(XERR_MEMORY, "test.xml.etree", 0, "stale memory error");
		bOk = !xrtXmlElementWrite(pRoot, pWriter, true, NULL, &tError) &&
			tError.iCode == XRT_XML_ERROR_IO && tProbe.Offset != 0u &&
			tProbe.Closed == 1;
	}
	xrtClearError();
	tOps.Write = etreeWriteMemoryFailure;
	tOps.Close = NULL;
	pWriter = xrtWriterCreate(&tOps, NULL);
	bOk = bOk && pWriter != NULL;
	if ( pWriter != NULL ) {
		bool bWritten = xrtXmlElementWrite(pRoot, pWriter, true, NULL, &tError);
		bOk = !bWritten && tError.iCode == XRT_XML_ERROR_MEMORY && bOk;
	}
	xrtClearError();
	xrtXmlElementRelease(pRoot);
	return bOk;
}

static bool testParseCloseFailure(void)
{
	xreaderops tOps = {0};
	etree_close_reader tProbe = { "<root/>", 0u, 0 };
	xrtxmlerror tError;
	xreader* pReader;
	xrtxmlelement* pRoot;
	tOps.Read = etreeReadForClose;
	tOps.Close = etreeFailClose;
	pReader = xrtReaderCreate(&tOps, &tProbe);
	if ( pReader == NULL ) { return false; }
	pRoot = xrtXmlElementParse(pReader, true, NULL, &tError);
	return pRoot == NULL && tError.iCode == XRT_XML_ERROR_IO &&
		tProbe.Closed == 1 && tProbe.Offset == strlen(tProbe.Text);
}

static void releaseMatches(xvalue* pMatches)
{
	size_t i;
	for ( i = 0u; i < xrtValueCount(pMatches); i++ ) {
		ptr pHandle = NULL;
		if ( xrtValueGetPointer(xrtValueArrayGet(pMatches, i), &pHandle) ) {
			xrtXmlElementRelease((xrtxmlelement*)pHandle);
		}
	}
	xrtValueRelease(pMatches);
}

static bool testFindAllAllocationFailures(void)
{
	static const char sXml[] = "<root><z/><branch><z/></branch><z/></root>";
	xrtxmlerror tError;
	xrtxmlelement* pRoot = xrtXmlElementParseMemory(sXml,
		sizeof(sXml) - 1u, NULL, &tError);
	xmemdebugsnapshot tBefore;
	xmemdebugsnapshot tAfter;
	unsigned int iFailure;
	bool bCompleted = false;
	bool bOk = pRoot != NULL;
	if ( !bOk ) { return false; }
	xrtClearError();
	xrtMemDebugSnapshot(&tBefore);
	for ( iFailure = 0u; iFailure < 64u; iFailure++ ) {
		xvalue* pMatches;
		bool bTriggered;
		if ( !xrtMemDebugFailAfter(iFailure) ) { bOk = false; break; }
		pMatches = xrtXmlElementFindAll(pRoot, (str)"//z");
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pMatches != NULL && xrtValueCount(pMatches) != 3u ) { bOk = false; }
		if ( !bTriggered ) { bCompleted = pMatches != NULL; }
		releaseMatches(pMatches);
		xrtClearError();
		xrtMemDebugSnapshot(&tAfter);
		if ( tAfter.LiveCount != tBefore.LiveCount ||
			tAfter.LiveBytes != tBefore.LiveBytes ) {
			fprintf(stderr, "findAll allocation failure %u leaked: "
				"live %zu -> %zu, bytes %zu -> %zu\n", iFailure,
				tBefore.LiveCount, tAfter.LiveCount,
				tBefore.LiveBytes, tAfter.LiveBytes);
			bOk = false;
		}
		if ( !bOk || !bTriggered ) { break; }
	}
	xrtXmlElementRelease(pRoot);
	return bOk && bCompleted && iFailure > 0u;
}

static bool testDeepWriterAllocations(const xrtxmlelement* pRoot)
{
	xmemdebugsnapshot tBefore;
	xmemdebugsnapshot tAfter;
	xrtxmlerror tError;
	unsigned int iFailure;
	bool bCompleted = false;
	xrtClearError();
	xrtMemDebugSnapshot(&tBefore);
	for ( iFailure = 0u; iFailure < 128u; iFailure++ ) {
		xbuffer* pEncoded;
		bool bTriggered;
		if ( !xrtMemDebugFailAfter(iFailure) ) { return false; }
		pEncoded = xrtXmlElementStringify(pRoot, NULL, &tError);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pEncoded == NULL && tError.iCode != XRT_XML_ERROR_MEMORY ) {
			return false;
		}
		if ( !bTriggered ) { bCompleted = pEncoded != NULL; }
		xrtBufferDestroy(pEncoded);
		xrtClearError();
		xrtMemDebugSnapshot(&tAfter);
		if ( tAfter.LiveCount != tBefore.LiveCount ||
			tAfter.LiveBytes != tBefore.LiveBytes ) {
			fprintf(stderr, "deep ElementTree writer failure %u leaked\n", iFailure);
			return false;
		}
		if ( !bTriggered ) { break; }
	}
	return bCompleted && iFailure > 0u;
}

/* 可配置解析深度不应让后续查询、写出或释放耗尽 C 栈。 */
static bool testDeepTree(void)
{
	const uint32 iDepth = 8192u;
	xbuffer* pXml = xrtBufferCreate();
	xbuffer* pEncoded = NULL;
	xrtxmloptions tOptions;
	xrtxmletreewriteoptions tWrite;
	xrtxmlerror tError;
	xrtxmlelement* pRoot = NULL;
	xrtxmlelement* pFound = NULL;
	xvalue* pMatches = NULL;
	ptr pMatchHandle = NULL;
	uint32 i;
	bool bOk = pXml != NULL;
	for ( i = 0u; bOk && i < iDepth; i++ ) {
		bOk = xrtBufferAppend(pXml, (xbytesview){ (cbytes)"<a>", 3u });
	}
	bOk = bOk && xrtBufferAppend(pXml, (xbytesview){ (cbytes)"<z/>", 4u });
	for ( i = 0u; bOk && i < iDepth; i++ ) {
		bOk = xrtBufferAppend(pXml, (xbytesview){ (cbytes)"</a>", 4u });
	}
	xrtXmlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxDepth = iDepth + 2u;
	pRoot = bOk ? xrtXmlElementParseMemory(pXml->Data, pXml->Size,
		&tOptions, &tError) : NULL;
	bOk = bOk && pRoot != NULL && tError.iCode == XRT_XML_OK;
	pFound = bOk ? xrtXmlElementFind(pRoot, (str)"//z") : NULL;
	bOk = bOk && pFound != NULL &&
		strcmp((const char*)xrtXmlElementName(pFound), "z") == 0;
	pMatches = bOk ? xrtXmlElementFindAll(pRoot, (str)"//z") : NULL;
	bOk = bOk && pMatches != NULL && xrtValueCount(pMatches) == 1u &&
		xrtValueGetPointer(xrtValueArrayGet(pMatches, 0u), &pMatchHandle) &&
		pMatchHandle == pFound;
	pEncoded = bOk ? xrtXmlElementStringify(pRoot, NULL, &tError) : NULL;
	bOk = bOk && pEncoded != NULL && tError.iCode == XRT_XML_OK &&
		pEncoded->Size == pXml->Size &&
		memcmp(pEncoded->Data, pXml->Data, pXml->Size) == 0;
	xrtBufferDestroy(pEncoded);
	xrtXmlElementWriteOptionsInit(&tWrite);
	tWrite.bPretty = true;
	tWrite.iMaxOutputBytes = 1024u;
	pEncoded = bOk ? xrtXmlElementStringify(pRoot, &tWrite, &tError) : NULL;
	bOk = bOk && pEncoded == NULL && tError.iCode == XRT_XML_ERROR_LIMIT;
	bOk = bOk && testDeepWriterAllocations(pRoot);
	if ( !bOk ) { fprintf(stderr, "deep ElementTree failed, XML error %d\n", tError.iCode); }
	xrtBufferDestroy(pEncoded);
	releaseMatches(pMatches);
	xrtXmlElementRelease(pFound);
	xrtXmlElementRelease(pRoot);
	xrtBufferDestroy(pXml);
	return bOk;
}

int main(void)
{
	const char* sXml = "<root xmlns:p=\"urn:test\">head<p:item id=\"1\">one</p:item>tail<!--note--><?app mode?></root>";
	xrtxmloptions tOptions;
	xrtxmletreewriteoptions tWrite;
	xrtxmlerror tError;
	xrtxmlelement* pRoot;
	xrtxmlelement* pItem;
	xrtxmlelement* pInvalid;
	xrtxmlelement* pComment;
	xrtxmlelement* pInstruction;
	xrtxmlelement* pRoundRoot = NULL;
	xrtxmlelement* pRoundItem = NULL;
	xbuffer* pText;
	int bOk;
	bool bAllocated;
	xrtXmlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 4u;
	if ( !xrtMemDebugEnabled() && !xrtMemDebugEnable(true) ) { return 2; }
	if ( !xrtMemDebugFailAfter(0u) ) { return 2; }
	pRoot = xrtXmlElementParseMemory("<root/>", 7u, &tOptions, &tError);
	bAllocated = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	bOk = pRoot == NULL && tError.iCode == XRT_XML_ERROR_LIMIT && !bAllocated;
	xrtXmlElementRelease(pRoot);
	pRoot = xrtXmlElementParseMemory("<root/>", UINT64_MAX, &tOptions, &tError);
	bOk = bOk && pRoot == NULL && tError.iCode == XRT_XML_ERROR_LIMIT;
	xrtXmlElementRelease(pRoot);
	xrtXmlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 0u;
	tOptions.tLimits.iMaxOutputBytes = 0u;
	tOptions.tLimits.iMaxItemBytes = 0u;
	tOptions.tLimits.iMaxEntries = 0u;
	tOptions.tLimits.iMaxNodes = 0u;
	tOptions.tLimits.iMaxDepth = 0u;
	pRoot = xrtXmlElementParseMemory("<root>ok</root>", 15u,
		&tOptions, &tError);
	bOk = bOk && pRoot != NULL && tError.iCode == XRT_XML_OK;
	xrtXmlElementRelease(pRoot);
	xrtXmlOptionsInit(&tOptions);
	pRoot = xrtXmlElementParseMemory(sXml, strlen(sXml), &tOptions, &tError);
	pItem = pRoot != NULL ? xrtXmlElementFind(pRoot, (str)"p:item") : NULL;
	bOk = bOk && pRoot != NULL && pItem != NULL &&
		strcmp((const char*)xrtXmlElementText(pRoot), "head") == 0 &&
		strcmp((const char*)xrtXmlElementTail(pItem), "tail") == 0 &&
		strcmp((const char*)xrtXmlElementNamespaceUri(pItem), "urn:test") == 0;
	bOk = bOk && xrtXmlElementSetText(pItem, (str)"changed & safe\rline") &&
		xrtXmlElementSetAttribute(pItem, (str)"quote", (str)"a\"b") &&
		xrtXmlElementSetAttribute(pItem, (str)"space", (str)"A\nB\tC\rD") &&
		xrtXmlElementChildCount(pRoot) == 3u;
	xrtXmlElementWriteOptionsInit(&tWrite);
	pText = bOk ? xrtXmlElementStringify(pRoot, &tWrite, &tError) : NULL;
	bOk = bOk && pText != NULL && xrtBufferAppendByte(pText, 0u) &&
		strstr((const char*)pText->Data, "xmlns:p=\"urn:test\"") != NULL &&
		strstr((const char*)pText->Data, "head<p:item") != NULL &&
		strstr((const char*)pText->Data,
			"</p:item>tail<!--note--><?app mode?>") != NULL &&
		strstr((const char*)pText->Data, "changed &amp; safe&#xD;line") != NULL &&
		strstr((const char*)pText->Data,
			"space=\"A&#xA;B&#x9;C&#xD;D\"") != NULL;
	pRoundRoot = bOk ? xrtXmlElementParseMemory(pText->Data,
		pText->Size - 1u, &tOptions, &tError) : NULL;
	pRoundItem = pRoundRoot != NULL ?
		xrtXmlElementFind(pRoundRoot, (str)"p:item") : NULL;
	bOk = bOk && pRoundItem != NULL &&
		strcmp((const char*)xrtXmlElementGetAttribute(pRoundItem,
			(str)"space"), "A\nB\tC\rD") == 0 &&
		strcmp((const char*)xrtXmlElementText(pRoundItem),
			"changed & safe\rline") == 0;
	xrtXmlElementRelease(pRoundItem);
	xrtXmlElementRelease(pRoundRoot);
	pInvalid = xrtXmlElementParseMemory("<root><x></root>",
		strlen("<root><x></root>"), &tOptions, &tError);
	bOk = bOk && pInvalid == NULL && tError.iCode == XRT_XML_ERROR_FORMAT;
	xrtXmlElementRelease(pInvalid);
	pInvalid = xrtXmlElementParseMemory("<!DOCTYPE root [<!ENTITY value 'secret'>]><root>&value;</root>",
		strlen("<!DOCTYPE root [<!ENTITY value 'secret'>]><root>&value;</root>"),
		&tOptions, &tError);
	bOk = bOk && pInvalid == NULL && tError.iCode == XRT_XML_ERROR_DTD;
	xrtXmlElementRelease(pInvalid);
	{
		static const char sExternal[] =
			"<!DOCTYPE root SYSTEM \"file:///xlang-extlib-no-such-entity\"><root/>";
		pInvalid = xrtXmlElementParseMemory(sExternal, sizeof(sExternal) - 1u,
			&tOptions, &tError);
		bOk = bOk && pInvalid == NULL && tError.iCode == XRT_XML_ERROR_DTD;
		xrtXmlElementRelease(pInvalid);
	}
	{
		static const char sEntity[] =
			"<!DOCTYPE root [<!ENTITY x SYSTEM \"file:///xlang-extlib-no-such-entity\">]><root>&x;</root>";
		pInvalid = xrtXmlElementParseMemory(sEntity, sizeof(sEntity) - 1u,
			&tOptions, &tError);
		bOk = bOk && pInvalid == NULL && tError.iCode == XRT_XML_ERROR_DTD;
		xrtXmlElementRelease(pInvalid);
	}
	tOptions.tLimits.iMaxDepth = 2u;
	pInvalid = xrtXmlElementParseMemory("<a><b><c/></b></a>",
		strlen("<a><b><c/></b></a>"), &tOptions, &tError);
	bOk = bOk && pInvalid == NULL && tError.iCode == XRT_XML_ERROR_LIMIT;
	xrtXmlElementRelease(pInvalid);
	/* 写出限额、XML 字符和失败保存的边界。 */
	bOk = bOk && xrtXmlElementCreate((str)"1root") == NULL &&
		!xrtXmlElementSetAttribute(pRoot, (str)"bad?", (str)"value");
	xrtXmlElementWriteOptionsInit(&tWrite);
	tWrite.iMaxOutputBytes = 1u;
	{
		xbuffer* pLimited = xrtXmlElementStringify(pRoot, &tWrite, &tError);
		bOk = bOk && pLimited == NULL && tError.iCode == XRT_XML_ERROR_LIMIT;
		xrtBufferDestroy(pLimited);
	}
	xrtXmlElementWriteOptionsInit(&tWrite);
	bOk = bOk && xrtXmlElementSetText(pItem, (str)"bad \xef\xbf\xbf");
	{
		const char* sPath = "xrt_xml_etree_writer_boundary.xml";
		size_t iSaved = 0u;
		bytes pSaved;
		xbuffer* pInvalidText = bOk ?
			xrtXmlElementStringify(pRoot, &tWrite, &tError) : NULL;
		bOk = bOk && pInvalidText == NULL &&
			tError.iCode == XRT_XML_ERROR_FORMAT;
		xrtBufferDestroy(pInvalidText);
		bOk = bOk && xrtFileWriteAll(sPath,
			(xbytesview){ (cbytes)"old", 3u });
		bOk = bOk && !xrtXmlElementSave((str)sPath, pRoot, &tWrite,
			&tError) && tError.iCode == XRT_XML_ERROR_FORMAT;
		pSaved = xrtFileReadAll(sPath, &iSaved);
		bOk = bOk && pSaved != NULL && iSaved == 3u &&
			memcmp(pSaved, "old", 3u) == 0;
		xrtFree(pSaved);
		bOk = bOk && xrtXmlElementSetText(pItem, (str)"ok") &&
			xrtXmlElementSave((str)sPath, pRoot, &tWrite, &tError);
		pSaved = xrtFileReadAll(sPath, &iSaved);
		bOk = bOk && pSaved != NULL && iSaved >= 5u &&
			memcmp(pSaved, "<root", 5u) == 0;
		xrtFree(pSaved);
		if ( bOk ) {
			xrtxmlelement* pLoaded;
			bool bTriggered;
			if ( !xrtMemDebugFailAfter(0u) ) { bOk = 0; }
			else {
				pLoaded = xrtXmlElementLoad((str)sPath, NULL, &tError);
				bTriggered = xrtMemDebugFailTriggered();
				xrtMemDebugFailClear();
				bOk = bOk && bTriggered && pLoaded == NULL &&
					tError.iCode == XRT_XML_ERROR_MEMORY;
				xrtXmlElementRelease(pLoaded);
				xrtClearError();
				if ( bOk ) {
					pLoaded = xrtXmlElementLoad((str)sPath, NULL, &tError);
					bOk = pLoaded != NULL && tError.iCode == XRT_XML_OK;
					xrtXmlElementRelease(pLoaded);
				}
			}
			if ( !bOk ) { fputs("ElementTree file open memory failure was misclassified\n", stderr); }
		}
		if ( bOk ) {
			unsigned int iFailure;
			bool bCompleted = false;
			for ( iFailure = 0u; iFailure < 128u; ++iFailure ) {
				bool bSaved, bTriggered;
				if ( !xrtMemDebugFailAfter(iFailure) ) { bOk = 0; break; }
				bSaved = xrtXmlElementSave((str)sPath, pRoot, &tWrite, &tError);
				bTriggered = xrtMemDebugFailTriggered();
				xrtMemDebugFailClear();
				if ( bTriggered ? (bSaved || tError.iCode != XRT_XML_ERROR_MEMORY) :
					(!bSaved || tError.iCode != XRT_XML_OK) ) {
					fprintf(stderr, "ElementTree save allocation %u misclassified: %d\n",
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
	}
	pComment = xrtXmlElementChild(pRoot, 1u);
	bOk = bOk && pComment != NULL &&
		xrtXmlElementSetText(pComment, (str)"bad-");
	{
		xbuffer* pInvalidComment = bOk ?
			xrtXmlElementStringify(pRoot, &tWrite, &tError) : NULL;
		bOk = bOk && pInvalidComment == NULL &&
			tError.iCode == XRT_XML_ERROR_FORMAT;
		xrtBufferDestroy(pInvalidComment);
	}
	bOk = bOk && xrtXmlElementSetText(pComment, (str)"ok");
	pInstruction = xrtXmlElementChild(pRoot, 2u);
	bOk = bOk && pInstruction != NULL &&
		xrtXmlElementSetAttribute(pInstruction, (str)"target", (str)"xml");
	{
		xbuffer* pInvalidInstruction = bOk ?
			xrtXmlElementStringify(pRoot, &tWrite, &tError) : NULL;
		bOk = bOk && pInvalidInstruction == NULL &&
			tError.iCode == XRT_XML_ERROR_FORMAT;
		xrtBufferDestroy(pInvalidInstruction);
	}
	xrtXmlElementRelease(pInstruction);
	xrtXmlElementRelease(pComment);
	xrtBufferDestroy(pText);
	xrtXmlElementRelease(pItem);
	xrtXmlElementRelease(pRoot);
	bOk = bOk && testAttributeAllocationAtomicity() &&
		testParseCloseFailure() && testWriteStreamFailures() &&
		testFindAllAllocationFailures() &&
		testDeepTree();
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
