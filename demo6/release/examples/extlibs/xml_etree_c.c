#include "../../extlibs/xml.etree/include/xrt_xml_etree.h"

#include <stdio.h>



int main(void)
{
	xrtxmlelement* pCatalog;
	xrtxmlelement* pBook;
	xrtxmletreewriteoptions tOptions;
	xrtxmlerror tError;
	xbuffer pText;
	int bOk;
	if ( xrtInit() == NULL ) { return 1; }
	pCatalog = xrtXmlElementCreate((str)"catalog");
	pBook = xrtXmlElementAppend(pCatalog, (str)"book");
	bOk = pCatalog != NULL && pBook != NULL &&
		xrtXmlElementSetAttribute(pBook, (str)"id", (str)"xlang") &&
		xrtXmlElementSetText(pBook, (str)"XLang");
	xrtXmlElementWriteOptionsInit(&tOptions);
	tOptions.bPretty = TRUE;
	tOptions.bXmlDeclaration = TRUE;
	pText = bOk ? xrtXmlElementStringify(pCatalog, &tOptions, &tError) : NULL;
	if ( pText != NULL ) { fwrite(pText->Buffer, 1u, pText->Length, stdout); }
	xrtBufferDestroy(pText);
	xrtXmlElementRelease(pBook);
	xrtXmlElementRelease(pCatalog);
	xrtUnit();
	if ( bOk && pText != NULL ) { puts("xml.etree C example: PASS"); }
	return bOk && pText != NULL ? 0 : 2;
}

/* 单文件范例入口；包导入时由清单分别编译源码。 */
#include "../../extlibs/xml.etree/xrt_xml_etree_tree.c"
#include "../../extlibs/xml.etree/xrt_xml_etree_parser.c"
#include "../../extlibs/xml.etree/xrt_xml_etree_writer.c"
#include "../../extlibs/xml.sax/xrt_xml_common.c"
#include "../../extlibs/xml.sax/xrt_xml_sax.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmlparse.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmlrole.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmltok.c"
#include "../../extlibs/_expat/expat_random.c"
