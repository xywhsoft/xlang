#include "src/diag/xl6_diag.h"
#include "src/common/xl6_console.h"

void xl6DiagInit(XL6_DiagList* list)
{
    memset(list, 0, sizeof(*list));
}

void xl6DiagFree(XL6_DiagList* list)
{
    size_t i;

    if ( list == NULL ) {
        return;
    }
    for ( i = 0; i < list->count; ++i ) {
        xrtFree(list->items[i].message);
    }
    xrtFree(list->items);
    memset(list, 0, sizeof(*list));
}

void xl6DiagAdd(XL6_DiagList* list, int line, int column, const char* message)
{
    XL6_Diag* newItems;
    size_t newCapacity;

    if ( list == NULL ) {
        return;
    }
    if ( list->count == list->capacity ) {
        newCapacity = (list->capacity == 0) ? 8 : list->capacity * 2;
        newItems = (XL6_Diag*)xrtRealloc(list->items, newCapacity * sizeof(XL6_Diag));
        if ( newItems == NULL ) {
            return;
        }
        list->items = newItems;
        list->capacity = newCapacity;
    }

    list->items[list->count].line = line;
    list->items[list->count].column = column;
    list->items[list->count].message = xrtCopyStr((char*)message, strlen(message));
    list->count += 1;
}

void xl6DiagPrintText(const XL6_DiagList* list, const char* fileName)
{
    size_t i;

    if ( list == NULL ) {
        return;
    }
    for ( i = 0; i < list->count; ++i ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR,
                         "%s:%d:%d: %s\n",
                         fileName != NULL ? fileName : "<source>",
                         list->items[i].line,
                         list->items[i].column,
                         list->items[i].message != NULL ? list->items[i].message : "error");
    }
}

void xl6DiagPrintTextWithPhase(const XL6_DiagList* list, const char* fileName, const char* phase)
{
    size_t i;
    const char* phaseText;

    if ( list == NULL ) {
        return;
    }
    phaseText = phase != NULL ? phase : "compile";
    if ( list->count == 0 ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s: [%s] failed\n", fileName != NULL ? fileName : "<source>", phaseText);
        return;
    }
    for ( i = 0; i < list->count; ++i ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR,
                         "%s:%d:%d: [%s] %s\n",
                         fileName != NULL ? fileName : "<source>",
                         list->items[i].line,
                         list->items[i].column,
                         phaseText,
                         list->items[i].message != NULL ? list->items[i].message : "error");
    }
}

static void xl6DiagJsonEscaped(XL6_Output* out, const char* text)
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

void xl6DiagPrintJson(XL6_Output* out, const XL6_DiagList* list, const char* fileName, const char* phase, int ok)
{
    size_t i;
    size_t count;

    if ( out == NULL ) {
        return;
    }
    count = list != NULL ? list->count : 0;
    xl6OutputPrintf(out, "{\"ok\":%s,\"phase\":\"", ok ? "true" : "false");
    xl6DiagJsonEscaped(out, phase != NULL ? phase : "compile");
    xl6OutputPuts("\",\"diagnostics\":[", out);
    for ( i = 0; i < count; ++i ) {
        if ( i > 0 ) {
            xl6OutputPutc(',', out);
        }
        xl6OutputPuts("{\"file\":\"", out);
        xl6DiagJsonEscaped(out, fileName != NULL ? fileName : "<source>");
        xl6OutputPrintf(out, "\",\"line\":%d,\"column\":%d,\"message\":\"",
                list->items[i].line,
                list->items[i].column);
        xl6DiagJsonEscaped(out, list->items[i].message != NULL ? list->items[i].message : "error");
        xl6OutputPuts("\"}", out);
    }
    xl6OutputPuts("]}\n", out);
}
