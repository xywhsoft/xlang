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

static const XL6_XapiTypeDecl* findType(const XL6_XapiModule* module, const char* name)
{
    size_t i;

    for ( i = 0; i < module->typeCount; ++i ) {
        if ( textEquals(module->types[i].name, name) ) {
            return &module->types[i];
        }
    }
    return NULL;
}

static const XL6_XapiField* findField(const XL6_XapiTypeDecl* type, const char* name)
{
    size_t i;

    if ( type == NULL ) {
        return NULL;
    }
    for ( i = 0; i < type->fieldCount; ++i ) {
        if ( textEquals(type->fields[i].name, name) ) {
            return &type->fields[i];
        }
    }
    return NULL;
}

static int fail(const char* message)
{
    printf("%s\n", message);
    return 1;
}

int main(int argc, char** argv)
{
    XL6_XapiModule module;
    XL6_DiagList diags;
    const XL6_XapiTypeDecl* packet;
    const XL6_XapiTypeDecl* child;
    const XL6_XapiTypeDecl* box;
    const XL6_XapiTypeDecl* point;
    const XL6_XapiTypeDecl* classPayload;
    const XL6_XapiTypeDecl* payload;
    const XL6_XapiTypeDecl* nativeView;
    const XL6_XapiField* tag;
    const XL6_XapiField* code;
    const XL6_XapiField* ratio;
    const XL6_XapiField* pointField;
    const XL6_XapiField* d;
    int ok;

    if ( argc < 2 ) {
        return fail("missing xapi path");
    }
    if ( xrtInit() == NULL ) {
        return fail("xrt init failed");
    }

    xl6XapiModuleInit(&module);
    memset(&diags, 0, sizeof(diags));
    ok = xl6XapiLoadJsonFile(&module, argv[1], &diags);
    if ( !ok ) {
        xl6XapiModuleFree(&module);
        xrtUnit();
        return fail("load failed");
    }

    packet = findType(&module, "Packet");
    child = findType(&module, "ChildPacket");
    box = findType(&module, "Box");
    point = findType(&module, "Point");
    classPayload = findType(&module, "ClassPayload");
    payload = findType(&module, "Payload");
    nativeView = findType(&module, "NativeView");
    if ( packet == NULL || child == NULL || box == NULL || point == NULL || classPayload == NULL || payload == NULL || nativeView == NULL ) {
        xl6XapiModuleFree(&module);
        xrtUnit();
        return fail("missing loaded type");
    }
    if ( !textEquals(packet->kind, "class") || !textEquals(point->kind, "class") || !textEquals(classPayload->kind, "union") || !textEquals(payload->kind, "union") || !textEquals(nativeView->kind, "class") ) {
        xl6XapiModuleFree(&module);
        xrtUnit();
        return fail("type kind was not preserved");
    }
    if ( !textEquals(child->base, "Packet") ) {
        xl6XapiModuleFree(&module);
        xrtUnit();
        return fail("class base was not preserved");
    }
    if ( box->typeParamCount != 1 || !textEquals(box->typeParams[0], "T") ) {
        xl6XapiModuleFree(&module);
        xrtUnit();
        return fail("type params were not preserved");
    }

    tag = findField(packet, "tag");
    code = findField(packet, "code");
    ratio = findField(packet, "ratio");
    pointField = findField(classPayload, "point");
    d = findField(payload, "d");
    if ( tag == NULL || code == NULL || ratio == NULL || pointField == NULL || d == NULL ) {
        xl6XapiModuleFree(&module);
        xrtUnit();
        return fail("fields were not preserved");
    }
    if ( !textEquals(tag->type, "int") || !textEquals(tag->storage, "field") ) {
        xl6XapiModuleFree(&module);
        xrtUnit();
        return fail("field metadata was not preserved");
    }
    if ( tag->doc == NULL || strstr(tag->doc, "Packet tag field.") == NULL ) {
        xl6XapiModuleFree(&module);
        xrtUnit();
        return fail("field doc raw text was not preserved");
    }
    if ( !textEquals(code->storage, "inline-union") || !textEquals(ratio->type, "c_double") ) {
        xl6XapiModuleFree(&module);
        xrtUnit();
        return fail("inline union metadata was not preserved");
    }
    if ( !textEquals(pointField->type, "Point") || !textEquals(pointField->storage, "union") ) {
        xl6XapiModuleFree(&module);
        xrtUnit();
        return fail("union class field metadata was not preserved");
    }
    if ( !textEquals(d->type, "c_double") || !textEquals(d->storage, "union") ) {
        xl6XapiModuleFree(&module);
        xrtUnit();
        return fail("union field metadata was not preserved");
    }

    xl6XapiModuleFree(&module);
    xrtUnit();
    return 0;
}
