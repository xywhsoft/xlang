#include "src/ast/xl6_ast_dump.h"

static void xl6DumpIndent(FILE* out, int indent)
{
    int i;

    for ( i = 0; i < indent; ++i ) {
        fputs("  ", out);
    }
}

void xl6AstDumpTextFile(FILE* out, const XL6_AstNode* node, int indent)
{
    size_t i;

    if ( out == NULL ) {
        return;
    }
    if ( node == NULL ) {
        return;
    }
    xl6DumpIndent(out, indent);
    fputs(xl6AstKindName(node->kind), out);
    if ( node->text != NULL && node->text[0] != '\0' ) {
        fprintf(out, " \"%s\"", node->text);
    }
    if ( node->line > 0 ) {
        fprintf(out, " @%d:%d", node->line, node->column);
    }
    fputc('\n', out);

    for ( i = 0; i < node->childCount; ++i ) {
        xl6AstDumpTextFile(out, node->children[i], indent + 1);
    }
}

void xl6AstDumpText(const XL6_AstNode* node, int indent)
{
    xl6AstDumpTextFile(stdout, node, indent);
}

static void xl6AstDumpJsonEscaped(FILE* out, const char* text)
{
    const unsigned char* p;

    if ( text == NULL ) {
        return;
    }
    for ( p = (const unsigned char*)text; *p != '\0'; ++p ) {
        unsigned char ch = *p;
        if ( ch == '"' ) {
            fputs("\\\"", out);
        } else if ( ch == '\\' ) {
            fputs("\\\\", out);
        } else if ( ch == '\b' ) {
            fputs("\\b", out);
        } else if ( ch == '\f' ) {
            fputs("\\f", out);
        } else if ( ch == '\n' ) {
            fputs("\\n", out);
        } else if ( ch == '\r' ) {
            fputs("\\r", out);
        } else if ( ch == '\t' ) {
            fputs("\\t", out);
        } else if ( ch < 0x20 ) {
            fprintf(out, "\\u%04X", (unsigned)ch);
        } else {
            fputc((int)ch, out);
        }
    }
}

static void xl6AstDumpJsonIndent(FILE* out, int indent)
{
    int i;

    for ( i = 0; i < indent; ++i ) {
        fputs("  ", out);
    }
}

static void xl6AstDumpJsonNode(FILE* out, const XL6_AstNode* node, int indent)
{
    size_t i;

    if ( node == NULL ) {
        fputs("null", out);
        return;
    }

    fputs("{\n", out);
    xl6AstDumpJsonIndent(out, indent + 1);
    fprintf(out, "\"kind\":\"%s\"", xl6AstKindName(node->kind));
    if ( node->text != NULL && node->text[0] != '\0' ) {
        fputs(",\n", out);
        xl6AstDumpJsonIndent(out, indent + 1);
        fputs("\"text\":\"", out);
        xl6AstDumpJsonEscaped(out, node->text);
        fputc('"', out);
    }
    if ( node->line > 0 ) {
        fputs(",\n", out);
        xl6AstDumpJsonIndent(out, indent + 1);
        fprintf(out, "\"line\":%d,\n", node->line);
        xl6AstDumpJsonIndent(out, indent + 1);
        fprintf(out, "\"column\":%d", node->column);
    }
    fputs(",\n", out);
    xl6AstDumpJsonIndent(out, indent + 1);
    fputs("\"children\":[", out);
    if ( node->childCount > 0 ) {
        fputc('\n', out);
        for ( i = 0; i < node->childCount; ++i ) {
            xl6AstDumpJsonIndent(out, indent + 2);
            xl6AstDumpJsonNode(out, node->children[i], indent + 2);
            if ( i + 1 < node->childCount ) {
                fputc(',', out);
            }
            fputc('\n', out);
        }
        xl6AstDumpJsonIndent(out, indent + 1);
    }
    fputc(']', out);
    fputc('\n', out);
    xl6AstDumpJsonIndent(out, indent);
    fputc('}', out);
}

void xl6AstDumpJson(FILE* out, const XL6_AstNode* node)
{
    if ( out == NULL ) {
        return;
    }
    xl6AstDumpJsonNode(out, node, 0);
    fputc('\n', out);
}
