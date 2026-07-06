#include "src/abi/xl6_xapi_load.h"

#include <stdio.h>
#include <string.h>

void xl6DiagAdd(XL6_DiagList* list, int line, int column, const char* message)
{
    (void)list;
    fprintf(stderr, "%d:%d: %s\n", line, column, message != NULL ? message : "diagnostic");
}

void xl6XapiModuleInit(XL6_XapiModule* module)
{
    if ( module != NULL ) {
        memset(module, 0, sizeof(*module));
    }
}

void xl6XapiModuleFree(XL6_XapiModule* module)
{
    size_t i;
    size_t j;

    if ( module == NULL ) {
        return;
    }
    xrtFree(module->packageName);
    xrtFree(module->packageVersion);
    xrtFree(module->packageAbiVersion);
    xrtFree(module->packageId);
    xrtFree(module->name);
    xrtFree(module->moduleId);
    xrtFree(module->source);
    xrtFree(module->sourceHash);
    for ( i = 0; i < module->exportCount; ++i ) {
        XL6_XapiExport* item = &module->exports[i];
        xrtFree(item->kind);
        xrtFree(item->name);
        xrtFree(item->cSymbol);
        xrtFree(item->type);
        xrtFree(item->signature);
        xrtFree(item->exportId);
        xrtFree(item->callingConvention);
        xrtFree(item->varargMode);
        xrtFree(item->doc);
        for ( j = 0; j < item->paramCount; ++j ) {
            xrtFree(item->params[j].name);
            xrtFree(item->params[j].type);
        }
        xrtFree(item->params);
        for ( j = 0; j < item->returnCount; ++j ) {
            xrtFree(item->returns[j]);
        }
        xrtFree(item->returns);
    }
    xrtFree(module->exports);
    for ( i = 0; i < module->typeCount; ++i ) {
        XL6_XapiTypeDecl* item = &module->types[i];
        xrtFree(item->kind);
        xrtFree(item->name);
        xrtFree(item->doc);
        xrtFree(item->base);
        for ( j = 0; j < item->typeParamCount; ++j ) {
            xrtFree(item->typeParams[j]);
        }
        xrtFree(item->typeParams);
        for ( j = 0; j < item->fieldCount; ++j ) {
            xrtFree(item->fields[j].name);
            xrtFree(item->fields[j].type);
            xrtFree(item->fields[j].doc);
            xrtFree(item->fields[j].storage);
        }
        xrtFree(item->fields);
    }
    xrtFree(module->types);
    for ( i = 0; i < module->importCount; ++i ) {
        xrtFree(module->imports[i].target);
        xrtFree(module->imports[i].mode);
        xrtFree(module->imports[i].resolvedKind);
        xrtFree(module->imports[i].manifestPath);
        xrtFree(module->imports[i].interfacePath);
        xrtFree(module->imports[i].packageId);
    }
    xrtFree(module->imports);
    for ( i = 0; i < module->linkCount; ++i ) {
        xrtFree(module->links[i]);
    }
    xrtFree(module->links);
    for ( i = 0; i < module->loadlibCount; ++i ) {
        xrtFree(module->loadlibs[i]);
    }
    xrtFree(module->loadlibs);
    for ( i = 0; i < module->cSourceCount; ++i ) {
        xrtFree(module->cSources[i]);
    }
    xrtFree(module->cSources);
    xrtFree(module->nativeLoadName);
    xrtFree(module->nativeVersion);
    memset(module, 0, sizeof(*module));
}

static int textEquals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static int fail(XL6_XapiModule* module, const char* message)
{
    xl6XapiModuleFree(module);
    xrtUnit();
    printf("%s\n", message);
    return 1;
}

static int checkUnknowns(XL6_XapiModule* module)
{
    const XL6_XapiExport* fn;

    if ( !textEquals(module->packageName, "unknowns") ||
         !textEquals(module->packageVersion, "2.1.0") ||
         !textEquals(module->packageAbiVersion, "7") ||
         !textEquals(module->packageId, "unknowns@2.1.0#abi7") ||
         !textEquals(module->moduleId, "unknowns@2.1.0#abi7::unknowns") ) {
        return 0;
    }
    if ( module->exportCount != 1 || module->typeCount != 1 ||
         module->linkCount != 1 || module->loadlibCount != 1 || module->cSourceCount != 1 ) {
        return 0;
    }
    if ( !textEquals(module->links[0], "User32") ||
         !textEquals(module->loadlibs[0], "tool.dll") ||
         !textEquals(module->cSources[0], "native.c") ||
         module->nativeSingleton != 1 ||
         !textEquals(module->nativeLoadName, "unknown_native") ||
         !textEquals(module->nativeVersion, "2.1.0") ) {
        return 0;
    }
    fn = &module->exports[0];
    if ( !textEquals(fn->callingConvention, "cdecl") ||
         !textEquals(fn->varargMode, "none") ||
         !textEquals(fn->signature, "func(string) as bool") ||
         !textEquals(fn->exportId, "unknowns@2.1.0#abi7::unknowns::touch(func(string) as bool)") ||
         !textEquals(fn->doc, "function doc") ) {
        return 0;
    }
    if ( !textEquals(module->types[0].doc, "type doc") ) {
        return 0;
    }
    if ( module->types[0].fieldCount != 1 ||
         !textEquals(module->types[0].fields[0].storage, "field") ||
         !textEquals(module->types[0].fields[0].doc, "field doc") ) {
        return 0;
    }
    return 1;
}

int main(int argc, char** argv)
{
    XL6_XapiModule module;
    XL6_DiagList diags;
    int ok;

    if ( argc < 3 ) {
        printf("usage: xapi_loader_defaults_host <unknowns> <xapi>\n");
        return 1;
    }
    if ( xrtInit() == NULL ) {
        printf("xrt init failed\n");
        return 1;
    }
    xl6XapiModuleInit(&module);
    memset(&diags, 0, sizeof(diags));
    if ( !xl6XapiLoadJsonFile(&module, argv[2], &diags) ) {
        return fail(&module, "load failed");
    }
    if ( strcmp(argv[1], "unknowns") == 0 ) {
        ok = checkUnknowns(&module);
    } else {
        ok = 0;
    }
    if ( !ok ) {
        return fail(&module, "loaded metadata did not match expectations");
    }
    xl6XapiModuleFree(&module);
    xrtUnit();
    return 0;
}
