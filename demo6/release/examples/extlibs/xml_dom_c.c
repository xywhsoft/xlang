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
	xbuffer pXml;
	if ( xrtInit() == NULL ) { return 1; }
	pDocument = xrtXmlDomCreateDocument();
	pCatalog = xrtXmlDomCreateElement(pDocument, (str)"catalog");
	pBook = xrtXmlDomCreateElement(pDocument, (str)"book");
	pText = xrtXmlDomCreateText(pDocument, (str)"XLang DOM");
	xrtXmlDomSetAttribute(pBook, (str)"id", (str)"xlang");
	xrtXmlDomAppendChild(pDocument, pCatalog);
	xrtXmlDomAppendChild(pCatalog, pBook);
	xrtXmlDomAppendChild(pBook, pText);
	xrtXmlDomWriteOptionsInit(&tOptions);
	tOptions.bPretty = TRUE;
	tOptions.bXmlDeclaration = TRUE;
	pXml = xrtXmlDomStringify(pDocument, &tOptions, &tError);
	if ( pXml != NULL ) { printf("%.*s", (int)pXml->Length, pXml->Buffer); }
	xrtBufferDestroy(pXml);
	xrtXmlDomNodeRelease(pText);
	xrtXmlDomNodeRelease(pBook);
	xrtXmlDomNodeRelease(pCatalog);
	xrtXmlDomNodeRelease(pDocument);
	xrtUnit();
	return pXml != NULL ? 0 : 2;
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
