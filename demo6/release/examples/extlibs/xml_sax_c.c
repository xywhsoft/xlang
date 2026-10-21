#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_STRING
#define XRT_MODULE_ONCE
#define XRT_MODULE_THREAD_KEY
#define XRT_MODULE_UNICODE
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_RANDOM_SECURE
#include "xrt.h"
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
	return true;
}

int main(void)
{
	const char* sXml = "<catalog><book id=\"xlang\">XLang 编译器</book><book id=\"xrt\">XRT 运行库</book></catalog>";
	const char* sStopped = "<root><child/></root>";
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	xrtxmlsax* pPull;
	const xrtxmlevent* pEvent;
	int bOk;
	xrtXmlOptionsInit(&tOptions);
	bOk = xrtXmlSaxParseMemory(sXml, strlen(sXml), &tOptions, printEvent,
		NULL, &tError);
	if ( !bOk ) { fprintf(stderr, "%s\n", tError.sMessage); }
	pPull = xrtXmlSaxOpenMemory(sStopped, strlen(sStopped), &tOptions, &tError);
	if ( pPull == NULL ) { bOk = 0; }
	else {
		bOk = xrtXmlSaxNext(pPull, &pEvent, &tError) == XRT_XML_NEXT_EVENT &&
			pEvent != NULL && xrtXmlSaxClose(pPull, &tError) &&
			xrtXmlSaxNext(pPull, &pEvent, &tError) == XRT_XML_NEXT_END && bOk;
		xrtXmlSaxDestroy(pPull);
	}
	if ( bOk ) { puts("xml.sax C/XL example: PASS"); }
	return bOk ? 0 : 2;
}

/* 单文件范例入口；包导入时由清单分别编译这些源码。 */
#include "../../extlibs/xml.sax/xrt_xml_common.c"
#include "../../extlibs/xml.sax/xrt_xml_sax.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmlparse.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmlrole.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmltok.c"
#include "../../extlibs/_expat/expat_random.c"
