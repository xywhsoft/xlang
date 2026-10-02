#include "src/lexer/xl6_literal_span.h"
#include "src/lexer/xl6_lexer_api.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static void truncated(const char* source)
{
    size_t len = strlen(source), cut;
    for ( cut = 0; cut <= len; ++cut ) {
        /* The scanner receives exactly cut accessible bytes, not a C string. */
        char* exact = (char*)malloc(cut == 0 ? 1 : cut);
        XL6_LiteralSpan span;
        assert(exact != NULL);
        memcpy(exact, source, cut);
        span = xl6ScanLiteralSpan(exact, cut, 0, 0);
        assert(span.end <= cut);
        if ( cut == len ) assert(span.error == NULL && span.end == len);
        free(exact);
        ++checks;
    }
}

static void token_positions(void)
{
    const char* source = "f\"{\"a\"}\"\r\nf\"{f\"{\"\xE4\xBD\xA0\"}\"}\" tail";
    XL6_Lexer lexer;
    XL6_Token token;
    assert(xl6LexerOpen(&lexer, source));
    token = xl6LexerNext(&lexer);
    assert(token.kind == XL6_TK_FSTRING && token.len == 8);
    assert(token.text == source && token.startLine == 1 && token.endColumn == 9);
    token = xl6LexerNext(&lexer);
    assert(token.kind == XL6_TK_SEP && token.startLine == 1 && token.endLine == 2);
    token = xl6LexerNext(&lexer);
    assert(token.kind == XL6_TK_FSTRING && token.text == source + 10);
    assert(token.len == 15 && token.startLine == 2 && token.endColumn == 14);
    token = xl6LexerNext(&lexer);
    assert(token.kind == XL6_TK_IDENT && token.startColumn == 15);
    assert(token.len == 4 && memcmp(token.text, "tail", 4) == 0);
    token = xl6LexerNext(&lexer);
    assert(token.kind == XL6_TK_EOF);
    xl6LexerClose(&lexer);
    ++checks;
}

int main(void)
{
    const char* examples[] = {
        "f\"{string.format(42, \"04\")}\"",
        "f\"{f\"{f\"{'a'}\"}\"}\"",
        "rf\"a\\{r\"C:\\{root}\\\"}\"",
        "f`{f`{`}`}`}`",
        "f\"{1 /* } /* { */ \" */ + 2}\"",
        "f\"{1 // } \"\r\n + 2}\"",
        "f\"{[\"}\", {\"k\": br\"a\\b\"}]}\"",
        "f\"{#2026-10-02#}\"",
        "f\"\"\"{f\"\"\"{\"a\"}\"\"\"}\"\"\"",
        "f\"a\\\"b {{x}} {\"c\"}\"",
        "\"\"\"escaped \\\"\"\" delimiter\"\"\"", "'\\''", "r\"C:\\tail\\\"",
        "b\"a\\0b\"", "br\"\"\"a\\b\"\"\"", "`a\n}b`"
    };
    const char* malformed[] = {
        "f\"{1 /* comment", "f\"{(1]}\"", "f\"{`text}",
        "f\"{#time\n}\"", "f\"plain\ntext\"", "f\"{\"unclosed}"
    };
    uint32_t state = 0x584C3612u;
    static const char alphabet[] = "rfb\"'`{}[]()/#*\\\r\n a\0";
    size_t n, iteration;
    XL6_LiteralSpan span;
    for ( n = 0; n < sizeof(examples)/sizeof(examples[0]); ++n ) truncated(examples[n]);
    for ( n = 0; n < sizeof(malformed)/sizeof(malformed[0]); ++n ) {
        span = xl6ScanLiteralSpan(malformed[n], strlen(malformed[n]), 0, 0);
        assert(span.error != NULL && span.end <= strlen(malformed[n]));
        ++checks;
    }
    assert(xl6ScanLiteralSpan(NULL, 0, 0, 0).error != NULL);
    assert(xl6ScanLiteralSpan("", 0, (size_t)-1, 0).end == 0);
    for ( iteration = 0; iteration < 100000; ++iteration ) {
        size_t len;
        char* data;
        state = state * 1664525u + 1013904223u;
        len = state % 257u;
        data = (char*)malloc(len == 0 ? 1 : len);
        assert(data != NULL);
        for ( n = 0; n < len; ++n ) {
            state = state * 1664525u + 1013904223u;
            data[n] = alphabet[state % (sizeof(alphabet) - 1)];
        }
        span = xl6ScanLiteralSpan(data, len, 0, iteration % 2);
        assert(span.end <= len);
        free(data);
        ++checks;
    }
    {
        char nested[2 + XL6_LITERAL_MAX_DEPTH * 2 + 3];
        memcpy(nested, "f\"{", 3);
        memset(nested + 3, '(', XL6_LITERAL_MAX_DEPTH);
        memset(nested + 3 + XL6_LITERAL_MAX_DEPTH, ')', XL6_LITERAL_MAX_DEPTH);
        memcpy(nested + 3 + XL6_LITERAL_MAX_DEPTH * 2, "}\"", 2);
        span = xl6ScanLiteralSpan(nested, sizeof(nested), 0, 0);
        assert(span.error != NULL && strstr(span.error, "128") != NULL);
        {
            XL6_Lexer lexer;
            XL6_Token token;
            char source[sizeof(nested) + 1];
            memcpy(source, nested, sizeof(nested));
            source[sizeof(nested)] = 0;
            assert(xl6LexerOpen(&lexer, source));
            token = xl6LexerNext(&lexer);
            assert(token.kind == XL6_TK_ERROR && strstr(token.message, "128") != NULL);
            xl6LexerClose(&lexer);
        }
        ++checks;
    }
    for ( iteration = 0; iteration < 1000; ++iteration ) {
        char source[129];
        size_t previous = 0, rounds = 0;
        XL6_Lexer lexer;
        XL6_Token token;
        for ( n = 0; n < sizeof(source) - 1; ++n ) {
            state = state * 1664525u + 1013904223u;
            source[n] = alphabet[state % (sizeof(alphabet) - 2)];
        }
        source[n] = 0;
        assert(xl6LexerOpen(&lexer, source));
        do {
            token = xl6LexerNext(&lexer);
            assert(token.endOffset <= strlen(source) && token.endOffset >= previous);
            assert(token.kind == XL6_TK_EOF || token.endOffset > previous);
            previous = token.endOffset;
            assert(++rounds <= sizeof(source));
        } while ( token.kind != XL6_TK_EOF );
        xl6LexerClose(&lexer);
        ++checks;
    }
    token_positions();
    printf("literal-span contract: %u checks passed\n", checks);
    return 0;
}
