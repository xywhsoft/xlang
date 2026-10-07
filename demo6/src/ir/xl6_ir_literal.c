#include "src/ir/xl6_ir.h"
#include <string.h>

/* Included by xl6_ir.c. Typed layout and CFG lowering share this one
 * source-literal normalization boundary; Cgen receives canonical bytes. */
/* A runtime string payload is already a C string literal by the time it
 * reaches XLIR.  This is intentionally here, at the source-to-IR boundary:
 * the C emitter receives no source spelling, quote style, raw-prefix, or
 * escape semantics to rediscover. */
static int xl6IrLiteralStringParts(
    const char* text, size_t* outStart, size_t* outEnd, int* outRaw)
{
    size_t len;
    size_t pos = 0;
    size_t delimiter;
    char quote;
    if ( text == NULL || outStart == NULL || outEnd == NULL || outRaw == NULL ) return 0;
    len = strlen(text);
    if ( len > 1 && text[0] == 'r' && (text[1] == '\'' || text[1] == '\"') ) {
        *outRaw = 1;
        pos = 1;
    } else {
        *outRaw = 0;
    }
    if ( pos >= len || (text[pos] != '\'' && text[pos] != '\"') ) return 0;
    quote = text[pos];
    delimiter = pos + 2 < len && text[pos + 1] == quote && text[pos + 2] == quote ? 3 : 1;
    if ( len < pos + delimiter * 2 ) return 0;
    if ( delimiter == 3 ) {
        if ( text[len - 1] != quote || text[len - 2] != quote || text[len - 3] != quote ) return 0;
    } else if ( text[len - 1] != quote ) {
        return 0;
    }
    *outStart = pos + delimiter;
    *outEnd = len - delimiter;
    return 1;
}

static int xl6IrLiteralAppendByte(char* output, size_t* length, unsigned char ch)
{
    size_t at = *length;
    switch ( ch ) {
    case '\n': output[at++] = '\\'; output[at++] = 'n'; break;
    case '\r': output[at++] = '\\'; output[at++] = 'r'; break;
    case '\t': output[at++] = '\\'; output[at++] = 't'; break;
    case '\a': output[at++] = '\\'; output[at++] = 'a'; break;
    case '\b': output[at++] = '\\'; output[at++] = 'b'; break;
    case '\f': output[at++] = '\\'; output[at++] = 'f'; break;
    case '\v': output[at++] = '\\'; output[at++] = 'v'; break;
    case '\\': output[at++] = '\\'; output[at++] = '\\'; break;
    case '\"': output[at++] = '\\'; output[at++] = '\"'; break;
    default:
        if ( ch < 0x20 || ch == 0x7f ) {
            output[at++] = '\\';
            output[at++] = (char)('0' + ((ch >> 6) & 7));
            output[at++] = (char)('0' + ((ch >> 3) & 7));
            output[at++] = (char)('0' + (ch & 7));
        } else {
            output[at++] = (char)ch;
        }
        break;
    }
    *length = at;
    return 1;
}

/* Dynamic member names are already semantic identifiers rather than source
 * string tokens.  Encode their bytes directly into the same canonical C
 * literal form used by literal-key object operations, so Cgen never needs to
 * interpret member spelling or manufacture an object key. */
char* xl6IrCanonicalCStringBytes(const char* text)
{
    size_t i;
    size_t length = 0;
    size_t textLength;
    char* output;

    if ( text == NULL ) return NULL;
    textLength = strlen(text);
    if ( textLength > (SIZE_MAX - 3) / 4 ) return NULL;
    output = (char*)xrtCalloc(textLength * 4 + 3, sizeof(*output));
    if ( output == NULL ) return NULL;
    output[length++] = '"';
    for ( i = 0; i < textLength; ++i ) {
        xl6IrLiteralAppendByte(output, &length, (unsigned char)text[i]);
    }
    output[length++] = '"';
    output[length] = '\0';
    return output;
}

static int xl6IrLiteralHexDigit(char ch)
{
    if ( ch >= '0' && ch <= '9' ) return ch - '0';
    if ( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
    if ( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
    return -1;
}

/* Canonicalize one already-delimited string byte range.  Interpolated text
 * parts do not carry their source quotes, while ordinary literals do; sharing
 * this range routine gives both paths exactly the same escape semantics. */
static char* xl6IrLiteralCStringRange(
    const char* text, size_t start, size_t end, int raw)
{
    size_t i;
    size_t length = 0;
    size_t capacity;
    char* output;
    if ( text == NULL || start > end || end > strlen(text) ) return NULL;
    /* Every UTF-8 input byte may become a three-digit C escape, plus quotes
     * and NUL.  Guard the allocation before calculating the capacity. */
    if ( end - start > (SIZE_MAX - 3) / 4 ) return NULL;
    capacity = (end - start) * 4 + 3;
    output = (char*)xrtCalloc(capacity, sizeof(*output));
    if ( output == NULL ) return NULL;
    output[length++] = '\"';
    for ( i = start; i < end; ++i ) {
        unsigned char ch = (unsigned char)text[i];
        if ( ch == '\r' ) {
            if ( i + 1 < end && text[i + 1] == '\n' ) ++i;
            ch = '\n';
        }
        if ( !raw && ch == '\\' ) {
            int digit;
            if ( ++i >= end ) { xrtFree(output); return NULL; }
            ch = (unsigned char)text[i];
            switch ( ch ) {
            case '\\': case '\'': case '\"': case '?': break;
            case 'a': ch = '\a'; break;
            case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case 'v': ch = '\v'; break;
            case 'x':
                if ( i + 1 >= end || (digit = xl6IrLiteralHexDigit(text[i + 1])) < 0 )
                    { xrtFree(output); return NULL; }
                ch = (unsigned char)digit;
                ++i;
                while ( i + 1 < end && (digit = xl6IrLiteralHexDigit(text[i + 1])) >= 0 ) {
                    if ( ch > 0x0f ) { xrtFree(output); return NULL; }
                    ch = (unsigned char)((ch << 4) | digit);
                    ++i;
                }
                break;
            default:
                if ( ch < '0' || ch > '7' ) { xrtFree(output); return NULL; }
                {
                    unsigned int value = (unsigned int)(ch - '0');
                    size_t count = 1;
                    while ( count < 3 && i + 1 < end && text[i + 1] >= '0' && text[i + 1] <= '7' ) {
                        value = value * 8u + (unsigned int)(text[++i] - '0');
                        ++count;
                    }
                    if ( value > 0xffu ) { xrtFree(output); return NULL; }
                    ch = (unsigned char)value;
                }
                break;
            }
        }
        xl6IrLiteralAppendByte(output, &length, ch);
    }
    output[length++] = '\"';
    output[length] = '\0';
    return output;
}

char* xl6IrCanonicalCStringLiteral(const char* text)
{
    size_t start;
    size_t end;
    int raw;
    if ( !xl6IrLiteralStringParts(text, &start, &end, &raw) ) return NULL;
    return xl6IrLiteralCStringRange(text, start, end, raw);
}

/* Named CallSite dictionary keys are semantic identifiers rather than source
 * string literals.  Encode that already-resolved spelling directly instead
 * of fabricating quoted AST text and sending it through the literal parser. */
char* xl6IrCanonicalIdentifierCString(const char* text)
{
    size_t length;
    size_t i;
    char* output;
    if ( text == NULL || text[0] == '\0' ) return NULL;
    length = strlen(text);
    if ( length > SIZE_MAX - 3 ) return NULL;
    for ( i = 0; i < length; ++i ) {
        unsigned char ch = (unsigned char)text[i];
        if ( ch < 0x20 || ch == 0x7f || ch == '\\' || ch == '"' ) return NULL;
    }
    output = (char*)xrtMalloc(length + 3);
    if ( output == NULL ) return NULL;
    output[0] = '"';
    memcpy(output + 1, text, length);
    output[length + 1] = '"';
    output[length + 2] = '\0';
    return output;
}
