#ifndef XL6_IMPORTS_H
#define XL6_IMPORTS_H

#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"
#include "src/package/xl6_package.h"
#include "src/compile/xl6_source_overlay.h"
#include "src/package/xl6_compiled_package.h"

typedef struct XL6_ImportNativeItem {
    char* packageId;
    int singleton;
    char* loadName;
    char* version;
} XL6_ImportNativeItem;

typedef struct XL6_ImportPackageInfo {
    char* name;
    char* version;
    char* abiVersion;
    char* dir;
    XL6_PackageStringList dependencies;
    int compiledRoot;
} XL6_ImportPackageInfo;

enum {
    XL6_IMPORT_FEATURE_TCC_RUNTIME = 1u << 0,
    XL6_IMPORT_FEATURE_XLANG_RUNTIME = 1u << 1,
    XL6_IMPORT_FEATURE_XLANG_COMPILER = 1u << 2,
    XL6_IMPORT_FEATURE_COMPILED_PACKAGE_CONSUMER = 1u << 3,
    XL6_IMPORT_FEATURE_COMPILED_INTERFACE_EXPANSION = 1u << 4
};

typedef struct XL6_ImportDeps {
    const XL6_SourceOverlay* sourceOverlay; /* Borrowed during this unit. */
    const XL6_CompiledPackageSet* compiledPackages; /* Borrowed for this call. */
    XL6_ImportPackageInfo* packageInfos;
    size_t packageInfoCount;
    XL6_PackageStringList cSources;
    XL6_PackageStringList includeDirs;
    XL6_PackageStringList xrtModules;
    XL6_PackageStringList runtimeSymbols; /* Explicit native C resident dependencies. */
    XL6_PackageStringList links;
    XL6_PackageStringList loadlibs;
    /*
     * 当前递归展开栈中的源码绝对路径。
     * 这里只跟踪“正在展开”的文件，不把已经完成的文件永久缓存，
     * 因此共享依赖仍可从不同导入分支正常进入。
     */
    XL6_PackageStringList activeSources;
    char** packageIds;
    size_t packageCount;
    XL6_ImportNativeItem* natives;
    size_t nativeCount;
    int hasUndeclaredCSourceModules;
    unsigned int features;
} XL6_ImportDeps;

void xl6ImportDepsInit(XL6_ImportDeps* deps);
void xl6ImportDepsFree(XL6_ImportDeps* deps);
int xl6ImportsExpandModule(XL6_AstNode* root, const char* sourcePath, XL6_DiagList* diags, XL6_ImportDeps* deps);

#endif
