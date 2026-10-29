#ifndef XL6_IR_COMPILED_PACKAGE_H
#define XL6_IR_COMPILED_PACKAGE_H
/* Owned deployment metadata. Entries refer to canonical XLIR call targets;
 * no AST, source file or generated-C scan participates in emission. */
typedef struct XL6_IrCompiledPackage {
    char* name;
    char* version;
    char* abiVersion;
    unsigned char* interfaceData;
    size_t interfaceSize;
    unsigned long long interfaceHash;
    unsigned long long bindingId;
    unsigned int* entries;
    size_t entryCount;
    size_t* dependencies;
    size_t dependencyCount;
} XL6_IrCompiledPackage;
#endif
