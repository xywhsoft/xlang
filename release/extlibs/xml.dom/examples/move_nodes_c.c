/* 与 move_nodes.xl 相同的变更、拒绝、序列化和黄金输出；不使用语言桥接。 */
#define XRT_MODULE_ALL
#include "../include/xrt_xml_dom.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    xrtxmldomnode* document = xrtXmlDomCreateDocument();
    xrtxmldomnode* catalog = xrtXmlDomCreateElement(document, "catalog");
    xrtxmldomnode* text = xrtXmlDomCreateText(document, "你好");
    xrtxmldomnode* active = xrtXmlDomCreateElement(document, "active");
    xrtxmldomnode* archive = xrtXmlDomCreateElement(document, "archive");
    xrtxmldomnode* item = xrtXmlDomCreateElement(document, "item");
    xrtxmldomnode *parent = NULL, *restored = NULL, *root = NULL;
    xbuffer* encoded = NULL;
    str content = NULL;
    xrtxmlerror error;
    int status = 1;
    if (!document || !catalog || !text || !active || !archive || !item) goto done;
    if (xrtXmlDomAppendChild(catalog, document) || xrtXmlDomAppendChild(document, text)) goto done;
    parent = xrtXmlDomNodeParent(document);
    if (parent || xrtXmlDomNodeChildCount(document) || xrtXmlDomNodeChildCount(catalog)) goto done;
    if (!xrtXmlDomAppendChild(document, catalog) || !xrtXmlDomAppendChild(catalog, active) ||
        !xrtXmlDomAppendChild(catalog, archive) || !xrtXmlDomAppendChild(item, text) ||
        !xrtXmlDomAppendChild(active, item) || !xrtXmlDomAppendChild(archive, item)) goto done;
    parent = xrtXmlDomNodeParent(item);
    if (parent != archive || xrtXmlDomNodeChildCount(active) != 0 || xrtXmlDomNodeChildCount(archive) != 1) goto done;
    if (!xrtXmlDomAppendChild(archive, item) || xrtXmlDomNodeChildCount(archive) != 1) goto done;
    encoded = xrtXmlDomStringify(document, NULL, &error);
    const char* expected = "<catalog><active/><archive><item>你好</item></archive></catalog>";
    if (!encoded || encoded->Size != strlen(expected) || memcmp(encoded->Data, expected, encoded->Size)) goto done;
    restored = xrtXmlDomParseMemory(encoded->Data, encoded->Size, NULL, &error);
    root = xrtXmlDomDocumentElement(restored);
    content = xrtXmlDomTextContent(root);
    if (!content || strcmp(content, "你好")) goto done;
    printf("rejected: 2\nmoved: true\nsame parent: 1\nxml: %s\nroundtrip: true\n", expected);
    status = 0;
done:
    xrtFree(content); xrtBufferDestroy(encoded);
    xrtXmlDomNodeRelease(root); xrtXmlDomNodeRelease(restored); xrtXmlDomNodeRelease(parent);
    xrtXmlDomNodeRelease(item); xrtXmlDomNodeRelease(archive); xrtXmlDomNodeRelease(active);
    xrtXmlDomNodeRelease(text); xrtXmlDomNodeRelease(catalog); xrtXmlDomNodeRelease(document);
    return status;
}

#include "../xrt_xml_dom_tree.c"
#include "../xrt_xml_dom_parser.c"
#include "../xrt_xml_dom_writer.c"
#include "../../xml.sax/xrt_xml_common.c"
#include "../../xml.sax/xrt_xml_sax.c"
#include "../../_expat/vendor/expat/lib/xmlparse.c"
#include "../../_expat/vendor/expat/lib/xmlrole.c"
#include "../../_expat/vendor/expat/lib/xmltok.c"
#include "../../_expat/expat_random.c"
