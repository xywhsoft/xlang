#include "src/diag/xl6_diag.h"
#include "src/compile/xl6_source_report.h"
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
        xrtFree(list->items[i].file);
    }
    xrtFree(list->items);
    memset(list, 0, sizeof(*list));
}

void xl6DiagAdd(XL6_DiagList* list, int line, int column, const char* message)
{
    xl6DiagAddFile(list, list ? list->defaultFile : NULL, line, column, message);
}

void xl6DiagAddFile(XL6_DiagList* list, const char* file, int line, int column, const char* message)
{
    XL6_Diag* newItems;
    size_t newCapacity;
    size_t i;
    const char* text;

    if ( list == NULL ) {
        return;
    }
    text = message != NULL ? message : "error";
    for ( i = 0; i < list->count; ++i ) {
        const XL6_Diag* item = &list->items[i];
        if ( item->line == line &&
             item->column == column &&
             ((item->file == NULL && file == NULL) ||
              (item->file != NULL && file != NULL && strcmp(item->file, file) == 0)) &&
             item->message != NULL &&
             strcmp(item->message, text) == 0 ) {
            return;
        }
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
    list->items[list->count].message = xrtStrDupN(text, strlen(text));
    list->items[list->count].file = file ? xrtStrDupN(file, strlen(file)) : NULL;
    if (!list->items[list->count].message || (file && !list->items[list->count].file)) {
        xrtFree(list->items[list->count].message);
        xrtFree(list->items[list->count].file);
        return;
    }
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
                         list->items[i].file ? list->items[i].file : (fileName != NULL ? fileName : "<source>"),
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
                         list->items[i].file ? list->items[i].file : (fileName != NULL ? fileName : "<source>"),
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
    xl6DiagPrintJsonWithSources(out, list, fileName, phase, ok, NULL);
}

static void xl6DiagJsonHash(XL6_Output* out, const unsigned char hash[32])
{
    size_t i;
    for (i = 0; i < 32; ++i) xl6OutputPrintf(out, "%02x", (unsigned)hash[i]);
}

static void xl6DiagPrintJsonExtended(XL6_Output* out, const XL6_DiagList* list,
    const char* fileName, const char* phase, int ok, const XL6_SourceReport* report, const char* indexJson, const char* completionJson,
    int withBridge, const char* headerText, const char* preamble, const char* guardName, const char* environmentJson, const char* mapJson)
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
        xl6DiagJsonEscaped(out, list->items[i].file ? list->items[i].file : (fileName != NULL ? fileName : "<source>"));
        xl6OutputPrintf(out, "\",\"line\":%d,\"column\":%d,\"message\":\"",
                list->items[i].line,
                list->items[i].column);
        xl6DiagJsonEscaped(out, list->items[i].message != NULL ? list->items[i].message : "error");
        xl6OutputPuts("\"}", out);
    }
    xl6OutputPutc(']', out);
    if (report) {
        xl6OutputPrintf(out, ",\"sourceReport\":{\"schema\":2,\"scope\":\"loaded-sources-and-manifests\",\"complete\":%s,\"error\":\"",
                         report->complete ? "true" : "false");
        xl6DiagJsonEscaped(out, report->error);
        xl6OutputPuts("\",\"errorFile\":\"", out);
        xl6DiagJsonEscaped(out, report->errorPath);
        xl6OutputPuts("\",\"files\":[", out);
        for (i = 0; i < report->count; ++i) {
            const XL6_SourceReportItem* item = &report->items[i];
            if (i) xl6OutputPutc(',', out);
            xl6OutputPuts("{\"path\":\"", out);
            xl6DiagJsonEscaped(out, item->path);
            xl6OutputPrintf(out, "\",\"origin\":\"%s\",\"kind\":%d,\"textSize\":%lu,\"rawSize\":%lu,\"textSha256\":\"",
                item->origin, item->kind, (unsigned long)item->size, (unsigned long)item->rawSize);
            xl6DiagJsonHash(out, item->textSha256);
            xl6OutputPuts("\",\"rawSha256\":\"", out);
            xl6DiagJsonHash(out, item->rawSha256);
            xl6OutputPrintf(out, "\",\"metadata\":{\"available\":%u,\"device\":\"%016llx\",\"identity\":\"%016llx\",\"modifiedMs\":\"%lld\",\"changedMs\":\"%lld\",\"attributes\":%u}}",
                (unsigned)item->info.Available, (unsigned long long)item->info.Device,
                (unsigned long long)item->info.Identity, (long long)item->info.Modified,
                (long long)item->info.Changed, (unsigned)item->info.Attributes);
        }
        xl6OutputPuts("]},\"resolutionReport\":{\"schema\":1,\"scope\":\"xlang-import-resolution\",\"probes\":[", out);
        for (i = 0; i < report->probeCount; ++i) {
            if (i) xl6OutputPutc(',', out);
            xl6OutputPuts("{\"path\":\"", out);
            xl6DiagJsonEscaped(out, report->probes[i].path);
            xl6OutputPrintf(out, "\",\"exists\":%s}", report->probes[i].exists ? "true" : "false");
        }
        xl6OutputPrintf(out, "],\"packagePathObserved\":%s,\"XLANG_PACKAGE_PATH\":", report->packagePathObserved ? "true" : "false");
        if (report->packagePath) { xl6OutputPutc('"', out); xl6DiagJsonEscaped(out, report->packagePath); xl6OutputPutc('"', out); }
        else xl6OutputPuts("null", out);
        xl6OutputPutc('}', out);
    }
    if (indexJson) { xl6OutputPuts(",\"semanticIndex\":", out); xl6OutputPuts(indexJson, out); }
    if (completionJson) { xl6OutputPuts(",\"completion\":", out); xl6OutputPuts(completionJson, out); }
    if (withBridge) {
        int complete = ok && headerText != NULL && report != NULL && report->complete;
        xl6OutputPrintf(out, ",\"cBridge\":{\"schema\":1,\"scope\":\"lowered-xlang-native-declarations\",\"complete\":%s,\"cInputsObserved\":false,\"headerGuard\":\"",
            complete ? "true" : "false");
        xl6DiagJsonEscaped(out, guardName);
        xl6OutputPuts("\",\"runtimePreamble\":\"", out);
        xl6DiagJsonEscaped(out, complete ? preamble : "");
        xl6OutputPuts("\",\"declarations\":\"", out);
        xl6DiagJsonEscaped(out, complete ? headerText : "");
        xl6OutputPutc('"', out);
        if (withBridge > 1) {
            xl6OutputPuts(",\"nativeEnvironment\":", out);
            xl6OutputPuts(complete && environmentJson ? environmentJson :
                "{\"schema\":1,\"scope\":\"compiler-configured-native-c\",\"complete\":false,\"operations\":[],\"builtinHeaders\":[]}", out);
        }
        xl6OutputPuts(",\"declarationMap\":", out);
        xl6OutputPuts(complete && mapJson ? mapJson :
            "{\"schema\":1,\"scope\":\"lowered-public-symbol-source-bindings\",\"complete\":false,\"records\":[]}", out);
        xl6OutputPutc('}', out);
    }
    xl6OutputPuts("}\n", out);
}

void xl6DiagPrintJsonWithIndex(XL6_Output* out,const XL6_DiagList* list,
    const char* fileName,const char* phase,int ok,const XL6_SourceReport* report,const char* indexJson)
{
    xl6DiagPrintJsonExtended(out,list,fileName,phase,ok,report,indexJson,NULL,0,NULL,NULL,NULL,NULL,NULL);
}
void xl6DiagPrintJsonWithCompletion(XL6_Output* out,const XL6_SourceReport* report,int available,const char* completionJson)
{
    xl6DiagPrintJsonExtended(out,NULL,NULL,"completion",available,report,NULL,completionJson,0,NULL,NULL,NULL,NULL,NULL);
}

void xl6DiagPrintJsonWithBridge(XL6_Output* out, const XL6_DiagList* list,
    const char* fileName, const char* phase, int ok, const XL6_SourceReport* report,
    const char* headerText, const char* preamble, const char* guardName,
    const char* environmentJson, int withEnvironment, const char* mapJson)
{
    xl6DiagPrintJsonExtended(out,list,fileName,phase,ok,report,NULL,NULL,withEnvironment ? 2 : 1,headerText,preamble,guardName,environmentJson,mapJson);
}

void xl6DiagPrintJsonWithSources(XL6_Output* out, const XL6_DiagList* list,
    const char* fileName, const char* phase, int ok, const XL6_SourceReport* report)
{
    xl6DiagPrintJsonWithIndex(out, list, fileName, phase, ok, report, NULL);
}
