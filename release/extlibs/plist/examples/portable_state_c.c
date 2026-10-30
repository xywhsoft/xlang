/* 对应 XLang 配置保存流程；节点由 document 拥有，不单独释放借用节点。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_plist.h"
#include <stdio.h>
#include <string.h>

static bool core_fields(xrtplistdocument* document)
{
    xrtplistnode* root = xrtPlistRoot(document);
    xrtplistnode* player = xrtPlistDictGet(root, "player", 6);
    return player && xrtPlistNodeType(player) == XRT_PLIST_STRING && xrtPlistByteSize(player) == strlen("阿青") &&
        !memcmp(xrtPlistBytes(player), "阿青", strlen("阿青")) &&
        xrtPlistInt(xrtPlistDictGet(root, "score", 5)) == 42 && xrtPlistBool(xrtPlistDictGet(root, "enabled", 7));
}

int main(void)
{
    xrtplistdocument* source = xrtPlistCreate();
    xrtplistdocument* restored = NULL;
    xbuffer* xml = NULL;
    xbuffer* binary = NULL;
    xbuffer* rejected = NULL;
    xrtplistnode* root = source ? xrtPlistCreateDict(source) : NULL;
    xrtplistoptions options;
    xrtplisterror error;
    bool ok = false;
    xrtPlistErrorInit(&error);
    if (!root || !xrtPlistSetRoot(source, root) ||
        !xrtPlistDictSet(root, "player", 6, xrtPlistCreateString(source, (str)"阿青")) ||
        !xrtPlistDictSet(root, "score", 5, xrtPlistCreateInt(source, 42)) ||
        !xrtPlistDictSet(root, "enabled", 7, xrtPlistCreateBool(source, true))) goto done;
    xrtPlistOptionsInit(&options);
    options.iFormat = XRT_PLIST_FORMAT_XML;
    options.bSortKeys = true;
    xml = xrtPlistStringify(source, &options, &error);
    if (!xml || !(restored = xrtPlistParseMemory(xml->Data, xml->Size, NULL, &error)) || !core_fields(restored)) goto done;
    xrtPlistDestroy(restored);
    restored = NULL;
    if (!xrtPlistDictSet(root, "token", 5, xrtPlistCreateUid(source, 42)) ||
        !xrtPlistDictSet(root, "checkpoint", 10, xrtPlistCreateNull(source))) goto done;
    options.iFormat = XRT_PLIST_FORMAT_BINARY;
    binary = xrtPlistStringify(source, &options, &error);
    if (!binary || !(restored = xrtPlistParseMemory(binary->Data, binary->Size, NULL, &error)) || !core_fields(restored)) goto done;
    xrtplistnode* decoded = xrtPlistRoot(restored);
    xrtplistnode* token = xrtPlistDictGet(decoded, "token", 5);
    xrtplistnode* checkpoint = xrtPlistDictGet(decoded, "checkpoint", 10);
    if (!token || xrtPlistNodeType(token) != XRT_PLIST_UID || xrtPlistUid(token) != 42 ||
        !checkpoint || xrtPlistNodeType(checkpoint) != XRT_PLIST_NULL) goto done;
    options.iFormat = XRT_PLIST_FORMAT_XML;
    rejected = xrtPlistStringify(source, &options, &error);
    if (rejected || error.iCode != XRT_PLIST_ERROR_TYPE) goto done;
    xrtPlistDestroy(restored);
    xrtPlistOptionsInit(&options);
    options.tLimits.iMaxEntries = 1;
    restored = xrtPlistParseMemory(binary->Data, binary->Size, &options, &error);
    if (restored || error.iCode != XRT_PLIST_ERROR_LIMIT) goto done;
    restored = xrtPlistParseMemory(binary->Data, binary->Size, NULL, &error);
    if (!restored || !core_fields(restored)) goto done;
    puts("player=阿青; score=42\nencoding=xml,binary; uid=42; checkpoint=null\n"
        "type rejected=true; entry limit=true; recovery=true\nplist portable state: OK");
    ok = true;
done:
    if (!ok) fprintf(stderr, "plist portable state: %s\n", error.sMessage[0] ? error.sMessage : "unexpected result");
    xrtBufferDestroy(rejected);
    xrtBufferDestroy(binary);
    xrtBufferDestroy(xml);
    xrtPlistDestroy(restored);
    xrtPlistDestroy(source);
    xrtClearError();
    return ok ? 0 : 1;
}

#include "../xrt_plist_tree.c"
#include "../xrt_plist_io.c"
#include "../xrt_plist_xml.c"
#include "../xrt_plist_binary.c"
#include "../xrt_plist_xml_writer.c"
#include "../xrt_plist_binary_writer.c"
#include "../../xml.dom/xrt_xml_dom_tree.c"
#include "../../xml.dom/xrt_xml_dom_parser.c"
#include "../../xml.dom/xrt_xml_dom_writer.c"
#include "../../xml.sax/xrt_xml_common.c"
#include "../../xml.sax/xrt_xml_sax.c"
#include "../../_expat/vendor/expat/lib/xmlparse.c"
#include "../../_expat/vendor/expat/lib/xmlrole.c"
#include "../../_expat/vendor/expat/lib/xmltok.c"
#include "../../_expat/expat_random.c"
