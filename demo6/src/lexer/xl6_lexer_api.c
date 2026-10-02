#include "src/lexer/xl6_lexer_api.h"
#include "src/lexer/xl6_lexer_gen.h"
#include "src/common/xl6_console.h"

#include <stdio.h>
#include <limits.h>

static const char* xl6SkipUtf8Bom(const char* sourceText)
{
    const unsigned char* bytes;

    if ( sourceText == NULL ) {
        return NULL;
    }

    bytes = (const unsigned char*)sourceText;
    if ( bytes[0] == 0xEFu && bytes[1] == 0xBBu && bytes[2] == 0xBFu ) {
        return sourceText + 3;
    }
    return sourceText;
}

int xl6LexerOpen(XL6_Lexer* lexer, const char* sourceText)
{
    const char* scanText;

    if ( (lexer == NULL) || (sourceText == NULL) ) {
        return 0;
    }
    scanText = xl6SkipUtf8Bom(sourceText);
    memset(lexer, 0, sizeof(*lexer));
    lexer->extra.line = 1;
    lexer->extra.column = 1;
    lexer->extra.sourceText = scanText;
    lexer->extra.sourceSize = strlen(scanText);
    /* Flex's scan_bytes uses int and reserves two end-of-buffer sentinels. */
    if ( lexer->extra.sourceSize > (size_t)INT_MAX - 2 ) return 0;

    if ( xl6lex_init_extra(&lexer->extra, (yyscan_t*)&lexer->scanner) != 0 ) {
        return 0;
    }
    lexer->buffer = xl6_scan_string(scanText, (yyscan_t)lexer->scanner);
    return lexer->buffer != NULL;
}

void xl6LexerClose(XL6_Lexer* lexer)
{
    if ( (lexer == NULL) || (lexer->scanner == NULL) ) {
        return;
    }
    if ( lexer->buffer != NULL ) {
        xl6_delete_buffer((YY_BUFFER_STATE)lexer->buffer, (yyscan_t)lexer->scanner);
    }
    xl6lex_destroy((yyscan_t)lexer->scanner);
    memset(lexer, 0, sizeof(*lexer));
}

XL6_Token xl6LexerNext(XL6_Lexer* lexer)
{
    XL6_Token token;

    memset(&token, 0, sizeof(token));
    if ( (lexer == NULL) || (lexer->scanner == NULL) ) {
        token.kind = XL6_TK_EOF;
        return token;
    }

    (void)xl6lex((yyscan_t)lexer->scanner);
    return lexer->extra.current;
}

static void xl6PrintTokenText(XL6_Output* out, const XL6_Token* token)
{
    xl6OutputPrintf(out, "%4d:%-3d %-16s ", token->startLine, token->startColumn, xl6TokenKindName(token->kind));
    if ( token->text != NULL && token->len > 0 ) {
        xl6OutputPrintf(out, "%.*s", (int)token->len, token->text);
    }
    if ( token->message[0] != '\0' ) {
        xl6OutputPrintf(out, "  # %s", token->message);
    }
    xl6OutputPutc('\n', out);
}

static void xl6PrintEscapedJsonText(XL6_Output* out, const char* text, size_t len)
{
    size_t i;

    for ( i = 0; i < len; ++i ) {
        unsigned char ch = (unsigned char)text[i];
        if ( ch == '"' || ch == '\\' ) {
            xl6OutputPutc('\\', out);
            xl6OutputPutc(ch, out);
        } else if ( ch == '\n' ) {
            xl6OutputPuts("\\n", out);
        } else if ( ch == '\r' ) {
            xl6OutputPuts("\\r", out);
        } else if ( ch == '\t' ) {
            xl6OutputPuts("\\t", out);
        } else {
            xl6OutputPutc(ch, out);
        }
    }
}

static void xl6PrintTokenJson(XL6_Output* out, const XL6_Token* token, int comma)
{
    xl6OutputPrintf(out, "  {\"kind\":\"%s\",\"line\":%d,\"column\":%d,\"text\":\"",
            xl6TokenKindName(token->kind), token->startLine, token->startColumn);
    if ( token->text != NULL && token->len > 0 ) {
        xl6PrintEscapedJsonText(out, token->text, token->len);
    }
    xl6OutputPuts("\"}", out);
    if ( comma ) {
        xl6OutputPutc(',', out);
    }
}

int xl6LexerDumpFileTo(XL6_Output* out, const char* path, int jsonOutput)
{
    char* source;
    size_t sourceLen;
    XL6_Lexer lexer;
    XL6_Token token;
    int ok;
    int first;

    if ( out == NULL ) {
        return 1;
    }
    if ( path == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot read file: <null>\n");
        return 1;
    }

    source = xrtFileReadText(path, XENCODING_UTF8, XUTF_STRICT, &sourceLen);
    (void)sourceLen;
    if ( source == NULL ) {
        xrtFree(source);
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot read file: %s\n", path);
        return 1;
    }

    if ( !xl6LexerOpen(&lexer, source) ) {
        xrtFree(source);
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot open lexer\n");
        return 1;
    }

    ok = 1;
    first = 1;
    if ( jsonOutput ) {
        xl6OutputPuts("[\n", out);
    }

    for (;;) {
        token = xl6LexerNext(&lexer);
        if ( jsonOutput ) {
            if ( !first ) {
                xl6OutputPuts(",\n", out);
            }
            xl6PrintTokenJson(out, &token, 0);
            first = 0;
        } else {
            xl6PrintTokenText(out, &token);
        }
        if ( token.kind == XL6_TK_ERROR ) {
            ok = 0;
        }
        if ( token.kind == XL6_TK_EOF ) {
            break;
        }
    }

    if ( jsonOutput ) {
        xl6OutputPuts("\n]\n", out);
    }

    xl6LexerClose(&lexer);
    xrtFree(source);
    return ok ? 0 : 1;
}

int xl6LexerDumpFile(const char* path, int jsonOutput)
{
    XL6_Output out;

    xl6OutputInitStdFile(&out, stdout);
    return xl6LexerDumpFileTo(&out, path, jsonOutput);
}
