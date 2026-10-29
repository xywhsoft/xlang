#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/plist/include/xrt_plist.h"

#include <stdio.h>



int main(int argc, char **argv)
{
	xrtplistdocument* pDocument;
	xrtplistdocument* pRestored = NULL;
	xrtplistnode* pRoot;
	xrtplistnode* pTags;
	xrtplistnode* pUid;
	xrtplistoptions tOptions;
	xrtplisterror tError;
	xbuffer* pXml;
	xbuffer* pBinary;
	const uint8 sIcon[] = { 0u, 'X', 'L' };
	bool bOk;
	if (argc != 1 && argc != 3) { return 2; }
	pDocument = xrtPlistCreate();
	pRoot = xrtPlistCreateDict(pDocument);
	pTags = xrtPlistCreateArray(pDocument);
	xrtPlistSetRoot(pDocument, pRoot);
	xrtPlistDictSet(pRoot, "application", 11u,
		xrtPlistCreateString(pDocument, (str)"xlang"));
	xrtPlistDictSet(pRoot, "version", 7u,
		xrtPlistCreateInt(pDocument, 6));
	xrtPlistDictSet(pRoot, "enabled", 7u,
		xrtPlistCreateBool(pDocument, true));
	xrtPlistDictSet(pRoot, "icon", 4u,
		xrtPlistCreateData(pDocument, sIcon, sizeof(sIcon)));
	xrtPlistArrayAppend(pTags,
		xrtPlistCreateString(pDocument, (str)"compiler"));
	xrtPlistArrayAppend(pTags,
		xrtPlistCreateString(pDocument, (str)"script"));
	xrtPlistDictSet(pRoot, "tags", 4u, pTags);
	xrtPlistOptionsInit(&tOptions);
	tOptions.iFormat = XRT_PLIST_FORMAT_XML;
	tOptions.bPretty = true;
	tOptions.bSortKeys = true;
	pXml = xrtPlistStringify(pDocument, &tOptions, &tError);
	bOk = pXml != NULL;
	if ( pXml != NULL ) { printf("%.*s\n", (int)pXml->Size, pXml->Data); }
	if ( bOk ) {
		bOk = xrtPlistDictSet(pRoot, "uid", 3u,
			xrtPlistCreateUid(pDocument, 42u));
	}
	tOptions.iFormat = XRT_PLIST_FORMAT_BINARY;
	pBinary = bOk ? xrtPlistStringify(pDocument, &tOptions, &tError) : NULL;
	bOk = bOk && pBinary != NULL;
	if ( pBinary != NULL ) { printf("binary bytes: %zu\n", pBinary->Size); }
	pRestored = pBinary != NULL ? xrtPlistParseMemory(pBinary->Data,
		pBinary->Size, NULL, &tError) : NULL;
	pUid = pRestored != NULL ? xrtPlistDictGet(xrtPlistRoot(pRestored),
		"uid", 3u) : NULL;
	bOk = bOk && pUid != NULL && xrtPlistNodeType(pUid) == XRT_PLIST_UID &&
		xrtPlistUid(pUid) == 42u;
	if ( bOk ) { printf("uid: %llu\n", (unsigned long long)xrtPlistUid(pUid)); }
	if ( bOk && argc == 3 ) {
		bOk = xrtFileWriteAll(argv[1],
			(xbytesview){ (const uint8*)pXml->Data, pXml->Size }) &&
			xrtFileWriteAll(argv[2],
				(xbytesview){ (const uint8*)pBinary->Data, pBinary->Size });
	}
	xrtBufferDestroy(pBinary);
	xrtBufferDestroy(pXml);
	xrtPlistDestroy(pRestored);
	xrtPlistDestroy(pDocument);
	return bOk ? 0 : 2;
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
