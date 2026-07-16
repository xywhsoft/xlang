#include "../../extlibs/xml.sax/include/xrt_xml.h"

#include <stdio.h>
#include <string.h>



static bool printEvent(ptr pContext, const xrtxmlevent* pEvent)
{
	(void)pContext;
	if ( pEvent->iType == XRT_XML_EVENT_START ) {
		printf("start: %s\n", pEvent->sName);
	} else if ( pEvent->iType == XRT_XML_EVENT_TEXT ) {
		printf("text: %s\n", pEvent->sText);
	}
	return TRUE;
}

int main(void)
{
	const char* sXml = "<catalog><book>XLang</book><book>XRT</book></catalog>";
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	int bOk;
	if ( xrtInit() == NULL ) { return 1; }
	xrtXmlOptionsInit(&tOptions);
	bOk = xrtXmlSaxParseMemory(sXml, strlen(sXml), &tOptions, printEvent,
		NULL, &tError);
	if ( !bOk ) { fprintf(stderr, "%s\n", tError.sMessage); }
	xrtUnit();
	if ( bOk ) { puts("xml.sax C example: PASS"); }
	return bOk ? 0 : 2;
}

/* 单文件范例入口；包导入时由清单分别编译这些源码。 */
#include "../../extlibs/xml.sax/xrt_xml_common.c"
#include "../../extlibs/xml.sax/xrt_xml_sax.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmlparse.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmlrole.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmltok.c"
#include "../../extlibs/_expat/expat_random.c"
