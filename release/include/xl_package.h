#ifndef XL_COMPILED_PACKAGE_H
#define XL_COMPILED_PACKAGE_H
#include "xl.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XL_COMPILED_PACKAGE_VERSION 1u
#define XL_COMPILED_PACKAGE_COMPILER_ABI UINT64_C(0x584C36504B470001)
#define XL_COMPILED_PACKAGE_MAX_PACKAGES 64u
#define XL_COMPILED_PACKAGE_MAX_ENTRIES 65536u
#define XL_COMPILED_PACKAGE_MAX_INTERFACE (4u * 1024u * 1024u)

typedef struct XL_PackageEntry {
    const char* name;
    const void* address;
} XL_PackageEntry;

/* Compiler-produced, immutable interface. The input record/text may be freed
 * after import; native addresses remain borrowed, exactly as with registered
 * C functions. Their provider must outlive every context/module using them.
 * This record does NOT certify unloadable generated code as resident. */
typedef struct XL_CompiledPackage {
    size_t size;
    unsigned int version;
    unsigned int pointerBytes;
    uint64_t compilerAbi;
    const void* runtimeIdentity;
    const char* name;
    const char* packageVersion;
    const char* abiVersion;
    const unsigned char* interfaceData;
    size_t interfaceSize;
    uint64_t interfaceHash;
    const XL_PackageEntry* entries;
    size_t entryCount;
    const struct XL_CompiledPackage* const* dependencies;
    size_t dependencyCount;
} XL_CompiledPackage;

/* No implicit parent/global inheritance. Import includes the complete
 * dependency closure or leaves the context unchanged. Repeated identical
 * imports are idempotent. Conflicting package/symbol identities are errors. */
XL_API int xlImportCompiledPackage(XL_Context*, const XL_CompiledPackage*);
/* Borrowed diagnostic/introspection data, invalidated by context destruction.
 * Import does not publish this package's declarations without script import. */
XL_API const XL_CompiledPackage* xlFindCompiledPackage(XL_Context*, const char* name);
XL_API const void* xlCompiledPackageRuntimeIdentity(void);
XL_API uint64_t xlCompiledPackageInterfaceHash(const void*, size_t);

#ifdef __cplusplus
}
#endif
#endif
