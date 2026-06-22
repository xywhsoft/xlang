#ifndef XL6_COMPILE_UNIT_H
#define XL6_COMPILE_UNIT_H

#include "src/compile/xl6_imports.h"
#include "src/parser/xl6_parser_api.h"

typedef struct XL6_CompileUnit {
    const char* path;
    const char* phase;
    char moduleName[256];
    XL6_ParseResult parse;
    XL6_ImportDeps importDeps;
    int semChecked;
    int success;
} XL6_CompileUnit;

void xl6CompileUnitInit(XL6_CompileUnit* unit);
void xl6CompileUnitFree(XL6_CompileUnit* unit);
int xl6CompileUnitLoadFile(XL6_CompileUnit* unit, const char* path, int runSem);
int xl6CompileUnitLoadSource(XL6_CompileUnit* unit, const char* source, const char* virtualPath, int runSem);

#endif
