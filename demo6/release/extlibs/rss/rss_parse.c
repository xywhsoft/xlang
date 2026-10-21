/* RSS 2.0 / Atom 解析。XML 语法与安全限制由 xml.dom/Expat 负责。 */
#include <string.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "../xml.dom/include/xrt_xml_dom.h"
#include "rss_native.h"

#define RSS_DOMAIN "xlang.extlib.rss"
#define RSS_ATOM_NS "http://www.w3.org/2005/Atom"

static void rss_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, RSS_DOMAIN, 0, message);
}

static int rss_name(const xrtxmldomnode *node, const char *name, int atom) {
    const char *actual;
    const char *ns;
    if (node == NULL || xrtXmlDomNodeType(node) != XRT_XML_DOM_ELEMENT_NODE) return 0;
    actual = (const char *)(atom ? xrtXmlDomNodeLocalName(node) : xrtXmlDomNodeName(node));
    ns = (const char *)xrtXmlDomNodeNamespaceUri(node);
    return actual != NULL && strcmp(actual, name) == 0 &&
        (atom ? (ns != NULL && strcmp(ns, RSS_ATOM_NS) == 0) :
                (ns == NULL || *ns == 0));
}

static xrtxmldomnode *rss_child(const xrtxmldomnode *parent,
    const char *name, int atom) {
    uint32 i, count = xrtXmlDomNodeChildCount(parent);
    for (i = 0; i < count; ++i) {
        xrtxmldomnode *child = xrtXmlDomNodeChild(parent, i);
        if (rss_name(child, name, atom)) return child;
    }
    return NULL;
}

/* SetNew 无论成功失败都消费 value；调用方只释放对象本身。 */
static int rss_put(xvalue *object, const char *key, xvalue *value) {
    xstrview name = {key, strlen(key)};
    return value != NULL && xrtValueObjectSetNew(object, name, value);
}

static int rss_put_string(xvalue *object, const char *key, const char *text) {
    if (text == NULL) text = "";
    return rss_put(object, key, xrtValueString((xstrview){text, strlen(text)}));
}

static int rss_put_text(xvalue *object, const char *key,
    const xrtxmldomnode *node) {
    str text;
    xvalue *value;
    if (node == NULL) return 1;
    text = xrtXmlDomTextContent(node);
    if (text == NULL) return 0;
    value = xrtValueStringTake(&text, strlen((const char *)text));
    xrtFree(text);
    return rss_put(object, key, value);
}

static int rss_put_child(xvalue *object, const char *key,
    const xrtxmldomnode *parent, const char *name, int atom) {
    return rss_put_text(object, key, rss_child(parent, name, atom));
}

static const char *rss_atom_link(const xrtxmldomnode *parent) {
    const char *first = NULL;
    uint32 i, count = xrtXmlDomNodeChildCount(parent);
    for (i = 0; i < count; ++i) {
        xrtxmldomnode *node = xrtXmlDomNodeChild(parent, i);
        const char *href, *rel;
        if (!rss_name(node, "link", 1)) continue;
        href = (const char *)xrtXmlDomGetAttribute(node, (str)"href");
        rel = (const char *)xrtXmlDomGetAttribute(node, (str)"rel");
        if (href == NULL) continue;
        if (first == NULL) first = href;
        if (rel != NULL && strcmp(rel, "alternate") == 0) return href;
    }
    return first != NULL ? first : "";
}

static xvalue *rss_item(const xrtxmldomnode *node) {
    xvalue *item = xrtValueObject();
    xvalue *categories;
    uint32 i, count = xrtXmlDomNodeChildCount(node);
    if (item == NULL) return NULL;
    if (!rss_put_child(item, "title", node, "title", 0) ||
        !rss_put_child(item, "link", node, "link", 0) ||
        !rss_put_child(item, "description", node, "description", 0) ||
        !rss_put_child(item, "pubDate", node, "pubDate", 0) ||
        !rss_put_child(item, "guid", node, "guid", 0) ||
        !rss_put_child(item, "author", node, "author", 0)) goto fail;
    categories = xrtValueArray();
    if (categories == NULL) goto fail;
    for (i = 0; i < count; ++i) {
        xrtxmldomnode *child = xrtXmlDomNodeChild(node, i);
        str text;
        xvalue *value;
        if (!rss_name(child, "category", 0)) continue;
        text = xrtXmlDomTextContent(child);
        if (text == NULL) { xrtValueRelease(categories); goto fail; }
        value = xrtValueStringTake(&text, strlen((const char *)text));
        xrtFree(text);
        if (value == NULL || !xrtValueArrayAppendNew(categories, value)) {
            xrtValueRelease(categories);
            goto fail;
        }
    }
    if (!rss_put(item, "categories", categories)) goto fail;
    return item;
fail:
    xrtValueRelease(item);
    return NULL;
}

static xvalue *rss_map_rss(const xrtxmldomnode *root) {
    xrtxmldomnode *channel = rss_child(root, "channel", 0);
    xvalue *result, *items;
    uint32 i, count;
    if (channel == NULL) { rss_error("rss feed has no channel element"); return NULL; }
    result = xrtValueObject();
    if (result == NULL) return NULL;
    if (!rss_put_string(result, "type", "rss") ||
        !rss_put_child(result, "title", channel, "title", 0) ||
        !rss_put_child(result, "link", channel, "link", 0) ||
        !rss_put_child(result, "description", channel, "description", 0) ||
        !rss_put_child(result, "updated", channel, "lastBuildDate", 0)) goto fail;
    items = xrtValueArray();
    if (items == NULL) goto fail;
    count = xrtXmlDomNodeChildCount(channel);
    for (i = 0; i < count; ++i) {
        xrtxmldomnode *child = xrtXmlDomNodeChild(channel, i);
        xvalue *item;
        if (!rss_name(child, "item", 0)) continue;
        item = rss_item(child);
        if (item == NULL || !xrtValueArrayAppendNew(items, item)) {
            xrtValueRelease(items);
            goto fail;
        }
    }
    if (!rss_put(result, "items", items)) goto fail;
    return result;
fail:
    xrtValueRelease(result);
    return NULL;
}

static xvalue *rss_entry(const xrtxmldomnode *node,
    const xrtxmldomnode *feedAuthor) {
    xvalue *item = xrtValueObject();
    const xrtxmldomnode *author;
    if (item == NULL) return NULL;
    author = rss_child(node, "author", 1);
    /* Atom 有效作者按 entry、source、feed 的次序继承。 */
    if (author == NULL) {
        xrtxmldomnode *source = rss_child(node, "source", 1);
        if (source != NULL) author = rss_child(source, "author", 1);
    }
    if (author == NULL) author = feedAuthor;
    if (!rss_put_child(item, "title", node, "title", 1) ||
        !rss_put_child(item, "id", node, "id", 1) ||
        !rss_put_child(item, "updated", node, "updated", 1) ||
        !rss_put_child(item, "summary", node, "summary", 1) ||
        !rss_put_child(item, "content", node, "content", 1) ||
        !rss_put_child(item, "author", author, "name", 1) ||
        !rss_put_string(item, "link", rss_atom_link(node))) {
        xrtValueRelease(item);
        return NULL;
    }
    return item;
}

static xvalue *rss_map_atom(const xrtxmldomnode *root) {
    xvalue *result = xrtValueObject();
    xvalue *items;
    xrtxmldomnode *author = rss_child(root, "author", 1);
    uint32 i, count;
    if (result == NULL) return NULL;
    if (!rss_put_string(result, "type", "atom") ||
        !rss_put_child(result, "title", root, "title", 1) ||
        !rss_put_child(result, "subtitle", root, "subtitle", 1) ||
        !rss_put_child(result, "updated", root, "updated", 1) ||
        !rss_put_child(result, "id", root, "id", 1) ||
        !rss_put_child(result, "author", author, "name", 1) ||
        !rss_put_string(result, "link", rss_atom_link(root))) goto fail;
    items = xrtValueArray();
    if (items == NULL) goto fail;
    count = xrtXmlDomNodeChildCount(root);
    for (i = 0; i < count; ++i) {
        xrtxmldomnode *child = xrtXmlDomNodeChild(root, i);
        xvalue *item;
        if (!rss_name(child, "entry", 1)) continue;
        item = rss_entry(child, author);
        if (item == NULL || !xrtValueArrayAppendNew(items, item)) {
            xrtValueRelease(items);
            goto fail;
        }
    }
    if (!rss_put(result, "items", items)) goto fail;
    return result;
fail:
    xrtValueRelease(result);
    return NULL;
}

static xrtxmldomnode *rss_document(const char *xml, int64 length) {
    xrtxmloptions options;
    xrtxmlerror error;
    xrtxmldomnode *document;
    xrtClearError();
    if (xml == NULL) { rss_error("null feed text"); return NULL; }
    if (length < 0 || length > 8 * 1024 * 1024) {
        rss_error("invalid feed length or feed exceeds 8 MiB limit");
        return NULL;
    }
    xrtXmlOptionsInit(&options);
    options.bPreserveComments = false;
    options.tLimits.iMaxInputBytes = 8u * 1024u * 1024u;
    options.tLimits.iMaxNodes = 100000u;
    options.tLimits.iMaxDepth = 64u;
    document = xrtXmlDomParseMemory(xml, (uint64)length, &options, &error);
    if (document == NULL && xrtGetError() == NULL) {
        if (error.iCode == XRT_XML_ERROR_MEMORY)
            xrtSetErrorInfo(XERR_MEMORY, RSS_DOMAIN, 0,
                "out of memory while parsing XML feed");
        else rss_error(error.sMessage[0] ? error.sMessage : "invalid XML feed");
    }
    return document;
}

static int rss_kind(const xrtxmldomnode *root) {
    if (rss_name(root, "rss", 0)) {
        const char *version = (const char *)xrtXmlDomGetAttribute(root, (str)"version");
        return version != NULL && strcmp(version, "2.0") == 0 ? 1 : 0;
    }
    if (rss_name(root, "feed", 1)) return 2;
    return 0;
}

const char *xrss_detect(const char *xml, int64 length) {
    xrtxmldomnode *document = rss_document(xml, length);
    int kind;
    if (document == NULL) {
        if (xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_MEMORY)
            xrtClearError();
        return "";
    }
    kind = rss_kind(xrtXmlDomDocumentElement(document));
    xrtXmlDomNodeRelease(document);
    return kind == 1 ? "rss" : kind == 2 ? "atom" : "";
}

static xvalue *rss_parse(const char *xml, int64 length, int required) {
    xrtxmldomnode *document = rss_document(xml, length);
    xrtxmldomnode *root;
    xvalue *result;
    int kind;
    if (document == NULL) return xrtValueNull();
    root = xrtXmlDomDocumentElement(document);
    kind = rss_kind(root);
    if (kind == 0 || (required != 0 && required != kind)) {
        rss_error(required == 1 ? "not an RSS 2.0 document" :
            required == 2 ? "not an Atom document" : "unsupported feed root");
        result = NULL;
    } else {
        result = kind == 1 ? rss_map_rss(root) : rss_map_atom(root);
    }
    xrtXmlDomNodeRelease(document);
    return result != NULL ? result : xrtValueNull();
}

xvalue *xrss_parse(const char *xml, int64 length) { return rss_parse(xml, length, 0); }
xvalue *xrss_parse_rss(const char *xml, int64 length) { return rss_parse(xml, length, 1); }
xvalue *xrss_parse_atom(const char *xml, int64 length) { return rss_parse(xml, length, 2); }
