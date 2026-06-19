#include "src/compile/xl6_compile_unit.h"

#include "src/compile/xl6_imports.h"
#include "src/sem/xl6_sem.h"

#include <stdio.h>
#include <string.h>

static void xl6CompileUnitBaseNameNoExt(char* out, size_t outSize, const char* path)
{
    const char* base;
    const char* p;
    const char* dot;
    size_t len;

    if ( out == NULL || outSize == 0 ) {
        return;
    }

    base = path != NULL ? path : "module";
    for ( p = base; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' ) {
            base = p + 1;
        }
    }

    dot = NULL;
    for ( p = base; *p != '\0'; ++p ) {
        if ( *p == '.' ) {
            dot = p;
        }
    }

    len = dot != NULL ? (size_t)(dot - base) : strlen(base);
    if ( len == 0 ) {
        base = "module";
        len = strlen(base);
    }
    if ( len >= outSize ) {
        len = outSize - 1;
    }
    memcpy(out, base, len);
    out[len] = '\0';
}

void xl6CompileUnitInit(XL6_CompileUnit* unit)
{
    if ( unit == NULL ) {
        return;
    }
    memset(unit, 0, sizeof(*unit));
    xl6ParseResultInit(&unit->parse);
}

void xl6CompileUnitFree(XL6_CompileUnit* unit)
{
    if ( unit == NULL ) {
        return;
    }
    xl6ParseResultFree(&unit->parse);
    memset(unit, 0, sizeof(*unit));
}

int xl6CompileUnitLoadFile(XL6_CompileUnit* unit, const char* path, int runSem)
{
    int ok;

    if ( unit == NULL || path == NULL ) {
        return 0;
    }

    unit->path = path;
    unit->phase = "parse";
    xl6CompileUnitBaseNameNoExt(unit->moduleName, sizeof(unit->moduleName), path);

    ok = xl6ParseFile(path, &unit->parse);
    if ( ok ) {
        unit->phase = "import";
        ok = xl6ImportsExpandModule(unit->parse.root, path, &unit->parse.diags);
    }
    if ( ok && runSem ) {
        unit->phase = "sem";
        ok = xl6SemCheckModule(unit->parse.root, &unit->parse.diags);
        unit->semChecked = 1;
    }
    unit->success = ok && unit->parse.diags.count == 0;
    if ( unit->success ) {
        unit->phase = "checked";
    }
    return unit->success;
}
