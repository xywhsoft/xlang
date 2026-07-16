#include "../../extlibs/plist/include/xrt_plist.h"

#include <stdio.h>



int main(void)
{
	xrtplistdocument* pDocument;
	xrtplistnode* pRoot;
	xrtplistnode* pTags;
	xrtplistoptions tOptions;
	xrtplisterror tError;
	xbuffer pXml;
	if ( xrtInit() == NULL ) { return 1; }
	pDocument = xrtPlistCreate();
	pRoot = xrtPlistCreateDict(pDocument);
	pTags = xrtPlistCreateArray(pDocument);
	xrtPlistSetRoot(pDocument, pRoot);
	xrtPlistDictSet(pRoot, "application", 11u,
		xrtPlistCreateString(pDocument, (str)"xlang"));
	xrtPlistDictSet(pRoot, "version", 7u,
		xrtPlistCreateInt(pDocument, 6));
	xrtPlistArrayAppend(pTags,
		xrtPlistCreateString(pDocument, (str)"compiler"));
	xrtPlistArrayAppend(pTags,
		xrtPlistCreateString(pDocument, (str)"script"));
	xrtPlistDictSet(pRoot, "tags", 4u, pTags);
	xrtPlistOptionsInit(&tOptions);
	tOptions.iFormat = XRT_PLIST_FORMAT_XML;
	tOptions.bPretty = TRUE;
	tOptions.bSortKeys = TRUE;
	pXml = xrtPlistStringify(pDocument, &tOptions, &tError);
	if ( pXml != NULL ) { printf("%.*s", (int)pXml->Length, pXml->Buffer); }
	xrtBufferDestroy(pXml);
	xrtPlistDestroy(pDocument);
	xrtUnit();
	return pXml != NULL ? 0 : 2;
}

/* xl run 可直接执行；包构建时不编译这一段。 */
#include "../../extlibs/plist/xrt_plist_tree.c"
#include "../../extlibs/plist/xrt_plist_io.c"
#include "../../extlibs/plist/xrt_plist_xml.c"
#include "../../extlibs/plist/xrt_plist_binary.c"
#include "../../extlibs/plist/xrt_plist_xml_writer.c"
#include "../../extlibs/plist/xrt_plist_binary_writer.c"
#include "../../extlibs/xml.dom/xrt_xml_dom_tree.c"
#include "../../extlibs/xml.dom/xrt_xml_dom_parser.c"
#include "../../extlibs/xml.dom/xrt_xml_dom_writer.c"
#include "../../extlibs/xml.sax/xrt_xml_common.c"
#include "../../extlibs/xml.sax/xrt_xml_sax.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmlparse.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmlrole.c"
#include "../../extlibs/_expat/vendor/expat/lib/xmltok.c"
#include "../../extlibs/_expat/expat_random.c"
