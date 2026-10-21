/* 测试端启用完整声明；包实现和独立运行时按需裁剪。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_xml.h"

#include <stdio.h>
#include <string.h>



typedef struct {
	int iStarts;
	int iEnds;
	int iTexts;
	int bNamespace;
	int bAttribute;
} test_state;

static bool onEvent(ptr pContext, const xrtxmlevent* pEvent)
{
	test_state* pState = (test_state*)pContext;
	if ( pEvent->iType == XRT_XML_EVENT_START ) {
		pState->iStarts++;
		if ( strcmp((const char*)pEvent->sLocalName, "item") == 0 &&
			strcmp((const char*)pEvent->sNamespaceUri, "urn:test") == 0 ) {
			pState->bNamespace = 1;
		}
		if ( pEvent->iAttributeCount == 1u &&
			strcmp((const char*)pEvent->pAttributes[0].sValue, "7") == 0 ) {
			pState->bAttribute = 1;
		}
	} else if ( pEvent->iType == XRT_XML_EVENT_END ) {
		pState->iEnds++;
	} else if ( pEvent->iType == XRT_XML_EVENT_TEXT ) {
		pState->iTexts++;
	}
	return true;
}

static bool testSmallLimits(void)
{
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	test_state tState = { 0 };
	const char* sText = "<r>hi</r>";
	const char* sAttributes = "<r a=\"1\" b=\"2\"/>";
	xrtxmlsax* pPull;
	bool bAllocated;
	xrtXmlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 4u;
	if ( !xrtMemDebugEnabled() && !xrtMemDebugEnable(true) ) { return false; }
	if ( !xrtMemDebugFailAfter(0u) ) { return false; }
	pPull = xrtXmlSaxOpenMemory(sText, strlen(sText), &tOptions, &tError);
	bAllocated = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( pPull != NULL || tError.iCode != XRT_XML_ERROR_LIMIT || bAllocated ) {
		xrtXmlSaxDestroy(pPull);
		return false;
	}
	if ( xrtXmlSaxParseMemory(sText, strlen(sText), &tOptions,
		onEvent, &tState, &tError) || tError.iCode != XRT_XML_ERROR_LIMIT ) {
		return false;
	}
	if ( xrtXmlSaxParseMemory(sText, UINT64_MAX, &tOptions,
		onEvent, &tState, &tError) || tError.iCode != XRT_XML_ERROR_LIMIT ) {
		return false;
	}
	xrtXmlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxOutputBytes = 1u;
	if ( xrtXmlSaxParseMemory(sText, strlen(sText), &tOptions,
		onEvent, &tState, &tError) || tError.iCode != XRT_XML_ERROR_LIMIT ) {
		return false;
	}
	xrtXmlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxEntries = 1u;
	return !xrtXmlSaxParseMemory(sAttributes, strlen(sAttributes),
		&tOptions, onEvent, &tState, &tError) &&
		tError.iCode == XRT_XML_ERROR_LIMIT;
}

static bool testZeroLimits(void)
{
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	test_state tState = { 0 };
	const char* sText = "<root>ok</root>";
	xrtXmlOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 0u;
	tOptions.tLimits.iMaxOutputBytes = 0u;
	tOptions.tLimits.iMaxItemBytes = 0u;
	tOptions.tLimits.iMaxEntries = 0u;
	tOptions.tLimits.iMaxNodes = 0u;
	tOptions.tLimits.iMaxDepth = 0u;
	return xrtXmlSaxParseMemory(sText, strlen(sText), &tOptions,
		onEvent, &tState, &tError) && tError.iCode == XRT_XML_OK &&
		tState.iStarts == 1 && tState.iTexts == 1 && tState.iEnds == 1;
}

static bool testUnsupportedLimits(void)
{
	const char* sText = "<root>ok</root>";
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	test_state tState = { 0 };
	xrtxmlsax* pPull;
	xrtXmlOptionsInit(&tOptions);
	tOptions.tLimits.iReserved = 1u;
	if ( xrtXmlSaxParseMemory(sText, strlen(sText), &tOptions, onEvent,
		&tState, &tError) || tError.iCode != XRT_XML_ERROR_ARGUMENT ||
		tState.iStarts != 0 ) { return false; }
	pPull = xrtXmlSaxOpenMemory(sText, strlen(sText), &tOptions, &tError);
	if ( pPull != NULL || tError.iCode != XRT_XML_ERROR_ARGUMENT ) {
		xrtXmlSaxDestroy(pPull);
		return false;
	}
	tOptions.tLimits.iReserved = 0u;
	tOptions.tLimits.iFlags = XRT_RESOURCE_ALLOW_EXTERNAL_ENTITIES;
	if ( xrtXmlSaxParseMemory(sText, strlen(sText), &tOptions, onEvent,
		&tState, &tError) || tError.iCode != XRT_XML_ERROR_ARGUMENT ||
		tState.iStarts != 0 ) { return false; }
	return true;
}

typedef struct {
	const char* Text;
	size_t Size;
	size_t Offset;
	int Closed;
} test_close_reader;

static bool readForClose(ptr pContext, ptr pData, size_t iSize, size_t* pRead)
{
	test_close_reader* pProbe = (test_close_reader*)pContext;
	size_t iRemain = pProbe->Size - pProbe->Offset;
	*pRead = iSize < iRemain ? iSize : iRemain;
	if ( *pRead != 0u ) { memcpy(pData, pProbe->Text + pProbe->Offset, *pRead); }
	pProbe->Offset += *pRead;
	return true;
}

static bool failClose(ptr pContext)
{
	test_close_reader* pProbe = (test_close_reader*)pContext;
	++pProbe->Closed;
	xrtSetErrorInfo(XERR_IO, "test.xml.sax", 0, "injected close failure");
	return false;
}

static bool failReadMemory(ptr pContext, ptr pData, size_t iSize, size_t* pRead)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
	*pRead = 0u;
	xrtSetErrorInfo(XERR_MEMORY, "test.xml.sax", 0, "injected read allocation failure");
	return false;
}

static bool failCloseMemory(ptr pContext)
{
	test_close_reader* pProbe = (test_close_reader*)pContext;
	++pProbe->Closed;
	xrtSetErrorInfo(XERR_MEMORY, "test.xml.sax", 0, "injected close allocation failure");
	return false;
}

static bool passClose(ptr pContext)
{
	test_close_reader* pProbe = (test_close_reader*)pContext;
	++pProbe->Closed;
	return true;
}

static bool testParseCloseFailure(void)
{
	xreaderops Ops = {0};
	test_close_reader Probe = { "<r/>", 4u, 0u, 0 };
	xrtxmlerror Error;
	test_state State = {0};
	xreader* pReader;
	bool bOk;
	Ops.Read = readForClose;
	Ops.Close = failClose;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	bOk = !xrtXmlSaxParse(pReader, true, NULL, onEvent, &State, &Error) &&
		Error.iCode == XRT_XML_ERROR_IO && Probe.Closed == 1 &&
		Probe.Offset == Probe.Size && State.iStarts == 1 && State.iEnds == 1;
	if ( !bOk ) { fputs("XML SAX owned reader close failure was lost\n", stderr); }
	xrtClearError();

	Probe.Offset = 0u;
	Probe.Closed = 0;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	bOk = xrtXmlSaxParse(pReader, false, NULL, onEvent, &State, &Error) &&
		Error.iCode == XRT_XML_OK && Probe.Closed == 0 && bOk;
	bOk = !xrtReaderDestroy(pReader) && Probe.Closed == 1 && bOk;
	xrtClearError();

	Probe.Text = "<r>";
	Probe.Size = 3u;
	Probe.Offset = 0u;
	Probe.Closed = 0;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	bOk = !xrtXmlSaxParse(pReader, true, NULL, onEvent, &State, &Error) &&
		Error.iCode == XRT_XML_ERROR_FORMAT && Probe.Closed == 1 && bOk;
	xrtClearError();
	return bOk;
}

static bool testPullCloseFailure(void)
{
	xreaderops Ops = {0};
	test_close_reader Probe = { "<r/>", 4u, 0u, 0 };
	xrtxmlerror Error;
	xrtxmlsax* pPull;
	xreader* pReader;
	const xrtxmlevent* pEvent;
	int iStatus;
	int iEvents = 0;
	bool bOk;
	Ops.Read = readForClose;
	Ops.Close = failClose;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	pPull = xrtXmlSaxOpen(pReader, true, NULL, &Error);
	if ( pPull == NULL ) { return false; }
	do {
		iStatus = xrtXmlSaxNext(pPull, &pEvent, &Error);
		if ( iStatus == XRT_XML_NEXT_EVENT ) { iEvents++; }
	} while ( iStatus == XRT_XML_NEXT_EVENT );
	bOk = iStatus == XRT_XML_NEXT_ERROR && Error.iCode == XRT_XML_ERROR_IO &&
		Probe.Closed == 1 && Probe.Offset == Probe.Size && iEvents == 2;
	xrtXmlSaxDestroy(pPull);
	bOk = bOk && Probe.Closed == 1;
	xrtClearError();

	Probe.Offset = 0u;
	Probe.Closed = 0;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	pPull = xrtXmlSaxOpen(pReader, false, NULL, &Error);
	if ( pPull == NULL ) { xrtReaderDestroy(pReader); return false; }
	do { iStatus = xrtXmlSaxNext(pPull, &pEvent, &Error); }
	while ( iStatus == XRT_XML_NEXT_EVENT );
	bOk = bOk && iStatus == XRT_XML_NEXT_END && Error.iCode == XRT_XML_OK &&
		Probe.Closed == 0;
	xrtXmlSaxDestroy(pPull);
	bOk = bOk && Probe.Closed == 0;
	bOk = !xrtReaderDestroy(pReader) && Probe.Closed == 1 && bOk;
	xrtClearError();

	Probe.Offset = 0u;
	Probe.Closed = 0;
	Ops.Close = passClose;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	pPull = xrtXmlSaxOpen(pReader, true, NULL, &Error);
	if ( pPull == NULL ) { return false; }
	do { iStatus = xrtXmlSaxNext(pPull, &pEvent, &Error); }
	while ( iStatus == XRT_XML_NEXT_EVENT );
	bOk = bOk && iStatus == XRT_XML_NEXT_END && Error.iCode == XRT_XML_OK &&
		Probe.Closed == 1;
	xrtXmlSaxDestroy(pPull);
	bOk = bOk && Probe.Closed == 1;
	return bOk;
}

static bool testEarlyClose(void)
{
	xreaderops Ops = {0};
	test_close_reader Probe = { "<r><child/></r>", 15u, 0u, 0 };
	xrtxmlerror Error;
	xrtxmlsax* pPull;
	xreader* pReader;
	const xrtxmlevent* pEvent;
	int iStatus;
	bool bOk;
	Ops.Read = readForClose;
	Ops.Close = failClose;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	pPull = xrtXmlSaxOpen(pReader, true, NULL, &Error);
	if ( pPull == NULL ) { return false; }
	bOk = xrtXmlSaxNext(pPull, &pEvent, &Error) == XRT_XML_NEXT_EVENT &&
		pEvent != NULL && Probe.Closed == 0;
	bOk = !xrtXmlSaxClose(pPull, &Error) &&
		Error.iCode == XRT_XML_ERROR_IO && Probe.Closed == 1 && bOk;
	bOk = !xrtXmlSaxClose(pPull, &Error) &&
		Error.iCode == XRT_XML_ERROR_IO && Probe.Closed == 1 && bOk;
	bOk = xrtXmlSaxNext(pPull, &pEvent, &Error) == XRT_XML_NEXT_ERROR &&
		pEvent == NULL && Error.iCode == XRT_XML_ERROR_IO && bOk;
	xrtXmlSaxDestroy(pPull);
	bOk = Probe.Closed == 1 && bOk;
	xrtClearError();

	Probe.Offset = 0u;
	Probe.Closed = 0;
	Ops.Close = passClose;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	pPull = xrtXmlSaxOpen(pReader, false, NULL, &Error);
	if ( pPull == NULL ) { xrtReaderDestroy(pReader); return false; }
	bOk = xrtXmlSaxNext(pPull, &pEvent, &Error) == XRT_XML_NEXT_EVENT && bOk;
	bOk = xrtXmlSaxClose(pPull, &Error) && Error.iCode == XRT_XML_OK &&
		Probe.Closed == 0 && bOk;
	bOk = xrtXmlSaxNext(pPull, &pEvent, &Error) == XRT_XML_NEXT_END &&
		pEvent == NULL && bOk;
	xrtXmlSaxDestroy(pPull);
	bOk = Probe.Closed == 0 && bOk;
	bOk = xrtReaderDestroy(pReader) && Probe.Closed == 1 && bOk;

	Probe.Offset = 0u;
	Probe.Closed = 0;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	pPull = xrtXmlSaxOpen(pReader, true, NULL, &Error);
	if ( pPull == NULL ) { return false; }
	bOk = xrtXmlSaxNext(pPull, &pEvent, &Error) == XRT_XML_NEXT_EVENT && bOk;
	bOk = xrtXmlSaxClose(pPull, &Error) &&
		xrtXmlSaxClose(pPull, &Error) && Probe.Closed == 1 && bOk;
	xrtXmlSaxDestroy(pPull);
	bOk = bOk && Probe.Closed == 1;

	Probe.Text = "<r>";
	Probe.Size = 3u;
	Probe.Offset = 0u;
	Probe.Closed = 0;
	Ops.Close = failClose;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	pPull = xrtXmlSaxOpen(pReader, true, NULL, &Error);
	if ( pPull == NULL ) { return false; }
	do { iStatus = xrtXmlSaxNext(pPull, &pEvent, &Error); }
	while ( iStatus == XRT_XML_NEXT_EVENT );
	bOk = iStatus == XRT_XML_NEXT_ERROR &&
		Error.iCode == XRT_XML_ERROR_FORMAT && bOk;
	bOk = !xrtXmlSaxClose(pPull, &Error) &&
		Error.iCode == XRT_XML_ERROR_FORMAT && Probe.Closed == 1 && bOk;
	xrtXmlSaxDestroy(pPull);
	xrtClearError();
	return bOk && Probe.Closed == 1;
}

/* 自定义 Reader 的资源故障不能被 XML 包误报为设备 I/O。 */
static bool testReaderMemoryErrors(void)
{
	xreaderops Ops = {0};
	test_close_reader Probe = { "<r/>", 4u, 0u, 0 };
	xrtxmlerror Error;
	test_state State = {0};
	xrtxmlsax* pPull;
	xreader* pReader;
	const xrtxmlevent* pEvent;
	int iStatus;
	bool bOk = true;
	Ops.Read = readForClose;
	Ops.Close = failCloseMemory;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	bOk = !xrtXmlSaxParse(pReader, true, NULL, onEvent, &State, &Error) &&
		Error.iCode == XRT_XML_ERROR_MEMORY && Probe.Closed == 1;
	xrtClearError();
	Probe.Offset = 0u;
	Probe.Closed = 0;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	pPull = xrtXmlSaxOpen(pReader, true, NULL, &Error);
	if ( pPull == NULL ) { return false; }
	do { iStatus = xrtXmlSaxNext(pPull, &pEvent, &Error); }
	while ( iStatus == XRT_XML_NEXT_EVENT );
	bOk = iStatus == XRT_XML_NEXT_ERROR &&
		Error.iCode == XRT_XML_ERROR_MEMORY && Probe.Closed == 1 && bOk;
	xrtXmlSaxDestroy(pPull);
	xrtClearError();
	Probe.Offset = 0u;
	Probe.Closed = 0;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	pPull = xrtXmlSaxOpen(pReader, true, NULL, &Error);
	if ( pPull == NULL ) { return false; }
	bOk = xrtXmlSaxNext(pPull, &pEvent, &Error) == XRT_XML_NEXT_EVENT && bOk;
	bOk = !xrtXmlSaxClose(pPull, &Error) &&
		Error.iCode == XRT_XML_ERROR_MEMORY && Probe.Closed == 1 && bOk;
	xrtXmlSaxDestroy(pPull);
	xrtClearError();
	Probe.Offset = 0u;
	Probe.Closed = 0;
	Ops.Read = failReadMemory;
	Ops.Close = passClose;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	bOk = !xrtXmlSaxParse(pReader, true, NULL, onEvent, &State, &Error) &&
		Error.iCode == XRT_XML_ERROR_MEMORY && Probe.Closed == 1 && bOk;
	xrtClearError();
	Probe.Closed = 0;
	pReader = xrtReaderCreate(&Ops, &Probe);
	if ( pReader == NULL ) { return false; }
	pPull = xrtXmlSaxOpen(pReader, true, NULL, &Error);
	if ( pPull == NULL ) { return false; }
	iStatus = xrtXmlSaxNext(pPull, &pEvent, &Error);
	bOk = iStatus == XRT_XML_NEXT_ERROR &&
		Error.iCode == XRT_XML_ERROR_MEMORY && bOk;
	xrtXmlSaxDestroy(pPull);
	xrtClearError();
	if ( !bOk ) { fputs("XML SAX changed Reader memory failure into I/O\n", stderr); }
	return bOk;
}

int main(void)
{
	const char* sXml = "<root id=\"7\" xmlns:p=\"urn:test\"><p:item>ok</p:item></root>";
	const char* sDtd = "<!DOCTYPE root [<!ENTITY x \"boom\">]><root>&x;</root>";
	const char* sExternal = "<!DOCTYPE root SYSTEM \"file:///xlang-extlib-no-such-entity\"><root/>";
	const char* sEntity = "<!DOCTYPE root [<!ENTITY x SYSTEM \"file:///xlang-extlib-no-such-entity\">]><root>&x;</root>";
	const char* sMalformed = "<root><item></root>";
	const char* sDeep = "<a><b><c/></b></a>";
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	test_state tState;
	int bOk;
	int iStatus;
	const xrtxmlevent* pEvent;
	xrtxmlsax* pPull;
	memset(&tState, 0, sizeof(tState));
	xrtXmlOptionsInit(&tOptions);
	bOk = testSmallLimits() && testZeroLimits() && testUnsupportedLimits() &&
		testParseCloseFailure() &&
		testPullCloseFailure() && testEarlyClose() &&
		testReaderMemoryErrors() &&
		xrtXmlSaxParseMemory(sXml, strlen(sXml), &tOptions, onEvent,
		&tState, &tError) && tState.iStarts == 2 && tState.iEnds == 2 &&
		tState.iTexts == 1 && tState.bNamespace && tState.bAttribute;
	bOk = bOk && !xrtXmlSaxParseMemory(sDtd, strlen(sDtd), &tOptions,
		onEvent, &tState, &tError) && tError.iCode == XRT_XML_ERROR_DTD;
	bOk = bOk && !xrtXmlSaxParseMemory(sExternal, strlen(sExternal), &tOptions,
		onEvent, &tState, &tError) && tError.iCode == XRT_XML_ERROR_DTD;
	bOk = bOk && !xrtXmlSaxParseMemory(sEntity, strlen(sEntity), &tOptions,
		onEvent, &tState, &tError) && tError.iCode == XRT_XML_ERROR_DTD;
	bOk = bOk && !xrtXmlSaxParseMemory(sMalformed, strlen(sMalformed), &tOptions,
		onEvent, &tState, &tError) && tError.iCode == XRT_XML_ERROR_FORMAT;
	tOptions.tLimits.iMaxDepth = 2u;
	bOk = bOk && !xrtXmlSaxParseMemory(sDeep, strlen(sDeep), &tOptions,
		onEvent, &tState, &tError) && tError.iCode == XRT_XML_ERROR_LIMIT;

	/* pull API 必须在全部事件消费后明确返回 END，而不能悬挂事件指针。 */
	xrtXmlOptionsInit(&tOptions);
	pPull = xrtXmlSaxOpenMemory("<root>ok</root>", 15u, &tOptions, &tError);
	bOk = bOk && pPull != NULL;
	if ( pPull != NULL ) {
		int iEvents = 0;
		do {
			iStatus = xrtXmlSaxNext(pPull, &pEvent, &tError);
			if ( iStatus == XRT_XML_NEXT_EVENT && pEvent != NULL ) { iEvents++; }
		} while ( iStatus == XRT_XML_NEXT_EVENT );
		bOk = bOk && iEvents == 3 && iStatus == XRT_XML_NEXT_END && pEvent == NULL;
		xrtXmlSaxDestroy(pPull);
	}
	if ( bOk ) { puts("test_xml_sax: PASS"); }
	return bOk ? 0 : 2;
}

/* 单文件测试入口；包导入时由清单分别编译这些源码。 */
#include "../xrt_xml_common.c"
#include "../xrt_xml_sax.c"
#ifndef XRT_XML_SAX_TEST_EXTERNAL_EXPAT
#include "../../_expat/vendor/expat/lib/xmlparse.c"
#include "../../_expat/vendor/expat/lib/xmlrole.c"
#include "../../_expat/vendor/expat/lib/xmltok.c"
#include "../../_expat/expat_random.c"
#endif
