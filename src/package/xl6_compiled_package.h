#ifndef XL6_COMPILED_PACKAGE_H
#define XL6_COMPILED_PACKAGE_H
#include "include/xl_package.h"
#include "src/ast/xl6_ast.h"
#include "src/package/xl6_package.h"

typedef struct XL6_CompiledPackageSet {
    XL_CompiledPackage* items;
    size_t count;
} XL6_CompiledPackageSet;

/* Validates the complete bounded wire tree, including trailing bytes, before
 * any imported declaration is published. Allocations never enter a Context. */
int xl6CompiledPackageInterfaceValid(const void*, size_t);
XL6_AstNode* xl6CompiledPackageDecode(const void*, size_t);

/* Private XLP2 root payload. Owning on Decode, borrowed on Encode. Only
 * manifest-declared dependency names provide native capability authority. */
typedef struct XL6_CompiledNativeFeatures {
    char *name, *version, *abiVersion;
    XL6_PackageStringList required, provided, dependencies;
} XL6_CompiledNativeFeatures;

#define XL6_COMPILED_NATIVE_FEATURES_TAG UINT32_C(0x31464e58)
void xl6CompiledNativeFeaturesFree(XL6_CompiledNativeFeatures*);
int xl6CompiledNativeFeaturesDecode(const void*, size_t, XL6_CompiledNativeFeatures*);
int xl6CompiledNativeFeaturesEncode(const XL6_CompiledNativeFeatures*, unsigned char**, size_t*);
/* Checks the wire tree plus descriptor identity and declared dependency
 * membership without allocation. Legacy XLP1 remains accepted with no caps. */
int xl6CompiledPackageDescriptorInterfaceValid(const XL_CompiledPackage*);
int xl6CompiledPackageFeaturesDecode(const XL_CompiledPackage*, XL6_CompiledNativeFeatures*);

#endif
