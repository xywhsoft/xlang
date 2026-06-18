#include "src/lexer/xl6_lexer_api.h"
#include "src/lexer/xl6_lexer_gen.h"

int xl6LexerOpen(XL6_Lexer* lexer, const char* sourceText)
{
    if ( (lexer == NULL) || (sourceText == NULL) ) {
        return 0;
    }
    memset(lexer, 0, sizeof(*lexer));
    lexer->extra.line = 1;
    lexer->extra.column = 1;

    if ( xl6lex_init_extra(&lexer->extra, (yyscan_t*)&lexer->scanner) != 0 ) {
        return 0;
    }
    lexer->buffer = xl6_scan_string(sourceText, (yyscan_t)lexer->scanner);
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

static void xl6PrintTokenText(FILE* out, const XL6_Token* token)
{
    fprintf(out, "%4d:%-3d %-16s ", token->startLine, token->startColumn, xl6TokenKindName(token->kind));
    if ( token->text != NULL && token->len > 0 ) {
        fprintf(out, "%.*s", (int)token->len, token->text);
    }
    if ( token->message[0] != '\0' ) {
        fprintf(out, "  # %s", token->message);
    }
    fputc('\n', out);
}

static void xl6PrintEscapedJsonText(FILE* out, const char* text, size_t len)
{
    size_t i;

    for ( i = 0; i < len; ++i ) {
        unsigned char ch = (unsigned char)text[i];
        if ( ch == '"' || ch == '\\' ) {
            fputc('\\', out);
            fputc(ch, out);
        } else if ( ch == '\n' ) {
            fputs("\\n", out);
        } else if ( ch == '\r' ) {
            fputs("\\r", out);
        } else if ( ch == '\t' ) {
            fputs("\\t", out);
        } else {
            fputc(ch, out);
        }
    }
}

static void xl6PrintTokenJson(FILE* out, const XL6_Token* token, int comma)
{
    fprintf(out, "  {\"kind\":\"%s\",\"line\":%d,\"column\":%d,\"text\":\"",
            xl6TokenKindName(token->kind), token->startLine, token->startColumn);
    if ( token->text != NULL && token->len > 0 ) {
        xl6PrintEscapedJsonText(out, token->text, token->len);
    }
    fputs("\"}", out);
    if ( comma ) {
        fputc(',', out);
    }
}

int xl6LexerDumpFileTo(FILE* out, const char* path, int jsonOutput)
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
    source = xl6ReadFileText(path, &sourceLen);
    (void)sourceLen;
    if ( source == NULL ) {
        fprintf(stderr, "cannot read file: %s\n", path);
        return 1;
    }

    if ( !xl6LexerOpen(&lexer, source) ) {
        xl6Free(source);
        fprintf(stderr, "cannot open lexer\n");
        return 1;
    }

    ok = 1;
    first = 1;
    if ( jsonOutput ) {
        fputs("[\n", out);
    }

    for (;;) {
        token = xl6LexerNext(&lexer);
        if ( jsonOutput ) {
            if ( !first ) {
                fputs(",\n", out);
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
        fputs("\n]\n", out);
    }

    xl6LexerClose(&lexer);
    xl6Free(source);
    return ok ? 0 : 1;
}

int xl6LexerDumpFile(const char* path, int jsonOutput)
{
    return xl6LexerDumpFileTo(stdout, path, jsonOutput);
}
