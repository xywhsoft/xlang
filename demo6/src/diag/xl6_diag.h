#ifndef XL6_DIAG_H
#define XL6_DIAG_H

#include "src/common/xl6_common.h"
#include "src/common/xl6_output.h"

typedef struct XL6_Diag {
    int line;
    int column;
    char* message;
    char* file;
} XL6_Diag;

typedef struct XL6_DiagList {
    XL6_Diag* items;
    size_t count;
    size_t capacity;
    const char* defaultFile; /* Borrowed only while adding diagnostics. */
} XL6_DiagList;

void xl6DiagInit(XL6_DiagList* list);
void xl6DiagFree(XL6_DiagList* list);
void xl6DiagAdd(XL6_DiagList* list, int line, int column, const char* message);
void xl6DiagAddFile(XL6_DiagList* list, const char* file, int line, int column, const char* message);
void xl6DiagPrintText(const XL6_DiagList* list, const char* fileName);
void xl6DiagPrintTextWithPhase(const XL6_DiagList* list, const char* fileName, const char* phase);
struct XL6_SourceReport;
void xl6DiagPrintJsonWithSources(XL6_Output* out, const XL6_DiagList* list,
    const char* fileName, const char* phase, int ok, const struct XL6_SourceReport* report);
void xl6DiagPrintJsonWithCompletion(XL6_Output* out,const struct XL6_SourceReport* report,
    int available,const char* completionJson);
void xl6DiagPrintJsonWithIndex(XL6_Output* out, const XL6_DiagList* list,
    const char* fileName, const char* phase, int ok, const struct XL6_SourceReport* report, const char* indexJson);
void xl6DiagPrintJsonWithBridge(XL6_Output* out, const XL6_DiagList* list,
    const char* fileName, const char* phase, int ok, const struct XL6_SourceReport* report,
    const char* headerText, const char* preamble, const char* guardName,
    const char* environmentJson, int withEnvironment, const char* mapJson);
void xl6DiagPrintJson(XL6_Output* out, const XL6_DiagList* list, const char* fileName, const char* phase, int ok);

#endif
