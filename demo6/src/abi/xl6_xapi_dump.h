#ifndef XL6_XAPI_DUMP_H
#define XL6_XAPI_DUMP_H

#include "src/ast/xl6_ast.h"
#include "src/common/xl6_output.h"

#include <stddef.h>

typedef enum XL6_XapiEntryKind {
    XL6_XAPI_ENTRY_NONE,
    XL6_XAPI_ENTRY_XLANG_MAIN,
    XL6_XAPI_ENTRY_SCRIPT_MAIN
} XL6_XapiEntryKind;

typedef struct XL6_XapiParam {
    char* name;
    char* type;
    int optional;
    int vararg;
} XL6_XapiParam;

typedef struct XL6_XapiExport {
    char* kind;
    char* name;
    char* owner;
    char* cSymbol;
    char* type;
    char* signature;
    char* exportId;
    char* callingConvention;
    char* varargMode;
    char* doc;
    int requiredParamCount;
    int fixedParamCount;
    XL6_XapiParam* params;
    size_t paramCount;
    char** returns;
    size_t returnCount;
} XL6_XapiExport;

typedef struct XL6_XapiField {
    char* name;
    char* type;
    char* doc;
    char* storage;
} XL6_XapiField;

typedef struct XL6_XapiTypeDecl {
    char* kind;
    char* name;
    char* doc;
    char* base;
    char** typeParams;
    size_t typeParamCount;
    XL6_XapiField* fields;
    size_t fieldCount;
} XL6_XapiTypeDecl;

typedef struct XL6_XapiImport {
    char* target;
    char* mode;
    int resolved;
    char* resolvedKind;
    char* manifestPath;
    char* interfacePath;
    char* packageId;
} XL6_XapiImport;

typedef struct XL6_XapiModule {
    char* packageName;
    char* packageVersion;
    char* packageAbiVersion;
    char* packageId;
    char* name;
    char* moduleId;
    char* source;
    char* sourceHash;
    XL6_XapiEntryKind entryKind;
    XL6_XapiExport* exports;
    size_t exportCount;
    XL6_XapiTypeDecl* types;
    size_t typeCount;
    XL6_XapiImport* imports;
    size_t importCount;
    char** links;
    size_t linkCount;
    char** loadlibs;
    size_t loadlibCount;
    char** cSources;
    size_t cSourceCount;
    int nativeSingleton;
    char* nativeLoadName;
    char* nativeVersion;
} XL6_XapiModule;

void xl6XapiModuleInit(XL6_XapiModule* module);
void xl6XapiModuleFree(XL6_XapiModule* module);
int xl6XapiModuleBuild(XL6_XapiModule* module, const XL6_AstNode* root, const char* sourcePath, const char* moduleName);
int xl6XapiModuleHasExecutableEntry(const XL6_XapiModule* module);
void xl6XapiDumpJsonModule(XL6_Output* out, const XL6_XapiModule* module);
void xl6XapiDumpJson(XL6_Output* out, const XL6_AstNode* root, const char* sourcePath, const char* moduleName);

#endif
