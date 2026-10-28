#ifndef XL6_COMPILED_PACKAGE_H
#define XL6_COMPILED_PACKAGE_H
#include "include/xl_package.h"
#include "src/ast/xl6_ast.h"

typedef struct XL6_CompiledPackageSet {
    XL_CompiledPackage* items;
    size_t count;
} XL6_CompiledPackageSet;

/* Validates the complete bounded wire tree, including trailing bytes, before
 * any imported declaration is published. Allocations never enter a Context. */
int xl6CompiledPackageInterfaceValid(const void*, size_t);
XL6_AstNode* xl6CompiledPackageDecode(const void*, size_t);

#endif
