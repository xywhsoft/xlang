/*
 * rss_native.c - RSS 2.0 and Atom feed parsing and generation.
 *
 * The parser is a small dedicated XML reader covering what feeds need:
 * elements, attributes, character entities, CDATA sections, comments,
 * processing instructions, and DOCTYPE skipping. Parsed results become
 * xvalue objects (xlang var); generated feeds are written into XRT
 * temporary memory and returned as strings.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XRSS_DOMAIN "xlang.extlib.rss"
#define XRSS_MAX_DEPTH 32
#define XRSS_MAX_ATTRS 16
#define XRSS_MAX_KIDS 512
#define XRSS_TAG_MAX 96

static void xrss_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XRSS_DOMAIN, 0, message);
}

/* ------------------------------------------------------------------ */
/* Lightweight DOM                                                     */
/* ------------------------------------------------------------------ */

typedef struct XrssNode {
    char tag[XRSS_TAG_MAX];
    char *attrs[XRSS_MAX_ATTRS * 2];
    int nattrs;
    char *text;
    size_t textLen;
    size_t textCap;
    struct XrssNode *kids[XRSS_MAX_KIDS];
    int nkids;
} XrssNode;

static XrssNode *xrss_node_new(const char *tag, size_t tagLen) {
    XrssNode *node = (XrssNode *)xrtMalloc(sizeof(XrssNode));
    if (node == NULL) {
        return NULL;
    }
    memset(node, 0, sizeof(*node));
    if (tagLen >= XRSS_TAG_MAX) {
        tagLen = XRSS_TAG_MAX - 1;
    }
    memcpy(node->tag, tag, tagLen);
    node->tag[tagLen] = '\0';
    return node;
}

static void xrss_node_free(XrssNode *node) {
    int i;
    if (node == NULL) {
        return;
    }
    for (i = 0; i < node->nattrs * 2; ++i) {
        xrtFree(node->attrs[i]);
    }
    for (i = 0; i < node->nkids; ++i) {
        xrss_node_free(node->kids[i]);
    }
    xrtFree(node->text);
    xrtFree(node);
}

static int xrss_text_append(XrssNode *node, const char *data, size_t len) {
    if (len == 0) {
        return 1;
    }
    if (node->textLen + len + 1 > node->textCap) {
        size_t cap = node->textCap ? node->textCap * 2 : 64;
        char *grown;
        while (cap < node->textLen + len + 1) {
            cap *= 2;
        }
        grown = (char *)xrtMalloc(cap);
        if (grown == NULL) {
            return 0;
        }
        if (node->text != NULL) {
            memcpy(grown, node->text, node->textLen);
            xrtFree(node->text);
        }
        node->text = grown;
        node->textCap = cap;
    }
    memcpy(node->text + node->textLen, data, len);
    node->textLen += len;
    node->text[node->textLen] = '\0';
    return 1;
}

static int xrss_node_add_child(XrssNode *parent, XrssNode *child) {
    if (parent->nkids >= XRSS_MAX_KIDS) {
        xrss_node_free(child);
        return 0;
    }
    parent->kids[parent->nkids++] = child;
    return 1;
}

static const char *xrss_node_attr(const XrssNode *node, const char *name) {
    int i;
    for (i = 0; i < node->nattrs; ++i) {
        if (strcmp(node->attrs[i * 2], name) == 0) {
            return node->attrs[i * 2 + 1];
        }
    }
    return NULL;
}

static XrssNode *xrss_child(const XrssNode *node, const char *tag) {
    int i;
    if (node == NULL) {
        return NULL;
    }
    for (i = 0; i < node->nkids; ++i) {
        if (strcmp(node->kids[i]->tag, tag) == 0) {
            return node->kids[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Mini XML parser                                                     */
/* ------------------------------------------------------------------ */

static int xrss_is_space(int c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* Decode entities from [p, p+len) appending to node text. */
static int xrss_append_decoded(XrssNode *node, const char *p, size_t len) {
    size_t i = 0;
    while (i < len) {
        if (p[i] == '&') {
            size_t j = i + 1;
            size_t end = i;
            while (j < len && j - i <= 10 && p[j] != ';' && p[j] != '&') {
                j++;
            }
            if (j < len && p[j] == ';') {
                end = j;
            }
            if (end > i) {
                char name[12];
                size_t nameLen = end - i - 1;
                if (nameLen < sizeof(name)) {
                    memcpy(name, p + i + 1, nameLen);
                    name[nameLen] = '\0';
                    if (name[0] == '#') {
                        long code = -1;
                        char out[5];
                        int outLen = 0;
                        if (name[1] == 'x' || name[1] == 'X') {
                            code = strtol(name + 2, NULL, 16);
                        } else {
                            code = strtol(name + 1, NULL, 10);
                        }
                        if (code == 0) {
                            outLen = 0;
                        } else if (code < 0x80) {
                            out[0] = (char)code;
                            outLen = 1;
                        } else if (code < 0x800) {
                            out[0] = (char)(0xC0 | (code >> 6));
                            out[1] = (char)(0x80 | (code & 0x3F));
                            outLen = 2;
                        } else {
                            out[0] = (char)(0xE0 | (code >> 12));
                            out[1] = (char)(0x80 | ((code >> 6) & 0x3F));
                            out[2] = (char)(0x80 | (code & 0x3F));
                            outLen = 3;
                        }
                        if (!xrss_text_append(node, out, (size_t)outLen)) {
                            return 0;
                        }
                        i = end + 1;
                        continue;
                    }
                    if (strcmp(name, "amp") == 0) {
                        if (!xrss_text_append(node, "&", 1)) return 0;
                        i = end + 1;
                        continue;
                    }
                    if (strcmp(name, "lt") == 0) {
                        if (!xrss_text_append(node, "<", 1)) return 0;
                        i = end + 1;
                        continue;
                    }
                    if (strcmp(name, "gt") == 0) {
                        if (!xrss_text_append(node, ">", 1)) return 0;
                        i = end + 1;
                        continue;
                    }
                    if (strcmp(name, "quot") == 0) {
                        if (!xrss_text_append(node, "\"", 1)) return 0;
                        i = end + 1;
                        continue;
                    }
                    if (strcmp(name, "apos") == 0) {
                        if (!xrss_text_append(node, "'", 1)) return 0;
                        i = end + 1;
                        continue;
                    }
                }
            }
        }
        if (!xrss_text_append(node, p + i, 1)) {
            return 0;
        }
        i++;
    }
    return 1;
}

typedef struct {
    const char *xml;
    size_t len;
    size_t pos;
    int failed;
} XrssParser;

static int xrss_starts_with_at(XrssParser *p, const char *prefix) {
    size_t n = strlen(prefix);
    return p->len - p->pos >= n && memcmp(p->xml + p->pos, prefix, n) == 0;
}

/* Parse one tag starting after '<'. Returns 1 for open, 2 for self-closed,
   0 for close (tag copied into out), -1 on failure. */
static int xrss_parse_tag(XrssParser *p, char *out, size_t outCap,
    XrssNode *nodeForAttrs) {
    const char *s = p->xml;
    size_t i = p->pos;
    int closing = 0;
    size_t tagLen = 0;
    if (i < p->len && s[i] == '/') {
        closing = 1;
        i++;
    }
    while (i < p->len && (isalnum((unsigned char)s[i]) || s[i] == ':' ||
           s[i] == '_' || s[i] == '-')) {
        if (tagLen < outCap - 1) {
            out[tagLen++] = s[i];
        }
        i++;
    }
    out[tagLen] = '\0';
    if (tagLen == 0) {
        return -1;
    }
    /* attributes */
    while (i < p->len && s[i] != '>' && s[i] != '/') {
        char name[XRSS_TAG_MAX];
        char value[2048];
        size_t nLen = 0;
        size_t vLen = 0;
        int quote;
        while (i < p->len && xrss_is_space((unsigned char)s[i])) {
            i++;
        }
        if (i >= p->len || s[i] == '>' || s[i] == '/') {
            break;
        }
        while (i < p->len && s[i] != '=' && s[i] != '>' && s[i] != '/' &&
               !xrss_is_space((unsigned char)s[i])) {
            if (nLen < sizeof(name) - 1) {
                name[nLen++] = s[i];
            }
            i++;
        }
        name[nLen] = '\0';
        while (i < p->len && xrss_is_space((unsigned char)s[i])) {
            i++;
        }
        if (i >= p->len || s[i] != '=') {
            continue;
        }
        i++;
        while (i < p->len && xrss_is_space((unsigned char)s[i])) {
            i++;
        }
        if (i >= p->len || (s[i] != '"' && s[i] != '\'')) {
            return -1;
        }
        quote = s[i];
        i++;
        while (i < p->len && s[i] != quote) {
            if (vLen < sizeof(value) - 1) {
                value[vLen++] = s[i];
            }
            i++;
        }
        if (i >= p->len) {
            return -1;
        }
        i++; /* closing quote */
        value[vLen] = '\0';
        if (nodeForAttrs != NULL && nodeForAttrs->nattrs < XRSS_MAX_ATTRS) {
            char *nameCopy = (char *)xrtMalloc(nLen + 1);
            char *valueCopy = (char *)xrtMalloc(vLen + 1);
            if (nameCopy == NULL || valueCopy == NULL) {
                xrtFree(nameCopy);
                xrtFree(valueCopy);
                return -1;
            }
            memcpy(nameCopy, name, nLen + 1);
            memcpy(valueCopy, value, vLen + 1);
            nodeForAttrs->attrs[nodeForAttrs->nattrs * 2] = nameCopy;
            nodeForAttrs->attrs[nodeForAttrs->nattrs * 2 + 1] = valueCopy;
            nodeForAttrs->nattrs++;
        }
    }
    if (i >= p->len) {
        return -1;
    }
    if (s[i] == '/') {
        i++;
        if (i >= p->len || s[i] != '>') {
            return -1;
        }
        i++;
        p->pos = i;
        return closing ? 0 : 2;
    }
    if (s[i] != '>') {
        return -1;
    }
    i++;
    p->pos = i;
    return closing ? 0 : 1;
}

static XrssNode *xrss_parse_xml(const char *xml) {
    XrssParser parser;
    XrssNode *stack[XRSS_MAX_DEPTH];
    XrssNode *root;
    XrssNode *node;
    XrssNode *current;
    size_t textStart;
    char tag[XRSS_TAG_MAX];
    int depth;
    int done;

    root = NULL;
    current = NULL;
    depth = 0;
    done = 0;
    textStart = 0;
    tag[0] = '\0';

    parser.xml = xml;
    parser.len = xml ? strlen(xml) : 0;
    parser.pos = 0;
    parser.failed = 0;

    while (!done && parser.pos < parser.len) {
        if (parser.xml[parser.pos] != '<') {
            /* plain text run; flushed when the next tag opens */
            if (current == NULL) {
                parser.pos++;
                textStart = parser.pos;
            } else {
                while (parser.pos < parser.len &&
                       parser.xml[parser.pos] != '<') {
                    parser.pos++;
                }
            }
            continue;
        }
        /* flush pending character data into the open element */
        if (current != NULL && parser.pos > textStart) {
            if (!xrss_append_decoded(current, parser.xml + textStart,
                    parser.pos - textStart)) {
                parser.failed = 1;
            }
        }
        if (!parser.failed && xrss_starts_with_at(&parser, "<!--")) {
            const char *end = strstr(parser.xml + parser.pos, "-->");
            if (end == NULL) {
                parser.failed = 1;
            } else {
                parser.pos = (size_t)(end - parser.xml) + 3;
                textStart = parser.pos;
            }
        } else if (!parser.failed && xrss_starts_with_at(&parser, "<![CDATA[")) {
            const char *start = parser.xml + parser.pos + 9;
            const char *end = strstr(start, "]]>");
            if (end == NULL) {
                parser.failed = 1;
            } else if (current == NULL ||
                       !xrss_text_append(current, start,
                           (size_t)(end - start))) {
                parser.failed = 1;
            } else {
                parser.pos = (size_t)(end - parser.xml) + 3;
                textStart = parser.pos;
            }
        } else if (!parser.failed && xrss_starts_with_at(&parser, "<?")) {
            const char *end = strstr(parser.xml + parser.pos, "?>");
            if (end == NULL) {
                parser.failed = 1;
            } else {
                parser.pos = (size_t)(end - parser.xml) + 2;
                textStart = parser.pos;
            }
        } else if (!parser.failed && xrss_starts_with_at(&parser, "<!")) {
            const char *end = strchr(parser.xml + parser.pos, '>');
            if (end == NULL) {
                parser.failed = 1;
            } else {
                parser.pos = (size_t)(end - parser.xml) + 1;
                textStart = parser.pos;
            }
        } else if (!parser.failed) {
            size_t peek;
            size_t nameEnd;
            int kind;
            peek = parser.pos + 1;
            if (peek < parser.len && parser.xml[peek] == '/') {
                peek++;
            }
            nameEnd = peek;
            while (nameEnd < parser.len &&
                   !xrss_is_space((unsigned char)parser.xml[nameEnd]) &&
                   parser.xml[nameEnd] != '>' &&
                   parser.xml[nameEnd] != '/') {
                nameEnd++;
            }
            node = NULL;
            if (nameEnd > peek) {
                node = xrss_node_new(parser.xml + peek, nameEnd - peek);
                if (node == NULL) {
                    parser.failed = 1;
                }
            }
            if (!parser.failed) {
                parser.pos++;
                kind = xrss_parse_tag(&parser, tag, sizeof(tag), node);
                if (kind == -1) {
                    xrss_node_free(node);
                    node = NULL;
                    parser.failed = 1;
                } else if (kind == 0) {
                    /* closing tag */
                    xrss_node_free(node);
                    node = NULL;
                    if (current == NULL || strcmp(current->tag, tag) != 0) {
                        parser.failed = 1;
                    } else {
                        depth--;
                        current = depth > 0 ? stack[depth - 1] : NULL;
                        if (depth == 0) {
                            root = stack[0];
                            done = 1;
                        }
                    }
                } else {
                    /* open or self-closed tag */
                    if (depth >= XRSS_MAX_DEPTH) {
                        xrss_node_free(node);
                        node = NULL;
                        parser.failed = 1;
                    } else if (current != NULL) {
                        if (!xrss_node_add_child(current, node)) {
                            node = NULL;
                            parser.failed = 1;
                        }
                    } else if (root != NULL) {
                        xrss_node_free(node);
                        node = NULL;
                        parser.failed = 1;
                    } else {
                        root = node;
                    }
                    if (!parser.failed) {
                        stack[depth] = node;
                        depth++;
                        if (current == NULL) {
                            /* root just opened */
                        }
                        current = node;
                        if (depth == 1 && kind == 2) {
                            /* self-closed root */
                            current = NULL;
                            depth = 0;
                            done = 1;
                        } else if (kind == 2) {
                            depth--;
                            current = depth > 0 ? stack[depth - 1] : NULL;
                            if (depth == 0) {
                                done = 1;
                            }
                        }
                    }
                }
                if (!done) {
                    textStart = parser.pos;
                }
            }
        }
    }

    if (parser.failed) {
        xrss_node_free(root);
        return NULL;
    }
    if (root == NULL) {
        return NULL;
    }
    return root;
}

/* ------------------------------------------------------------------ */
/* xvalue helpers                                                      */
/* ------------------------------------------------------------------ */

/* Build a string xvalue from C text. Avoids struct-by-value ABI risk by
   copying into an XRT buffer and transferring ownership, like bson. */
static xvalue *xrss_text_value(const char *text, size_t len) {
    str owned = (str)xrtMalloc(len + 1);
    if (owned == NULL) {
        return NULL;
    }
    if (len > 0) {
        memcpy(owned, text, len);
    }
    owned[len] = '\0';
    {
        xvalue *value = xrtValueStringTake(&owned, len);
        if (value == NULL && owned != NULL) {
            xrtFree(owned);
        }
        return value;
    }
}

static int xrss_set_str(xvalue *obj, const char *key, const char *text) {
    xvalue *value = xrss_text_value(text ? text : "",
        text ? strlen(text) : 0);
    if (value == NULL) {
        return 0;
    }
    {
        xstrview view;
        view.Data = key;
        view.Size = strlen(key);
        return xrtValueObjectSetNew(obj, view, value);
    }
}

static int xrss_set_node_text(xvalue *obj, const char *key,
    const XrssNode *node) {
    if (node == NULL) {
        return 1;
    }
    return xrss_set_str(obj, key, node->text ? node->text : "");
}

/* ------------------------------------------------------------------ */
/* RSS 2.0 / Atom mapping                                              */
/* ------------------------------------------------------------------ */

static xvalue *xrss_build_item(const XrssNode *itemNode) {
    xvalue *item = xrtValueObject();
    int i;
    if (item == NULL) {
        return NULL;
    }
    xrss_set_node_text(item, "title", xrss_child(itemNode, "title"));
    xrss_set_node_text(item, "link", xrss_child(itemNode, "link"));
    xrss_set_node_text(item, "description",
        xrss_child(itemNode, "description"));
    xrss_set_node_text(item, "pubDate", xrss_child(itemNode, "pubDate"));
    xrss_set_node_text(item, "guid", xrss_child(itemNode, "guid"));
    xrss_set_node_text(item, "author", xrss_child(itemNode, "author"));
    {
        xvalue *categories = xrtValueArray();
        if (categories == NULL) {
            xrtValueRelease(item);
            return NULL;
        }
        for (i = 0; i < itemNode->nkids; ++i) {
            if (strcmp(itemNode->kids[i]->tag, "category") == 0) {
                const char *cat = itemNode->kids[i]->text
                    ? itemNode->kids[i]->text : "";
                if (!xrtValueArrayAppendNew(categories,
                        xrss_text_value(cat, strlen(cat)))) {
                    xrtValueRelease(categories);
                    xrtValueRelease(item);
                    return NULL;
                }
            }
        }
        {
            xstrview view;
            view.Data = "categories";
            view.Size = strlen("categories");
            if (!xrtValueObjectSetNew(item, view, categories)) {
                xrtValueRelease(categories);
                xrtValueRelease(item);
                return NULL;
            }
        }
    }
    return item;
}

static xvalue *xrss_map_rss(const XrssNode *root) {
    const XrssNode *channel = xrss_child(root, "channel");
    xvalue *result;
    xvalue *items;
    int i;
    if (channel == NULL) {
        xrss_error("rss feed has no channel element");
        return NULL;
    }
    result = xrtValueObject();
    if (result == NULL) {
        return NULL;
    }
    xrss_set_str(result, "type", "rss");
    xrss_set_node_text(result, "title", xrss_child(channel, "title"));
    xrss_set_node_text(result, "link", xrss_child(channel, "link"));
    xrss_set_node_text(result, "description",
        xrss_child(channel, "description"));
    xrss_set_node_text(result, "updated", xrss_child(channel, "lastBuildDate"));
    items = xrtValueArray();
    if (items == NULL) {
        xrtValueRelease(result);
        return NULL;
    }
    for (i = 0; i < channel->nkids; ++i) {
        if (strcmp(channel->kids[i]->tag, "item") == 0) {
            xvalue *item = xrss_build_item(channel->kids[i]);
            if (item == NULL || !xrtValueArrayAppendNew(items, item)) {
                if (item != NULL) xrtValueRelease(item);
                xrtValueRelease(items);
                xrtValueRelease(result);
                return NULL;
            }
        }
    }
    {
        xstrview view;
        view.Data = "items";
        view.Size = strlen("items");
        if (!xrtValueObjectSetNew(result, view, items)) {
            xrtValueRelease(items);
            xrtValueRelease(result);
            return NULL;
        }
    }
    return result;
}

static xvalue *xrss_map_atom(const XrssNode *root) {
    xvalue *result;
    xvalue *entries;
    int i;
    result = xrtValueObject();
    if (result == NULL) {
        return NULL;
    }
    xrss_set_str(result, "type", "atom");
    xrss_set_node_text(result, "title", xrss_child(root, "title"));
    xrss_set_node_text(result, "subtitle", xrss_child(root, "subtitle"));
    xrss_set_node_text(result, "updated", xrss_child(root, "updated"));
    xrss_set_node_text(result, "id", xrss_child(root, "id"));
    {
        /* link: prefer rel=alternate, else the first link href */
        const char *href = NULL;
        for (i = 0; i < root->nkids; ++i) {
            if (strcmp(root->kids[i]->tag, "link") == 0) {
                const char *rel = xrss_node_attr(root->kids[i], "rel");
                const char *candidate = xrss_node_attr(root->kids[i], "href");
                if (candidate != NULL &&
                    (rel == NULL || strcmp(rel, "alternate") == 0)) {
                    href = candidate;
                    if (rel != NULL) {
                        break;
                    }
                }
            }
        }
        xrss_set_str(result, "link", href ? href : "");
    }
    entries = xrtValueArray();
    if (entries == NULL) {
        xrtValueRelease(result);
        return NULL;
    }
    for (i = 0; i < root->nkids; ++i) {
        if (strcmp(root->kids[i]->tag, "entry") == 0) {
            const XrssNode *entry = root->kids[i];
            xvalue *item = xrtValueObject();
            int j;
            if (item == NULL) {
                xrtValueRelease(entries);
                xrtValueRelease(result);
                return NULL;
            }
            xrss_set_node_text(item, "title", xrss_child(entry, "title"));
            xrss_set_node_text(item, "id", xrss_child(entry, "id"));
            xrss_set_node_text(item, "updated", xrss_child(entry, "updated"));
            xrss_set_node_text(item, "summary", xrss_child(entry, "summary"));
            xrss_set_node_text(item, "content", xrss_child(entry, "content"));
            {
                const XrssNode *author = xrss_child(entry, "author");
                xrss_set_node_text(item, "author",
                    author != NULL ? xrss_child(author, "name") : NULL);
            }
            {
                const char *href = NULL;
                for (j = 0; j < entry->nkids; ++j) {
                    if (strcmp(entry->kids[j]->tag, "link") == 0) {
                        const char *rel = xrss_node_attr(entry->kids[j], "rel");
                        const char *candidate =
                            xrss_node_attr(entry->kids[j], "href");
                        if (candidate != NULL &&
                            (rel == NULL || strcmp(rel, "alternate") == 0)) {
                            href = candidate;
                            if (rel != NULL) {
                                break;
                            }
                        }
                    }
                }
                xrss_set_str(item, "link", href ? href : "");
            }
            if (!xrtValueArrayAppendNew(entries, item)) {
                xrtValueRelease(item);
                xrtValueRelease(entries);
                xrtValueRelease(result);
                return NULL;
            }
        }
    }
    {
        xstrview view;
        view.Data = "items";
        view.Size = strlen("items");
        if (!xrtValueObjectSetNew(result, view, entries)) {
            xrtValueRelease(entries);
            xrtValueRelease(result);
            return NULL;
        }
    }
    return result;
}

static int xrss_is_rss(const char *xml) {
    const char *p = xml;
    while (p != NULL && *p != '\0') {
        p = strstr(p, "<rss");
        if (p != NULL) {
            return 1;
        }
    }
    return xml != NULL && strstr(xml, "<channel") != NULL;
}

static int xrss_is_atom(const char *xml) {
    return xml != NULL && strstr(xml, "<feed") != NULL;
}

const char *xrss_detect(const char *xml) {
    if (xml == NULL) {
        return "";
    }
    if (xrss_is_rss(xml)) {
        return "rss";
    }
    if (xrss_is_atom(xml)) {
        return "atom";
    }
    return "";
}

xvalue *xrss_parse_rss(const char *xml) {
    XrssNode *root = xrss_parse_xml(xml);
    xvalue *result;
    if (root == NULL) {
        xrss_error("invalid rss xml");
        return xrtValueNull();
    }
    if (strcmp(root->tag, "rss") != 0) {
        xrss_node_free(root);
        xrss_error("not an rss 2.0 document");
        return xrtValueNull();
    }
    result = xrss_map_rss(root);
    xrss_node_free(root);
    return result != NULL ? result : xrtValueNull();
}

xvalue *xrss_parse_atom(const char *xml) {
    XrssNode *root = xrss_parse_xml(xml);
    xvalue *result;
    if (root == NULL) {
        xrss_error("invalid atom xml");
        return xrtValueNull();
    }
    if (strcmp(root->tag, "feed") != 0) {
        xrss_node_free(root);
        xrss_error("not an atom document");
        return xrtValueNull();
    }
    result = xrss_map_atom(root);
    xrss_node_free(root);
    return result != NULL ? result : xrtValueNull();
}

xvalue *xrss_parse(const char *xml) {
    const char *kind = xrss_detect(xml);
    if (strcmp(kind, "rss") == 0) {
        return xrss_parse_rss(xml);
    }
    if (strcmp(kind, "atom") == 0) {
        return xrss_parse_atom(xml);
    }
    xrss_error("input is neither rss nor atom");
    return xrtValueNull();
}

/* ------------------------------------------------------------------ */
/* Feed generation                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} XrssBuf;

static int xrss_buf_reserve(XrssBuf *b, size_t extra) {
    if (b->len + extra + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 1024;
        char *grown;
        while (cap < b->len + extra + 1) {
            cap *= 2;
        }
        grown = (char *)xrtTemp(cap);
        if (grown == NULL) {
            return 0;
        }
        memcpy(grown, b->data, b->len);
        grown[b->len] = '\0';
        b->data = grown;
        b->cap = cap;
    }
    return 1;
}

static int xrss_buf_put(XrssBuf *b, const char *text) {
    size_t n = strlen(text);
    if (!xrss_buf_reserve(b, n)) {
        return 0;
    }
    memcpy(b->data + b->len, text, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 1;
}

static int xrss_buf_put_escaped(XrssBuf *b, const char *text) {
    const char *p = text ? text : "";
    for (; *p != '\0'; ++p) {
        switch (*p) {
            case '&': if (!xrss_buf_put(b, "&amp;")) return 0; break;
            case '<': if (!xrss_buf_put(b, "&lt;")) return 0; break;
            case '>': if (!xrss_buf_put(b, "&gt;")) return 0; break;
            case '"': if (!xrss_buf_put(b, "&quot;")) return 0; break;
            case '\'': if (!xrss_buf_put(b, "&apos;")) return 0; break;
            default:
                if (!xrss_buf_reserve(b, 1)) return 0;
                b->data[b->len++] = *p;
                b->data[b->len] = '\0';
        }
    }
    return 1;
}

/* Read a string field from an object into out (ring of 4). */
static int xrss_get_str(xvalue *obj, const char *key, const char **out) {
    static char ring[4][2048];
    static int next;
    char *slot = ring[next];
    xvalue *value;
    xstrview text;
    next = (next + 1) % 4;
    slot[0] = '\0';
    *out = slot;
    if (obj == NULL) {
        return 0;
    }
    {
        int64 i;
        int64 n = xrtValueCount(obj);
        size_t len = strlen(key);
        value = NULL;
        for (i = 0; i < n; ++i) {
            xstrview k;
            xvalue *v = xrtValueObjectAt(obj, i, &k);
            if (v != NULL && k.Size == len && memcmp(k.Data, key, len) == 0) {
                value = v;
                break;
            }
        }
    }
    if (value == NULL || !xrtValueGetString(value, &text)) {
        return 0;
    }
    if (text.Size >= 2048) {
        text.Size = 2047;
    }
    memcpy(slot, text.Data, text.Size);
    slot[text.Size] = '\0';
    return 1;
}

const char *xrss_build_rss(xvalue *channel, xvalue *items) {
    static const char *prologue =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rss version=\"2.0\">\n<channel>\n";
    XrssBuf buf = {NULL, 0, 0};
    const char *s;
    int64 i;
    int64 n;
    if (!xrss_buf_put(&buf, prologue)) {
        return "";
    }
    xrss_get_str(channel, "title", &s);
    if (!xrss_buf_put(&buf, "  <title>") ||
        !xrss_buf_put_escaped(&buf, s) ||
        !xrss_buf_put(&buf, "</title>\n")) {
        return "";
    }
    xrss_get_str(channel, "link", &s);
    if (!xrss_buf_put(&buf, "  <link>") ||
        !xrss_buf_put_escaped(&buf, s) ||
        !xrss_buf_put(&buf, "</link>\n")) {
        return "";
    }
    xrss_get_str(channel, "description", &s);
    if (!xrss_buf_put(&buf, "  <description>") ||
        !xrss_buf_put_escaped(&buf, s) ||
        !xrss_buf_put(&buf, "</description>\n")) {
        return "";
    }
    xrss_get_str(channel, "updated", &s);
    if (s[0] != '\0') {
        if (!xrss_buf_put(&buf, "  <lastBuildDate>") ||
            !xrss_buf_put_escaped(&buf, s) ||
            !xrss_buf_put(&buf, "</lastBuildDate>\n")) {
            return "";
        }
    }
    n = items != NULL ? xrtValueCount(items) : 0;
    for (i = 0; i < n; ++i) {
        xvalue *item = xrtValueArrayAt(items, i);
        if (item == NULL) {
            continue;
        }
        if (!xrss_buf_put(&buf, "  <item>\n")) {
            return "";
        }
        xrss_get_str(item, "title", &s);
        if (s[0] != '\0') {
            if (!xrss_buf_put(&buf, "    <title>") ||
                !xrss_buf_put_escaped(&buf, s) ||
                !xrss_buf_put(&buf, "</title>\n")) {
                return "";
            }
        }
        xrss_get_str(item, "link", &s);
        if (s[0] != '\0') {
            if (!xrss_buf_put(&buf, "    <link>") ||
                !xrss_buf_put_escaped(&buf, s) ||
                !xrss_buf_put(&buf, "</link>\n")) {
                return "";
            }
        }
        xrss_get_str(item, "description", &s);
        if (s[0] != '\0') {
            if (!xrss_buf_put(&buf, "    <description>") ||
                !xrss_buf_put_escaped(&buf, s) ||
                !xrss_buf_put(&buf, "</description>\n")) {
                return "";
            }
        }
        xrss_get_str(item, "pubDate", &s);
        if (s[0] != '\0') {
            if (!xrss_buf_put(&buf, "    <pubDate>") ||
                !xrss_buf_put_escaped(&buf, s) ||
                !xrss_buf_put(&buf, "</pubDate>\n")) {
                return "";
            }
        }
        xrss_get_str(item, "guid", &s);
        if (s[0] != '\0') {
            if (!xrss_buf_put(&buf, "    <guid>") ||
                !xrss_buf_put_escaped(&buf, s) ||
                !xrss_buf_put(&buf, "</guid>\n")) {
                return "";
            }
        }
        if (!xrss_buf_put(&buf, "  </item>\n")) {
            return "";
        }
    }
    if (!xrss_buf_put(&buf, "</channel>\n</rss>\n")) {
        return "";
    }
    return buf.data ? buf.data : "";
}

const char *xrss_build_atom(xvalue *feed, xvalue *entries) {
    static const char *prologue =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n";
    XrssBuf buf = {NULL, 0, 0};
    const char *s;
    int64 i;
    int64 n;
    if (!xrss_buf_put(&buf, prologue)) {
        return "";
    }
    xrss_get_str(feed, "title", &s);
    if (!xrss_buf_put(&buf, "  <title>") ||
        !xrss_buf_put_escaped(&buf, s) ||
        !xrss_buf_put(&buf, "</title>\n")) {
        return "";
    }
    xrss_get_str(feed, "link", &s);
    if (s[0] != '\0') {
        if (!xrss_buf_put(&buf, "  <link href=\"") ||
            !xrss_buf_put_escaped(&buf, s) ||
            !xrss_buf_put(&buf, "\"/>\n")) {
            return "";
        }
    }
    xrss_get_str(feed, "updated", &s);
    if (s[0] != '\0') {
        if (!xrss_buf_put(&buf, "  <updated>") ||
            !xrss_buf_put_escaped(&buf, s) ||
            !xrss_buf_put(&buf, "</updated>\n")) {
            return "";
        }
    }
    xrss_get_str(feed, "id", &s);
    if (s[0] != '\0') {
        if (!xrss_buf_put(&buf, "  <id>") ||
            !xrss_buf_put_escaped(&buf, s) ||
            !xrss_buf_put(&buf, "</id>\n")) {
            return "";
        }
    }
    n = entries != NULL ? xrtValueCount(entries) : 0;
    for (i = 0; i < n; ++i) {
        xvalue *entry = xrtValueArrayAt(entries, i);
        if (entry == NULL) {
            continue;
        }
        if (!xrss_buf_put(&buf, "  <entry>\n")) {
            return "";
        }
        xrss_get_str(entry, "title", &s);
        if (!xrss_buf_put(&buf, "    <title>") ||
            !xrss_buf_put_escaped(&buf, s) ||
            !xrss_buf_put(&buf, "</title>\n")) {
            return "";
        }
        xrss_get_str(entry, "link", &s);
        if (s[0] != '\0') {
            if (!xrss_buf_put(&buf, "    <link href=\"") ||
                !xrss_buf_put_escaped(&buf, s) ||
                !xrss_buf_put(&buf, "\"/>\n")) {
                return "";
            }
        }
        xrss_get_str(entry, "id", &s);
        if (s[0] != '\0') {
            if (!xrss_buf_put(&buf, "    <id>") ||
                !xrss_buf_put_escaped(&buf, s) ||
                !xrss_buf_put(&buf, "</id>\n")) {
                return "";
            }
        }
        xrss_get_str(entry, "updated", &s);
        if (s[0] != '\0') {
            if (!xrss_buf_put(&buf, "    <updated>") ||
                !xrss_buf_put_escaped(&buf, s) ||
                !xrss_buf_put(&buf, "</updated>\n")) {
                return "";
            }
        }
        xrss_get_str(entry, "summary", &s);
        if (s[0] != '\0') {
            if (!xrss_buf_put(&buf, "    <summary>") ||
                !xrss_buf_put_escaped(&buf, s) ||
                !xrss_buf_put(&buf, "</summary>\n")) {
                return "";
            }
        }
        if (!xrss_buf_put(&buf, "  </entry>\n")) {
            return "";
        }
    }
    if (!xrss_buf_put(&buf, "</feed>\n")) {
        return "";
    }
    return buf.data ? buf.data : "";
}
