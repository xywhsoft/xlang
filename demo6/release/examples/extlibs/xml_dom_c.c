#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_STRING
#define XRT_MODULE_ONCE
#define XRT_MODULE_THREAD_KEY
#define XRT_MODULE_UNICODE
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../../extlibs/xml.dom/include/xrt_xml_dom.h"

#include <stdio.h>



int main(void)
{
	xrtxmldomnode* pDocument;
	xrtxmldomnode* pCatalog;
	xrtxmldomnode* pBook;
	xrtxmldomnode* pText;
	xrtxmldomwriteoptions tOptions;
	xrtxmlerror tError;
	xbuffer* pXml;
	bool bOk;
	pDocument = xrtXmlDomCreateDocument();
	pCatalog = xrtXmlDomCreateElement(pDocument, (str)"catalog");
	pBook = xrtXmlDomCreateElement(pDocument, (str)"book");
	pText = xrtXmlDomCreateText(pDocument, (str)"XLang DOM");
	xrtXmlDomSetAttribute(pBook, (str)"id", (str)"xlang");
	xrtXmlDomAppendChild(pDocument, pCatalog);
	xrtXmlDomAppendChild(pCatalog, pBook);
	xrtXmlDomAppendChild(pBook, pText);
	xrtXmlDomWriteOptionsInit(&tOptions);
	tOptions.bPretty = true;
	tOptions.bXmlDeclaration = true;
	pXml = xrtXmlDomStringify(pDocument, &tOptions, &tError);
	bOk = pXml != NULL;
	if ( pXml != NULL ) { printf("%.*s", (int)pXml->Size, pXml->Data); }
	xrtBufferDestroy(pXml);
	xrtXmlDomNodeRelease(pText);
	xrtXmlDomNodeRelease(pBook);
	xrtXmlDomNodeRelease(pCatalog);
	xrtXmlDomNodeRelease(pDocument);
	return bOk ? 0 : 2;
}

/* xl run 可直接执行；包构建时不编译这一段。 */
#include "../../extlibs/xml.dom/xrt_xml_dom_tree.c"
#include "../../extlibs/xml.dom/xrt_xml_dom_parser.c"
#include "../../extlibs/xml.dom/xrt_xml_dom_writer.c"
#include "../../extlibs/xml.sax/xrt_xml_common.c"
#include "../../extlibs/xml.sax/xrt_xml_sax.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmlparse.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmlrole.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmltok.c"
#include "../../extlibs/_expat/expat_random.c"
