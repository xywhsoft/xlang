#include "src/common/xl6_xrt.h"
#include "src/package/xl6_compiled_package.h"
#include "src/compile/xl6_imports.h"
#include "src/parser/xl6_parser_api.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put32(unsigned char* p, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (unsigned char)(value >> (8u * i));
}

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
}

static XL_CompiledPackage makePackage(const char* name, const char** required, size_t nr,
    const char** provided, size_t np, const char** declared, size_t nd,
    const char** imports, size_t ni, int legacy)
{
    XL_CompiledPackage result = {0}; XL6_CompiledNativeFeatures metadata = {0};
    metadata.name = (char*)name; metadata.version = "1"; metadata.abiVersion = "1";
    metadata.required = (XL6_PackageStringList){(char**)required, nr};
    metadata.provided = (XL6_PackageStringList){(char**)provided, np};
    metadata.dependencies = (XL6_PackageStringList){(char**)declared, nd};
    unsigned char* payload = NULL; size_t payloadSize = 0;
    if (!legacy) assert(xl6CompiledNativeFeaturesEncode(&metadata, &payload, &payloadSize));
    size_t bytes = 40 + payloadSize;
    for (size_t i = 0; i < ni; ++i) bytes += 36 + strlen(imports[i]);
    unsigned char* wire = (unsigned char*)xrtCalloc(bytes, 1); assert(wire);
    memcpy(wire, legacy ? "XLP1" : "XLP2", 4);
    put32(wire + 16, (uint32_t)payloadSize); put32(wire + 20, (uint32_t)ni);
    if (!legacy) {
        put32(wire + 32, XL6_COMPILED_NATIVE_FEATURES_TAG); put32(wire + 36, 1);
        memcpy(wire + 40, payload, payloadSize);
    }
    size_t pos = 40 + payloadSize;
    for (size_t i = 0; i < ni; ++i) {
        size_t n = strlen(imports[i]); put32(wire + pos, XL6_AST_IMPORT);
        put32(wire + pos + 8, (uint32_t)n); memcpy(wire + pos + 36, imports[i], n); pos += 36 + n;
    }
    xrtFree(payload);
    result.size = sizeof(result); result.version = XL_COMPILED_PACKAGE_VERSION;
    result.pointerBytes = sizeof(void*); result.compilerAbi = XL_COMPILED_PACKAGE_COMPILER_ABI;
    result.runtimeIdentity = xlCompiledPackageRuntimeIdentity(); result.name = name;
    result.packageVersion = "1"; result.abiVersion = "1";
    result.interfaceData = wire; result.interfaceSize = bytes;
    result.interfaceHash = xlCompiledPackageInterfaceHash(wire, bytes);
    assert(xl6CompiledPackageInterfaceValid(wire, bytes));
    return result;
}

static void wireTests(void)
{
    assert(xrtMemDebugEnable(true)); xrtClearError();
    xmemdebugsnapshot initial; xrtMemDebugSnapshot(&initial);
    const char* caps[] = {"GPU3D", "DEBUG"}; const char* deps[] = {"provider"};
    XL_CompiledPackage child = makePackage("provider", NULL, 0, caps, 2, NULL, 0, NULL, 0, 0);
    XL_CompiledPackage parent = makePackage("consumer", caps, 2, NULL, 0, deps, 1, deps, 1, 0);
    const XL_CompiledPackage* links[] = {&child}; parent.dependencies = links; parent.dependencyCount = 1;
    assert(xl6CompiledPackageDescriptorInterfaceValid(&parent));
    xmemdebugsnapshot baseline; xrtMemDebugSnapshot(&baseline);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6CompiledPackageDescriptorInterfaceValid(&parent));
    assert(xl6CompiledPackageInterfaceValid(child.interfaceData, child.interfaceSize));
    xrtMemDebugFailClear(); balanced(&baseline); /* Wire/identity validation allocates nothing. */
    XL6_CompiledNativeFeatures decoded = {0};
    assert(xl6CompiledPackageFeaturesDecode(&parent, &decoded));
    assert(!strcmp(decoded.name, "consumer") && decoded.required.count == 2 &&
        !strcmp(decoded.required.items[0], "DEBUG") && !strcmp(decoded.required.items[1], "GPU3D"));
    assert(decoded.dependencies.count == 1 && !strcmp(decoded.dependencies.items[0], "provider"));
    unsigned char *first = NULL, *second = NULL; size_t size = 0, size2 = 0;
    assert(xl6CompiledNativeFeaturesEncode(&decoded, &first, &size));
    char* swap = decoded.required.items[0]; decoded.required.items[0] = decoded.required.items[1]; decoded.required.items[1] = swap;
    assert(xl6CompiledNativeFeaturesEncode(&decoded, &second, &size2));
    assert(size == size2 && !memcmp(first, second, size)); /* Input order does not alter wire identity. */
    xrtFree(first); xrtFree(second); xl6CompiledNativeFeaturesFree(&decoded); balanced(&baseline);
    XL_CompiledPackage wrong = parent;
    wrong.name = "other"; assert(!xl6CompiledPackageDescriptorInterfaceValid(&wrong));
    wrong = parent; wrong.packageVersion = "2"; assert(!xl6CompiledPackageDescriptorInterfaceValid(&wrong));
    wrong = parent; wrong.abiVersion = "2"; assert(!xl6CompiledPackageDescriptorInterfaceValid(&wrong));
    wrong = parent; wrong.dependencyCount = 0; assert(!xl6CompiledPackageDescriptorInterfaceValid(&wrong));
    wrong = parent; const XL_CompiledPackage* nullLink[] = {NULL}; wrong.dependencies = nullLink;
    assert(!xl6CompiledPackageDescriptorInterfaceValid(&wrong));
    for (size_t n = 0; n < parent.interfaceSize; ++n)
        assert(!xl6CompiledPackageInterfaceValid(parent.interfaceData, n));
    unsigned char* mutable = (unsigned char*)parent.interfaceData;
    for (size_t i = 0; i < 40; ++i) {
        if (i >= 20 && i < 24) continue; /* Child count is checked by complete-tree truncation above. */
        unsigned char save = mutable[i]; mutable[i] ^= 128;
        assert(!xl6CompiledPackageInterfaceValid(mutable, parent.interfaceSize)); mutable[i] = save;
    }
    unsigned char save = mutable[40]; mutable[40] = 'Q';
    assert(!xl6CompiledPackageInterfaceValid(mutable, parent.interfaceSize)); mutable[40] = save;
    /* Legacy interfaces remain usable, but never inherit a feature claim. */
    XL_CompiledPackage legacy = makePackage("old", NULL, 0, NULL, 0, NULL, 0, NULL, 0, 1);
    assert(xl6CompiledPackageFeaturesDecode(&legacy, &decoded) && !decoded.required.count && !decoded.provided.count);
    xl6CompiledNativeFeaturesFree(&decoded);
    xrtFree((void*)legacy.interfaceData); balanced(&baseline);
    int failures = 0, successes = 0;
    for (int prefix = 0; prefix < 96; ++prefix) {
        assert(xrtMemDebugFailAfter(prefix));
        int okay = xl6CompiledPackageFeaturesDecode(&parent, &decoded);
        xrtMemDebugFailClear(); failures += !okay; successes += okay;
        xl6CompiledNativeFeaturesFree(&decoded); xrtClearError(); balanced(&baseline);
    }
    assert(failures && successes); failures = successes = 0;
    for (int prefix = 0; prefix < 96; ++prefix) {
        assert(xrtMemDebugFailAfter(prefix));
        XL6_AstNode* root = xl6CompiledPackageDecode(parent.interfaceData, parent.interfaceSize);
        xrtMemDebugFailClear(); failures += !root; successes += root != NULL;
        xl6AstFree(root); xrtClearError(); balanced(&baseline);
    }
    assert(failures && successes);
    XL6_CompiledNativeFeatures input = {0}; input.name = "encoding"; input.version = input.abiVersion = "1";
    input.required = (XL6_PackageStringList){(char**)caps, 2};
    for (int prefix = 0; prefix < 4; ++prefix) {
        unsigned char* output = (unsigned char*)1; size_t length = 987;
        assert(xrtMemDebugFailAfter(prefix));
        int okay = xl6CompiledNativeFeaturesEncode(&input, &output, &length);
        xrtMemDebugFailClear(); assert(okay == (prefix >= 2));
        if (okay) xrtFree(output); else assert(output == (unsigned char*)1 && length == 987);
        xrtClearError(); balanced(&baseline);
    }
    const char* invalid[] = {"GPU3D", "GPU3D"}; input.required.items = (char**)invalid;
    assert(!xl6CompiledNativeFeaturesEncode(&input, &first, &size)); balanced(&baseline);
    invalid[1] = "GPU 3D"; assert(!xl6CompiledNativeFeaturesEncode(&input, &first, &size)); balanced(&baseline);
    xrtFree((void*)child.interfaceData); xrtFree((void*)parent.interfaceData); balanced(&initial);
    puts("Compiled native feature wire: canonical identity, strict bounds, legacy isolation and OOM rollback passed");
}

int main(int argc, char** argv)
{
    if (argc == 1) { wireTests(); return 0; }
    assert(argc == 6);
    const char* mode = argv[4]; int expected = atoi(argv[3]);
    const char* capability[] = {"GPU3D"}; const char* providerName[] = {"provider"}; const char* bridgeName[] = {"bridge"};
    int absent = !strcmp(mode, "missing"); int legacy = !strcmp(mode, "legacy-provider");
    int bridgeDeclared = strcmp(mode, "bridge-source-import-only") != 0;
    int consumerDeclared = strcmp(mode, "consumer-source-import-only") != 0;
    XL_CompiledPackage packages[3];
    packages[0] = makePackage("provider", NULL, 0, capability, absent ? 0 : 1, NULL, 0, NULL, 0, legacy);
    packages[1] = makePackage("bridge", NULL, 0, NULL, 0, providerName, bridgeDeclared ? 1 : 0, providerName, 1, 0);
    packages[2] = makePackage("consumer", capability, 1, NULL, 0, bridgeName, consumerDeclared ? 1 : 0, bridgeName, 1, 0);
    const XL_CompiledPackage* toProvider[] = {&packages[0]};
    const XL_CompiledPackage* toBridge[] = {&packages[1]};
    packages[1].dependencies = toProvider; packages[1].dependencyCount = 1;
    packages[2].dependencies = toBridge; packages[2].dependencyCount = 1;
    XL6_CompiledPackageSet set = {packages, !strcmp(argv[5], "source-consumer") ? 2 : 3};
    XL6_ParseResult parse; XL6_ImportDeps imports;
    xl6ParseResultInit(&parse); xl6ImportDepsInit(&imports); imports.nativeSelection = atoi(argv[2]); imports.compiledPackages = &set;
    int okay = xl6ParseFile(argv[1], &parse) && xl6ImportsExpandModule(parse.root, argv[1], &parse.diags, &imports) && !parse.diags.count;
    for (size_t i = 0; i < parse.diags.count; ++i) printf("%s\n", parse.diags.items[i].message);
    assert(okay == expected && !imports.nativeFeatureDepth);
    xl6ImportDepsFree(&imports); xl6ParseResultFree(&parse);
    for (size_t i = 0; i < 3; ++i) xrtFree((void*)packages[i].interfaceData);
    puts("Real compiled/source import capability decision passed"); return 0;
}
