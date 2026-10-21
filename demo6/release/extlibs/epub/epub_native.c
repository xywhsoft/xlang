/* 将 UTF-8 纯文本章节写成 EPUB 3；ZIP 封装集中在 epub_zip.c。 */
#include "epub_internal.h"
#include "include/epub_native.h"

#include <stdio.h>
#include <string.h>

static void xep_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.epub", 0, message);
}

/* EPUB 内嵌 XML 1.0，合法 UTF-8 还必须满足 XML Char 范围。 */
static int xep_xml_text_valid(xstrview text) {
    size_t pos = 0;
    while (pos < text.Size) {
        uint32 scalar;
        size_t read;
        if (xrtUtf8Decode((xstrview){text.Data + pos, text.Size - pos},
                &scalar, &read) != XUTF_OK || read == 0 ||
            (scalar < 0x20 && scalar != '\t' && scalar != '\n' && scalar != '\r') ||
            scalar == 0xfffe || scalar == 0xffff) return 0;
        pos += read;
    }
    return 1;
}

static int xep_field(const xvalue *object, const char *key, int required,
    xstrview *out) {
    xvalue *value = xrtValueObjectGet(object, (xstrview){key, strlen(key)});
    if (value == NULL) {
        *out = (xstrview){"", 0};
        return !required;
    }
    if (!xrtValueGetString(value, out) || (required && out->Size == 0) ||
        !xep_xml_text_valid(*out)) return 0;
    return 1;
}

static int xep_xml(xbuffer *out, xstrview text) {
    size_t i;
    for (i = 0; i < text.Size; ++i) {
        const char *escape = NULL;
        switch (text.Data[i]) {
            case '&': escape = "&amp;"; break;
            case '<': escape = "&lt;"; break;
            case '>': escape = "&gt;"; break;
            case '"': escape = "&quot;"; break;
            case '\'': escape = "&apos;"; break;
            /* 属性和文本均保留原始空白，避免 XML 解析时归一化。 */
            case '\t': escape = "&#x9;"; break;
            case '\n': escape = "&#xA;"; break;
            case '\r': escape = "&#xD;"; break;
            default: break;
        }
        if (escape != NULL) {
            if (!xep_literal(out, escape)) return 0;
        } else if (!xep_append(out, text.Data + i, 1)) return 0;
    }
    return 1;
}

/* 行长度不设固定数组；只把 # / ## 行识别为标题，其余作为纯文本。 */
static int xep_body(xbuffer *out, xstrview text) {
    size_t pos = 0;
    int paragraph = 0;
    while (pos < text.Size) {
        size_t end = pos;
        xstrview line;
        int level = 0;
        while (end < text.Size && text.Data[end] != '\n') ++end;
        line = (xstrview){text.Data + pos, end - pos};
        if (line.Size != 0 && line.Data[line.Size - 1] == '\r') --line.Size;
        if (memchr(line.Data, '\r', line.Size) != NULL) return 0;
        if (line.Size > 2 && line.Data[0] == '#' && line.Data[1] == ' ')
            level = 1;
        else if (line.Size > 3 && line.Data[0] == '#' &&
            line.Data[1] == '#' && line.Data[2] == ' ') level = 2;
        if (line.Size == 0 || level != 0) {
            if (paragraph && !xep_literal(out, "</p>\n")) return 0;
            paragraph = 0;
            if (level != 0) {
                line.Data += level + 1;
                line.Size -= (size_t)level + 1;
                if (!xep_literal(out, level == 1 ? "<h1>" : "<h2>") ||
                    !xep_xml(out, line) ||
                    !xep_literal(out, level == 1 ? "</h1>\n" : "</h2>\n")) return 0;
            }
        } else {
            if (!paragraph) {
                if (!xep_literal(out, "<p>")) return 0;
                paragraph = 1;
            } else if (!xep_literal(out, " ")) return 0;
            if (!xep_xml(out, line)) return 0;
        }
        pos = end < text.Size ? end + 1 : end;
    }
    return !paragraph || xep_literal(out, "</p>\n");
}

static int xep_page(xbuffer *out, xstrview title, xstrview language,
    xstrview text) {
    return xep_literal(out,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE html>\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"") &&
        xep_xml(out, language) && xep_literal(out, "\" lang=\"") &&
        xep_xml(out, language) && xep_literal(out, "\"><head><title>") &&
        xep_xml(out, title) && xep_literal(out, "</title></head><body>\n") &&
        xep_body(out, text) && xep_literal(out, "</body></html>\n");
}

static int xep_nav_start(xbuffer *out, xstrview language) {
    return xep_literal(out,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE html>\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
        "xmlns:epub=\"http://www.idpf.org/2007/ops\" xml:lang=\"") &&
        xep_xml(out, language) &&
        xep_literal(out,
            "\"><head><title>Contents</title></head><body>"
            "<nav epub:type=\"toc\" id=\"toc\"><h1>Contents</h1><ol>\n");
}

static int xep_opf_start(xbuffer *out, xstrview title, xstrview author,
    xstrview language, xstrview identifier, xstrview modified) {
    if (!xep_literal(out,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" "
        "unique-identifier=\"bookid\"><metadata "
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
        "<dc:identifier id=\"bookid\">") ||
        !xep_xml(out, identifier) ||
        !xep_literal(out, "</dc:identifier><dc:title>") ||
        !xep_xml(out, title) ||
        !xep_literal(out, "</dc:title><dc:language>") ||
        !xep_xml(out, language) || !xep_literal(out, "</dc:language>")) return 0;
    if (author.Size != 0 && (!xep_literal(out, "<dc:creator>") ||
        !xep_xml(out, author) || !xep_literal(out, "</dc:creator>"))) return 0;
    return xep_literal(out, "<meta property=\"dcterms:modified\">") &&
        xep_xml(out, modified) &&
        xep_literal(out,
            "</meta></metadata><manifest>"
            "<item id=\"nav\" href=\"nav.xhtml\" "
            "media-type=\"application/xhtml+xml\" properties=\"nav\"/>\n");
}

static int xep_modified_valid(xstrview text) {
    static const char shape[] = "0000-00-00T00:00:00Z";
    xtime parsed;
    size_t i;
    if (text.Size != sizeof(shape) - 1) return 0;
    for (i = 0; i < text.Size; ++i) {
        if (shape[i] == '0') {
            if (text.Data[i] < '0' || text.Data[i] > '9') return 0;
        } else if (shape[i] != text.Data[i]) return 0;
    }
    return xrtTimeParse(text, XRT_STR_LITERAL("%Y-%m-%dT%H:%M:%SZ"), &parsed);
}

int64_t xep_build(const char *path, xvalue *meta, xvalue *chapters) {
    static const char container[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<container version=\"1.0\" "
        "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
        "<rootfiles><rootfile full-path=\"OEBPS/content.opf\" "
        "media-type=\"application/oebps-package+xml\"/>"
        "</rootfiles></container>";
    XepArchive archive = {0};
    xbuffer opf = {0}, nav = {0};
    xstrview title, author, language, identifier, modified;
    char now[32];
    int64_t count, i;
    int64_t result = 0;
    xrtClearError();
    if (path == NULL || path[0] == 0 || meta == NULL ||
        xrtValueType(meta) != XVALUE_OBJECT || chapters == NULL ||
        xrtValueType(chapters) != XVALUE_ARRAY ||
        (count = xrtValueCount(chapters)) < 1 || count > 100 ||
        !xep_field(meta, "title", 1, &title) ||
        !xep_field(meta, "identifier", 1, &identifier) ||
        !xep_field(meta, "language", 1, &language) ||
        !xep_field(meta, "author", 0, &author) ||
        !xep_field(meta, "modified", 0, &modified)) goto invalid;
    if (modified.Size == 0) {
        xtime current = xrtNow();
        size_t length;
        if (xrtGetError() != NULL) goto invalid;
        length = xrtTimeWrite(now, sizeof(now), current, 0,
            XRT_STR_LITERAL("%Y-%m-%dT%H:%M:%SZ"));
        if (length == 0 || length >= sizeof(now)) goto invalid;
        modified = (xstrview){now, length};
    }
    if (!xep_modified_valid(modified) ||
        !xrtBufferInit(&archive.data) || !xrtBufferInit(&opf) ||
        !xrtBufferInit(&nav)) goto invalid;
    if (!xep_archive_add(&archive, "mimetype", "application/epub+zip", 20) ||
        !xep_archive_add(&archive, "META-INF/container.xml",
            container, sizeof(container) - 1) ||
        !xep_nav_start(&nav, language) ||
        !xep_opf_start(&opf, title, author, language, identifier, modified))
        goto invalid;
    for (i = 0; i < count; ++i) {
        xvalue *chapter = xrtValueArrayAt(chapters, i);
        xstrview chapter_title, chapter_text;
        xbuffer page = {0};
        char name[64], href[32], item[160];
        if (chapter == NULL || xrtValueType(chapter) != XVALUE_OBJECT ||
            !xep_field(chapter, "title", 1, &chapter_title) ||
            !xep_field(chapter, "text", 0, &chapter_text) ||
            !xrtBufferInit(&page)) goto invalid;
        snprintf(name, sizeof(name), "OEBPS/ch%lld.xhtml", (long long)i + 1);
        snprintf(href, sizeof(href), "ch%lld.xhtml", (long long)i + 1);
        if (!xep_page(&page, chapter_title, language, chapter_text) ||
            !xep_archive_add(&archive, name, page.Data, page.Size)) {
            xrtBufferUnit(&page);
            goto invalid;
        }
        xrtBufferUnit(&page);
        if (!xep_literal(&nav, "<li><a href=\"") ||
            !xep_literal(&nav, href) || !xep_literal(&nav, "\">") ||
            !xep_xml(&nav, chapter_title) ||
            !xep_literal(&nav, "</a></li>\n")) goto invalid;
        snprintf(item, sizeof(item),
            "<item id=\"ch%lld\" href=\"ch%lld.xhtml\" "
            "media-type=\"application/xhtml+xml\"/>\n",
            (long long)i + 1, (long long)i + 1);
        if (!xep_literal(&opf, item)) goto invalid;
    }
    if (!xep_literal(&nav, "</ol></nav></body></html>\n") ||
        !xep_archive_add(&archive, "OEBPS/nav.xhtml", nav.Data, nav.Size) ||
        !xep_literal(&opf, "</manifest><spine>\n")) goto invalid;
    for (i = 0; i < count; ++i) {
        char item[64];
        snprintf(item, sizeof(item), "<itemref idref=\"ch%lld\"/>\n",
            (long long)i + 1);
        if (!xep_literal(&opf, item)) goto invalid;
    }
    if (!xep_literal(&opf, "</spine></package>\n") ||
        !xep_archive_add(&archive, "OEBPS/content.opf", opf.Data, opf.Size) ||
        !xep_archive_finish(&archive)) goto invalid;
    if (!xrtFileWriteAtomic(path,
        (xbytesview){archive.data.Data, archive.data.Size})) goto done;
    result = (int64_t)archive.data.Size;
    goto done;
invalid:
    if (xrtGetError() == NULL) xep_error("invalid EPUB metadata, chapter or 8 MiB limit");
done:
    xrtBufferUnit(&archive.data);
    xrtBufferUnit(&opf);
    xrtBufferUnit(&nav);
    return result;
}
