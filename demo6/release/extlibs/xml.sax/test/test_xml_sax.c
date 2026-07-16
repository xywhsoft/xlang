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
	return TRUE;
}

int main(void)
{
	const char* sXml = "<root id=\"7\" xmlns:p=\"urn:test\"><p:item>ok</p:item></root>";
	const char* sDtd = "<!DOCTYPE root [<!ENTITY x \"boom\">]><root>&x;</root>";
	const char* sMalformed = "<root><item></root>";
	const char* sDeep = "<a><b><c/></b></a>";
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	test_state tState;
	int bOk;
	if ( xrtInit() == NULL ) { return 1; }
	memset(&tState, 0, sizeof(tState));
	xrtXmlOptionsInit(&tOptions);
	bOk = xrtXmlSaxParseMemory(sXml, strlen(sXml), &tOptions, onEvent,
		&tState, &tError) && tState.iStarts == 2 && tState.iEnds == 2 &&
		tState.iTexts == 1 && tState.bNamespace && tState.bAttribute;
	bOk = bOk && !xrtXmlSaxParseMemory(sDtd, strlen(sDtd), &tOptions,
		onEvent, &tState, &tError) && tError.iCode == XRT_XML_ERROR_DTD;
	bOk = bOk && !xrtXmlSaxParseMemory(sMalformed, strlen(sMalformed), &tOptions,
		onEvent, &tState, &tError) && tError.iCode == XRT_XML_ERROR_FORMAT;
	tOptions.tLimits.iMaxDepth = 2u;
	bOk = bOk && !xrtXmlSaxParseMemory(sDeep, strlen(sDeep), &tOptions,
		onEvent, &tState, &tError) && tError.iCode == XRT_XML_ERROR_LIMIT;
	xrtUnit();
	if ( bOk ) { puts("test_xml_sax: PASS"); }
	return bOk ? 0 : 2;
}

/* 单文件测试入口；包导入时由清单分别编译这些源码。 */
#include "../xrt_xml_common.c"
#include "../xrt_xml_sax.c"
#include "../../_expat/vendor/expat/lib/xmlparse.c"
#include "../../_expat/vendor/expat/lib/xmlrole.c"
#include "../../_expat/vendor/expat/lib/xmltok.c"
#include "../../_expat/expat_random.c"
