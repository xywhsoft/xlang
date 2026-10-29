#include "src/abi/xl6_abi_json.h"

#include <string.h>

void xl6AbiJsonEscapedRange(XL6_Output* out, const char* text, size_t len)
{
    size_t i;

    if ( out == NULL || text == NULL ) {
        return;
    }
    for ( i = 0; i < len; ++i ) {
        unsigned char ch = (unsigned char)text[i];
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

void xl6AbiJsonEscaped(XL6_Output* out, const char* text)
{
    if ( text != NULL ) {
        xl6AbiJsonEscapedRange(out, text, strlen(text));
    }
}

void xl6AbiJsonString(XL6_Output* out, const char* text)
{
    xl6OutputPutc('"', out);
    xl6AbiJsonEscaped(out, text);
    xl6OutputPutc('"', out);
}

void xl6AbiJsonCStringSymbol(XL6_Output* out, const char* text)
{
    size_t len;

    xl6OutputPutc('"', out);
    if ( text != NULL ) {
        len = strlen(text);
        if ( len >= 2 && (text[0] == '"' || text[0] == '\'') ) {
            xl6AbiJsonEscapedRange(out, text + 1, len - 2);
        } else {
            xl6AbiJsonEscapedRange(out, text, len);
        }
    }
    xl6OutputPutc('"', out);
}
