#ifndef XL6_IMPORTS_H
#define XL6_IMPORTS_H

#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"
#include "src/package/xl6_package.h"

typedef struct XL6_ImportNativeItem {
    char* packageId;
    int singleton;
    char* loadName;
    char* version;
} XL6_ImportNativeItem;

typedef struct XL6_ImportDeps {
    XL6_PackageStringList cSources;
    XL6_PackageStringList links;
    XL6_PackageStringList loadlibs;
    char** packageIds;
    size_t packageCount;
    XL6_ImportNativeItem* natives;
    size_t nativeCount;
} XL6_ImportDeps;

void xl6ImportDepsInit(XL6_ImportDeps* deps);
void xl6ImportDepsFree(XL6_ImportDeps* deps);
int xl6ImportsExpandModule(XL6_AstNode* root, const char* sourcePath, XL6_DiagList* diags, XL6_ImportDeps* deps);

#endif
