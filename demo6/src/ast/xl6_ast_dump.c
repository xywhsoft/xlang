#include "src/ast/xl6_ast_dump.h"
#include "src/common/xl6_output.h"

static void xl6DumpIndent(XL6_Output* out, int indent)
{
    int i;

    for ( i = 0; i < indent; ++i ) {
        xl6OutputPuts("  ", out);
    }
}

void xl6AstDumpTextFile(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    size_t i;

    if ( out == NULL ) {
        return;
    }
    if ( node == NULL ) {
        return;
    }
    xl6DumpIndent(out, indent);
    xl6OutputPuts(xl6AstKindName(node->kind), out);
    if ( node->text != NULL && node->text[0] != '\0' ) {
        xl6OutputPrintf(out, " \"%s\"", node->text);
    }
    if ( node->line > 0 ) {
        xl6OutputPrintf(out, " @%d:%d", node->line, node->column);
    }
    if ( node->doc != NULL && node->doc[0] != '\0' ) {
        xl6OutputPuts(" #doc", out);
    }
    xl6OutputPutc('\n', out);

    for ( i = 0; i < node->childCount; ++i ) {
        xl6AstDumpTextFile(out, node->children[i], indent + 1);
    }
}

void xl6AstDumpText(const XL6_AstNode* node, int indent)
{
    XL6_Output out;

    xl6OutputInitStdFile(&out, stdout);
    xl6AstDumpTextFile(&out, node, indent);
    xl6OutputClose(&out);
}

static void xl6AstDumpJsonEscaped(XL6_Output* out, const char* text)
{
    const unsigned char* p;

    if ( text == NULL ) {
        return;
    }
    for ( p = (const unsigned char*)text; *p != '\0'; ++p ) {
        unsigned char ch = *p;
        if ( ch == '"' ) {
            xl6OutputPuts("\\\"", out);
        } else if ( ch == '\\' ) {
            xl6OutputPuts("\\\\", out);
        } else if ( ch == '\b' ) {
            xl6OutputPuts("\\b", out);
        } else if ( ch == '\f' ) {
            xl6OutputPuts("\\f", out);
        } else if ( ch == '\n' ) {
            xl6OutputPuts("\\n", out);
        } else if ( ch == '\r' ) {
            xl6OutputPuts("\\r", out);
        } else if ( ch == '\t' ) {
            xl6OutputPuts("\\t", out);
        } else if ( ch < 0x20 ) {
            xl6OutputPrintf(out, "\\u%04X", (unsigned)ch);
        } else {
            xl6OutputPutc((int)ch, out);
        }
    }
}

static void xl6AstDumpJsonIndent(XL6_Output* out, int indent)
{
    int i;

    for ( i = 0; i < indent; ++i ) {
        xl6OutputPuts("  ", out);
    }
}

static void xl6AstDumpJsonNode(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    size_t i;

    if ( node == NULL ) {
        xl6OutputPuts("null", out);
        return;
    }

    xl6OutputPuts("{\n", out);
    xl6AstDumpJsonIndent(out, indent + 1);
    xl6OutputPrintf(out, "\"kind\":\"%s\"", xl6AstKindName(node->kind));
    if ( node->text != NULL && node->text[0] != '\0' ) {
        xl6OutputPuts(",\n", out);
        xl6AstDumpJsonIndent(out, indent + 1);
        xl6OutputPuts("\"text\":\"", out);
        xl6AstDumpJsonEscaped(out, node->text);
        xl6OutputPutc('"', out);
    }
    if ( node->line > 0 ) {
        xl6OutputPuts(",\n", out);
        xl6AstDumpJsonIndent(out, indent + 1);
        xl6OutputPrintf(out, "\"line\":%d,\n", node->line);
        xl6AstDumpJsonIndent(out, indent + 1);
        xl6OutputPrintf(out, "\"column\":%d", node->column);
    }
    if ( node->doc != NULL && node->doc[0] != '\0' ) {
        xl6OutputPuts(",\n", out);
        xl6AstDumpJsonIndent(out, indent + 1);
        xl6OutputPuts("\"doc\":\"", out);
        xl6AstDumpJsonEscaped(out, node->doc);
        xl6OutputPutc('"', out);
    }
    xl6OutputPuts(",\n", out);
    xl6AstDumpJsonIndent(out, indent + 1);
    xl6OutputPuts("\"children\":[", out);
    if ( node->childCount > 0 ) {
        xl6OutputPutc('\n', out);
        for ( i = 0; i < node->childCount; ++i ) {
            xl6AstDumpJsonIndent(out, indent + 2);
            xl6AstDumpJsonNode(out, node->children[i], indent + 2);
            if ( i + 1 < node->childCount ) {
                xl6OutputPutc(',', out);
            }
            xl6OutputPutc('\n', out);
        }
        xl6AstDumpJsonIndent(out, indent + 1);
    }
    xl6OutputPutc(']', out);
    xl6OutputPutc('\n', out);
    xl6AstDumpJsonIndent(out, indent);
    xl6OutputPutc('}', out);
}

void xl6AstDumpJson(XL6_Output* out, const XL6_AstNode* node)
{
    if ( out == NULL ) {
        return;
    }
    xl6AstDumpJsonNode(out, node, 0);
    xl6OutputPutc('\n', out);
}
