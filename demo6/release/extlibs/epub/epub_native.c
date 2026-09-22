/*
 * epub_native.c - Self-contained EPUB 3 writer (stored ZIP).
 *
 * Emits a valid EPUB without external dependencies: the ZIP container
 * uses stored (uncompressed) entries, which the OCF permits, with the
 * mimetype entry first. Content files generated: META-INF/container.xml,
 * OEBPS/content.opf, OEBPS/nav.xhtml, and one XHTML chapter per input.
 * Chapter text is plain UTF-8 with simple block markup: leading "# " or
 * "## " becomes a heading, blank lines split paragraphs, all markup is
 * escaped. readEntry scans local headers so tests can verify content
 * without unzipping.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XEP_DOMAIN "xlang.extlib.epub"

#define XEP_MAX_ENTRIES 128
#define XEP_MAX_CONTENT (8 * 1024 * 1024)

static void xep_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XEP_DOMAIN, 0, message);
}

static xvalue *xep_null(void) {
    return xrtValueNull();
}

static xvalue *xep_text(const char *s, size_t n) {
    str owned = (str)xrtMalloc(n + 1);
    xvalue *value;
    if (owned == NULL) return NULL;
    if (n > 0) memcpy(owned, s, n);
    owned[n] = '\0';
    value = xrtValueStringTake(&owned, n);
    if (value == NULL && owned != NULL) xrtFree(owned);
    return value;
}

static int xep_set_str(xvalue *obj, const char *key, const char *s) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return xrtValueObjectSetNew(obj, view, xep_text(s, strlen(s)));
}

/* ---------------- CRC-32 ---------------- */

static unsigned int xep_crc_table[256];
static int xep_crc_ready = 0;

static void xep_crc_init(void) {
    unsigned int i;
    unsigned int j;
    unsigned int c;
    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        xep_crc_table[i] = c;
    }
    xep_crc_ready = 1;
}

static unsigned int xep_crc32(const unsigned char *data, size_t n) {
    unsigned int crc = 0xFFFFFFFFu;
    size_t i;
    if (!xep_crc_ready) xep_crc_init();
    for (i = 0; i < n; i++) {
        crc = xep_crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

/* ---------------- growable buffer ---------------- */

typedef struct {
    unsigned char *data;
    size_t len;
    size_t cap;
} XepBuf;

static int xep_reserve(XepBuf *b, size_t extra) {
    if (b->len + extra > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 65536;
        unsigned char *grown;
        while (cap < b->len + extra) cap *= 2;
        if (cap > XEP_MAX_CONTENT) return 0;
        grown = (unsigned char *)xrtMalloc(cap);
        if (grown == NULL) return 0;
        if (b->len > 0) memcpy(grown, b->data, b->len);
        xrtFree(b->data);
        b->data = grown;
        b->cap = cap;
    }
    return 1;
}

static int xep_put(XepBuf *b, const void *data, size_t n) {
    if (n == 0) return 1;
    if (!xep_reserve(b, n)) return 0;
    memcpy(b->data + b->len, data, n);
    b->len += n;
    return 1;
}

static int xep_puts(XepBuf *b, const char *s) {
    return xep_put(b, s, strlen(s));
}

static void xep_le16(XepBuf *b, unsigned int v) {
    unsigned char p[2] = {(unsigned char)(v & 0xFF),
                          (unsigned char)((v >> 8) & 0xFF)};
    xep_put(b, p, 2);
}

static void xep_le32(XepBuf *b, unsigned int v) {
    unsigned char p[4] = {(unsigned char)(v & 0xFF),
                          (unsigned char)((v >> 8) & 0xFF),
                          (unsigned char)((v >> 16) & 0xFF),
                          (unsigned char)((v >> 24) & 0xFF)};
    xep_put(b, p, 4);
}

/* ---------------- content generation ---------------- */

static int xep_escape_put(XepBuf *b, const char *s) {
    const char *p;
    for (p = s; *p != '\0'; p++) {
        switch (*p) {
            case '&': if (!xep_puts(b, "&amp;")) return 0; break;
            case '<': if (!xep_puts(b, "&lt;")) return 0; break;
            case '>': if (!xep_puts(b, "&gt;")) return 0; break;
            case '"': if (!xep_puts(b, "&quot;")) return 0; break;
            default:
                if (!xep_put(b, p, 1)) return 0;
        }
    }
    return 1;
}

/* Convert chapter source text into an XHTML body. */
static int xep_chapter_body(XepBuf *out, const char *text) {
    const char *line = text;
    int inParagraph = 0;
    while (line != NULL && *line != '\0') {
        const char *eol = strchr(line, '\n');
        const char *lineEnd = eol ? eol : line + strlen(line);
        size_t len = (size_t)(lineEnd - line);
        char trimmed[1024];
        size_t t = 0;
        size_t i;
        for (i = 0; i < len && t < sizeof(trimmed) - 1; i++) {
            if (line[i] != '\r') trimmed[t++] = line[i];
        }
        trimmed[t] = '\0';
        if (trimmed[0] == '\0') {
            if (inParagraph) {
                if (!xep_puts(out, "</p>\n")) return 0;
                inParagraph = 0;
            }
        } else if (trimmed[0] == '#' && trimmed[1] == '#' &&
                   trimmed[2] == ' ') {
            if (inParagraph) {
                if (!xep_puts(out, "</p>\n")) return 0;
                inParagraph = 0;
            }
            if (!xep_puts(out, "    <h2>") ||
                !xep_escape_put(out, trimmed + 3) ||
                !xep_puts(out, "</h2>\n")) return 0;
        } else if (trimmed[0] == '#' && trimmed[1] == ' ') {
            if (inParagraph) {
                if (!xep_puts(out, "</p>\n")) return 0;
                inParagraph = 0;
            }
            if (!xep_puts(out, "    <h1>") ||
                !xep_escape_put(out, trimmed + 2) ||
                !xep_puts(out, "</h1>\n")) return 0;
        } else {
            if (!inParagraph) {
                if (!xep_puts(out, "    <p>")) return 0;
                inParagraph = 1;
            } else {
                if (!xep_puts(out, " ")) return 0;
            }
            if (!xep_escape_put(out, trimmed)) return 0;
        }
        line = eol ? eol + 1 : NULL;
    }
    if (inParagraph) {
        if (!xep_puts(out, "</p>\n")) return 0;
    }
    return 1;
}

static int xep_chapter_page(XepBuf *page, const char *title,
    const char *text) {
    if (!xep_puts(page,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE html>\n"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
            "xmlns:epub=\"http://www.idpf.org/2007/ops\" "
            "lang=\"zh\" xml:lang=\"zh\">\n"
            "<head><title>") ||
        !xep_escape_put(page, title) ||
        !xep_puts(page, "</title></head>\n<body>\n") ||
        !xep_chapter_body(page, text) ||
        !xep_puts(page, "</body>\n</html>\n")) {
        return 0;
    }
    return 1;
}

/* ---------------- reading inputs ---------------- */

/* strdup replacement avoiding CRT naming clashes */
static char *strdup_(const char *s) {
    size_t n = strlen(s);
    char *copy = (char *)xrtMalloc(n + 1);
    if (copy != NULL) {
        memcpy(copy, s, n + 1);
    }
    return copy;
}

static char *xep_obj_str(xvalue *obj, const char *key, const char *fallback) {
    int64 i;
    int64 n;
    size_t len = strlen(key);
    if (obj != NULL) {
        n = xrtValueCount(obj);
        for (i = 0; i < n; i++) {
            xstrview k;
            xvalue *v = xrtValueObjectAt(obj, i, &k);
            if (v != NULL && k.Size == len &&
                memcmp(k.Data, key, len) == 0) {
                xstrview text;
                if (xrtValueGetString(v, &text)) {
                    char *copy = (char *)xrtMalloc(text.Size + 1);
                    if (copy == NULL) return NULL;
                    memcpy(copy, text.Data, text.Size);
                    copy[text.Size] = '\0';
                    return copy;
                }
            }
        }
    }
    return strdup_(fallback);
}

/* strdup replacement avoiding CRT naming clashes */

typedef struct {
    char name[128];
    unsigned int crc;
    unsigned int size;
    unsigned int offset;
} XepEntry;

static int xep_add_entry(XepBuf *zip, XepEntry *entries, int *count,
    const char *name, const char *content, size_t contentLen) {
    size_t size = contentLen;
    size_t nameLen = strlen(name);
    unsigned int crc;
    XepEntry *e;
    if (*count >= XEP_MAX_ENTRIES || nameLen > 127) return 0;
    crc = xep_crc32((const unsigned char *)content, size);
    e = &entries[*count];
    strcpy(e->name, name);
    e->crc = crc;
    e->size = (unsigned int)size;
    e->offset = (unsigned int)zip->len;
    (*count)++;
    if (!xep_puts(zip, "PK\x03\x04")) return 0;
    xep_le16(zip, 20);      /* version needed */
    xep_le16(zip, 0);       /* flags */
    xep_le16(zip, 0);       /* method: stored */
    xep_le16(zip, 0);       /* time */
    xep_le16(zip, 0x5821);  /* date: 2021-01-01 */
    xep_le32(zip, crc);
    xep_le32(zip, (unsigned int)size);
    xep_le32(zip, (unsigned int)size);
    xep_le16(zip, (unsigned int)nameLen);
    xep_le16(zip, 0);       /* extra len */
    if (!xep_put(zip, name, nameLen)) return 0;
    if (!xep_put(zip, content, size)) return 0;
    return 1;
}

static int xep_finish_zip(XepBuf *zip, const XepEntry *entries, int count) {
    unsigned int cdStart = (unsigned int)zip->len;
    unsigned int cdSize;
    int i;
    for (i = 0; i < count; i++) {
        size_t nameLen = strlen(entries[i].name);
        if (!xep_puts(zip, "PK\x01\x02")) return 0;
        xep_le16(zip, 20);  /* version made by */
        xep_le16(zip, 20);  /* version needed */
        xep_le16(zip, 0);   /* flags */
        xep_le16(zip, 0);   /* method */
        xep_le16(zip, 0);   /* time */
        xep_le16(zip, 0x5821);
        xep_le32(zip, entries[i].crc);
        xep_le32(zip, entries[i].size);
        xep_le32(zip, entries[i].size);
        xep_le16(zip, (unsigned int)nameLen);
        xep_le16(zip, 0);   /* extra */
        xep_le16(zip, 0);   /* comment */
        xep_le16(zip, 0);   /* disk */
        xep_le16(zip, 0);   /* internal attrs */
        xep_le32(zip, 0);   /* external attrs */
        xep_le32(zip, entries[i].offset);
        if (!xep_put(zip, entries[i].name, nameLen)) return 0;
    }
    cdSize = (unsigned int)zip->len - cdStart;
    if (!xep_puts(zip, "PK\x05\x06")) return 0;
    xep_le16(zip, 0);
    xep_le16(zip, 0);
    xep_le16(zip, (unsigned int)count);
    xep_le16(zip, (unsigned int)count);
    xep_le32(zip, cdSize);
    xep_le32(zip, cdStart);
    xep_le16(zip, 0);
    return 1;
}

/* ---------------- public API ---------------- */

long long xep_build(const char *path, xvalue *meta, xvalue *chapters) {
    static const char *mimetype = "application/epub+zip";
    XepBuf zip = {NULL, 0, 0};
    XepEntry entries[XEP_MAX_ENTRIES];
    int count = 0;
    char *title;
    char *author;
    char *language;
    char *identifier;
    int64 chapterCount;
    int64 i;
    XepBuf opf = {NULL, 0, 0};
    XepBuf nav = {NULL, 0, 0};
    long long result = 0;
    title = xep_obj_str(meta, "title", "Untitled");
    author = xep_obj_str(meta, "author", "Unknown");
    language = xep_obj_str(meta, "language", "zh");
    identifier = xep_obj_str(meta, "identifier", "urn:xlang:epub:demo");
    if (title == NULL || author == NULL || language == NULL ||
        identifier == NULL) {
        goto done;
    }
    if (!xep_add_entry(&zip, entries, &count, "mimetype", mimetype, strlen(mimetype))) {
        goto done;
    }
    if (!xep_add_entry(&zip, entries, &count,
            "META-INF/container.xml",
            "<?xml version=\"1.0\"?>\n"
            "<container version=\"1.0\" "
            "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
            "  <rootfiles>\n"
            "    <rootfile full-path=\"OEBPS/content.opf\" "
            "media-type=\"application/oebps-package+xml\"/>\n"
            "  </rootfiles>\n"
            "</container>\n",
            strlen("<?xml version=\"1.0\"?>\n"
            "<container version=\"1.0\" "
            "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
            "  <rootfiles>\n"
            "    <rootfile full-path=\"OEBPS/content.opf\" "
            "media-type=\"application/oebps-package+xml\"/>\n"
            "  </rootfiles>\n"
            "</container>\n"))) {
        goto done;
    }
    chapterCount = chapters != NULL ? xrtValueCount(chapters) : 0;
    if (chapterCount < 1 || chapterCount > 100) {
        xep_error("epub needs 1-100 chapters");
        goto done;
    }
    /* nav document */
    if (!xep_puts(&nav,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE html>\n"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
            "xmlns:epub=\"http://www.idpf.org/2007/ops\" "
            "lang=\"zh\" xml:lang=\"zh\">\n"
            "<head><title>目录</title></head>\n"
            "<body>\n"
            "  <nav epub:type=\"toc\" id=\"toc\">\n"
            "    <h1>目录</h1>\n"
            "    <ol>\n")) {
        goto done;
    }
    /* chapters */
    for (i = 0; i < chapterCount; i++) {
        xvalue *chapter = xrtValueArrayAt(chapters, i);
        char *chTitle = xep_obj_str(chapter, "title", "Chapter");
        char *chText = xep_obj_str(chapter, "text", "");
        char fileName[64];
        char href[80];
        XepBuf page = {NULL, 0, 0};
        if (chTitle == NULL || chText == NULL) {
            xrtFree(chTitle);
            xrtFree(chText);
            goto done;
        }
        snprintf(fileName, sizeof(fileName), "OEBPS/ch%lld.xhtml",
            (long long)(i + 1));
        snprintf(href, sizeof(href), "ch%lld.xhtml", (long long)(i + 1));
        if (!xep_chapter_page(&page, chTitle, chText) ||
            !xep_add_entry(&zip, entries, &count, fileName,
                (const char *)(page.data ? (const char *)page.data : ""), page.len)) {
            xrtFree(page.data);
            xrtFree(chTitle);
            xrtFree(chText);
            goto done;
        }
        xrtFree(page.data);
        if (!xep_puts(&nav, "      <li><a href=\"") ||
            !xep_puts(&nav, href) ||
            !xep_puts(&nav, "\">") ||
            !xep_escape_put(&nav, chTitle) ||
            !xep_puts(&nav, "</a></li>\n")) {
            xrtFree(chTitle);
            xrtFree(chText);
            goto done;
        }
        xrtFree(chTitle);
        xrtFree(chText);
    }
    if (!xep_puts(&nav,
            "    </ol>\n"
            "  </nav>\n"
            "</body>\n"
            "</html>\n") ||
        !xep_add_entry(&zip, entries, &count, "OEBPS/nav.xhtml",
            (const char *)(nav.data ? (const char *)nav.data : ""), nav.len)) {
        goto done;
    }
    /* package document */
    if (!xep_puts(&opf,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" "
            "unique-identifier=\"bookid\">\n"
            "  <metadata "
            "xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
            "    <dc:identifier id=\"bookid\">") ||
        !xep_escape_put(&opf, identifier) ||
        !xep_puts(&opf, "</dc:identifier>\n    <dc:title>") ||
        !xep_escape_put(&opf, title) ||
        !xep_puts(&opf, "</dc:title>\n    <dc:creator>") ||
        !xep_escape_put(&opf, author) ||
        !xep_puts(&opf, "</dc:creator>\n    <dc:language>") ||
        !xep_escape_put(&opf, language) ||
        !xep_puts(&opf, "</dc:language>\n  </metadata>\n  <manifest>\n"
            "    <item id=\"nav\" href=\"nav.xhtml\" "
            "media-type=\"application/xhtml+xml\" "
            "properties=\"nav\"/>\n")) {
        goto done;
    }
    for (i = 0; i < chapterCount; i++) {
        char item[128];
        snprintf(item, sizeof(item),
            "    <item id=\"ch%lld\" href=\"ch%lld.xhtml\" "
            "media-type=\"application/xhtml+xml\"/>\n",
            (long long)(i + 1), (long long)(i + 1));
        if (!xep_puts(&opf, item)) goto done;
    }
    if (!xep_puts(&opf, "  </manifest>\n  <spine>\n")) goto done;
    for (i = 0; i < chapterCount; i++) {
        char item[64];
        snprintf(item, sizeof(item), "    <itemref idref=\"ch%lld\"/>\n",
            (long long)(i + 1));
        if (!xep_puts(&opf, item)) goto done;
    }
    if (!xep_puts(&opf, "  </spine>\n</package>\n") ||
        !xep_add_entry(&zip, entries, &count, "OEBPS/content.opf",
            (const char *)(opf.data ? (const char *)opf.data : ""), opf.len)) {
        goto done;
    }
    if (!xep_finish_zip(&zip, entries, count)) goto done;
    {
        xbytesview view;
        view.Data = zip.data;
        view.Size = zip.len;
        if (!xrtFileWriteAll(path, view)) {
            xep_error("epub file cannot be written");
            goto done;
        }
    }
    result = (long long)zip.len;
done:
    xrtFree(zip.data);
    xrtFree(opf.data);
    xrtFree(nav.data);
    xrtFree(title);
    xrtFree(author);
    xrtFree(language);
    xrtFree(identifier);
    return result;
}

const char *xep_read_entry(const char *path, const char *name) {
    static char *slot = NULL;
    size_t size = 0;
    bytes data = xrtFileReadAll(path, &size);
    const unsigned char *p;
    const unsigned char *end;
    if (data == NULL) {
        xep_error("epub file cannot be read");
        return "";
    }
    xrtFree(slot);
    slot = NULL;
    p = data;
    end = data + size;
    while (p + 4 <= end && memcmp(p, "PK\x03\x04", 4) == 0) {
        unsigned int nameLen = p[26] | (p[27] << 8);
        unsigned int extraLen = p[28] | (p[29] << 8);
        unsigned int compSize = p[18] | (p[19] << 8) |
                                 (p[20] << 16) | (p[21] << 24);
        if (p + 30 + nameLen > end) break;
        if (nameLen == strlen(name) &&
            memcmp(p + 30, name, nameLen) == 0) {
            const unsigned char *content = p + 30 + nameLen + extraLen;
            if (content + compSize > end) break;
            slot = (char *)xrtMalloc(compSize + 1);
            if (slot == NULL) break;
            memcpy(slot, content, compSize);
            slot[compSize] = '\0';
            xrtFree(data);
            return slot;
        }
        p = p + 30 + nameLen + extraLen + compSize;
    }
    xrtFree(data);
    return "";
}
