#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/xml.etree/include/xrt_xml_etree.h"

#include <stdio.h>



int main(void)
{
	xrtxmlelement* pCatalog;
	xrtxmlelement* pBook;
	xrtxmletreewriteoptions tOptions;
	xrtxmlerror tError;
	xbuffer* pText;
	int bOk;
	pCatalog = xrtXmlElementCreate((str)"catalog");
	pBook = xrtXmlElementAppend(pCatalog, (str)"book");
	bOk = pCatalog != NULL && pBook != NULL &&
		xrtXmlElementSetAttribute(pBook, (str)"id", (str)"xlang") &&
		xrtXmlElementSetText(pBook, (str)"XLang");
	xrtXmlElementWriteOptionsInit(&tOptions);
	tOptions.bPretty = true;
	tOptions.bXmlDeclaration = true;
	pText = bOk ? xrtXmlElementStringify(pCatalog, &tOptions, &tError) : NULL;
	bOk = bOk && pText != NULL;
	if ( pText != NULL ) { fwrite(pText->Data, 1u, pText->Size, stdout); }
	xrtBufferDestroy(pText);
	xrtXmlElementRelease(pBook);
	xrtXmlElementRelease(pCatalog);
	if ( bOk ) { puts("xml.etree C example: PASS"); }
	return bOk ? 0 : 2;
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
