#ifndef XL6_DIAG_H
#define XL6_DIAG_H

#include "src/common/xl6_common.h"

typedef struct XL6_Diag {
    int line;
    int column;
    char* message;
} XL6_Diag;

typedef struct XL6_DiagList {
    XL6_Diag* items;
    size_t count;
    size_t capacity;
} XL6_DiagList;

void xl6DiagInit(XL6_DiagList* list);
void xl6DiagFree(XL6_DiagList* list);
void xl6DiagAdd(XL6_DiagList* list, int line, int column, const char* message);
void xl6DiagPrintText(const XL6_DiagList* list, const char* fileName);
void xl6DiagPrintJson(FILE* out, const XL6_DiagList* list, const char* fileName, int ok);

#endif
