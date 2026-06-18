#include "src/diag/xl6_diag.h"

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
        xl6Free(list->items[i].message);
    }
    xl6Free(list->items);
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
        newItems = (XL6_Diag*)realloc(list->items, newCapacity * sizeof(XL6_Diag));
        if ( newItems == NULL ) {
            return;
        }
        list->items = newItems;
        list->capacity = newCapacity;
    }

    list->items[list->count].line = line;
    list->items[list->count].column = column;
    list->items[list->count].message = xl6StrDupRange(message, strlen(message));
    list->count += 1;
}

void xl6DiagPrintText(const XL6_DiagList* list, const char* fileName)
{
    size_t i;

    if ( list == NULL ) {
        return;
    }
    for ( i = 0; i < list->count; ++i ) {
        fprintf(stderr, "%s:%d:%d: %s\n",
                fileName != NULL ? fileName : "<source>",
                list->items[i].line,
                list->items[i].column,
                list->items[i].message != NULL ? list->items[i].message : "error");
    }
}

static void xl6DiagJsonEscaped(FILE* out, const char* text)
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

void xl6DiagPrintJson(FILE* out, const XL6_DiagList* list, const char* fileName, int ok)
{
    size_t i;
    size_t count;

    if ( out == NULL ) {
        return;
    }
    count = list != NULL ? list->count : 0;
    fprintf(out, "{\"ok\":%s,\"diagnostics\":[", ok ? "true" : "false");
    for ( i = 0; i < count; ++i ) {
        if ( i > 0 ) {
            fputc(',', out);
        }
        fputs("{\"file\":\"", out);
        xl6DiagJsonEscaped(out, fileName != NULL ? fileName : "<source>");
        fprintf(out, "\",\"line\":%d,\"column\":%d,\"message\":\"",
                list->items[i].line,
                list->items[i].column);
        xl6DiagJsonEscaped(out, list->items[i].message != NULL ? list->items[i].message : "error");
        fputs("\"}", out);
    }
    fputs("]}\n", out);
}
