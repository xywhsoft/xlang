#include "include/xlc.h"
#include "include/xl_package.h"
#include "include/xrt_decl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static XL_CompiledPackage readPackage(const char* directory, const char* file, const char* name)
{
    char path[4096]; snprintf(path, sizeof(path), "%s/%s", directory, file);
    FILE* input = fopen(path, "rb"); assert(input);
    assert(!fseek(input, 0, SEEK_END)); long bytes = ftell(input);
    assert(bytes > 0 && (unsigned long)bytes <= XL_COMPILED_PACKAGE_MAX_INTERFACE); rewind(input);
    unsigned char* data = (unsigned char*)malloc((size_t)bytes); assert(data);
    assert(fread(data, 1, (size_t)bytes, input) == (size_t)bytes); assert(!fclose(input));
    XL_CompiledPackage result = {0}; result.size = sizeof(result); result.version = XL_COMPILED_PACKAGE_VERSION;
    result.pointerBytes = sizeof(void*); result.compilerAbi = XL_COMPILED_PACKAGE_COMPILER_ABI;
    result.runtimeIdentity = xlCompiledPackageRuntimeIdentity(); result.name = name;
    result.packageVersion = result.abiVersion = "1"; result.interfaceData = data; result.interfaceSize = (size_t)bytes;
    result.interfaceHash = xlCompiledPackageInterfaceHash(data, (size_t)bytes); return result;
}

static void clear(XL_Context* context)
{
    xlClearLastError(context); xrtClearError();
}

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
}

static void registration(const XL_ContextOptions* options, const char* directory)
{
    assert(xrtMemDebugEnable(true));
    XL_Context* anchor = xlCreateContext(options); assert(anchor);
    XL_CompiledPackage provider = readPackage(directory, "provider.xlp", "provider");
    XL_CompiledPackage missing = readPackage(directory, "missing.xlp", "provider");
    XL_CompiledPackage legacy = readPackage(directory, "legacy.xlp", "provider");
    XL_CompiledPackage consumer = readPackage(directory, "consumer.xlp", "consumer");
    const XL_CompiledPackage* dependencies[] = {&provider}; consumer.dependencies = dependencies; consumer.dependencyCount = 1;
    XL_Context* context = xlCreateContext(options); assert(context);
    XL_CompiledPackage wrong = consumer; wrong.name = "other";
    assert(!xlImportCompiledPackage(context, &wrong)); clear(context);
    assert(!xlFindCompiledPackage(context, "consumer") && !xlFindCompiledPackage(context, "provider"));
    wrong = consumer; wrong.packageVersion = "2"; assert(!xlImportCompiledPackage(context, &wrong)); clear(context);
    wrong = consumer; wrong.dependencyCount = 0; assert(!xlImportCompiledPackage(context, &wrong)); clear(context);
    assert(xlImportCompiledPackage(context, &consumer) && xlImportCompiledPackage(context, &consumer));
    assert(xlRunScriptString(context, "import consumer\n"));
    xlDestroyContext(context);
    for (unsigned i = 0; i < 2; ++i) {
        dependencies[0] = i ? &legacy : &missing;
        context = xlCreateContext(options); assert(context);
        assert(xlImportCompiledPackage(context, &consumer));
        assert(!xlRunScriptString(context, "import consumer\n"));
        assert(strstr(xlGetLastError(context), "requires native feature GPU3D"));
        clear(context); xlDestroyContext(context);
    }
    dependencies[0] = &provider;
    int hit; size_t prefix;
    for (prefix = 0; prefix < 512; ++prefix) {
        context = xlCreateContext(options); assert(context);
        clear(context); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(prefix));
        int okay = xlImportCompiledPackage(context, &consumer);
        hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); clear(context);
        if (!okay) {
            assert(!xlFindCompiledPackage(context, "consumer") && !xlFindCompiledPackage(context, "provider"));
            balanced(&before); assert(xlImportCompiledPackage(context, &consumer));
        } else assert(!hit);
        xlDestroyContext(context); if (!hit) break;
    }
    assert(prefix < 512);
    free((void*)provider.interfaceData); free((void*)missing.interfaceData);
    free((void*)legacy.interfaceData); free((void*)consumer.interfaceData);
    xlDestroyContext(anchor);
    printf("Public API: selected feature snapshot, old-wire isolation, descriptor rejection and %zu OOM prefixes passed\n", prefix + 1);
}

static void reuse(const XL_ContextOptions* options, const char* directory)
{
    char binary[4096]; snprintf(binary, sizeof(binary), "%s/reused-auto-rejected.bin", directory);
    XLC_Context* context = xlcCreateContext(options); assert(context);
    assert(xlcAddSourceString(context, "feature-reuse.xl", "import sourceConsumer\nassert sourceConsumer.answer==42\n"));
    assert(xlcCompile(context)); /* AUTO JIT selects the enabled dynamic variant. */
    assert(!xlcEmitExe(context, binary)); /* AUTO AOT must recompute, and reject disabled static. */
    assert(strstr(xlcGetLastError(context), "requires native feature GPU3D"));
    assert(xlcCompile(context));
    assert(xlcSetNativeLinkage(context, XLC_NATIVE_LINKAGE_STATIC));
    assert(!xlcCompile(context)); assert(strstr(xlcGetLastError(context), "requires native feature GPU3D"));
    assert(xlcSetNativeLinkage(context, XLC_NATIVE_LINKAGE_DYNAMIC));
    assert(xlcCompile(context));
    snprintf(binary, sizeof(binary), "%s/reused-dynamic.bin", directory);
    assert(xlcEmitExe(context, binary));
    assert(xlcSetNativeLinkage(context, XLC_NATIVE_LINKAGE_AUTO));
    assert(xlcCompile(context));
    snprintf(binary, sizeof(binary), "%s/reused-auto-rejected-again.bin", directory);
    assert(!xlcEmitExe(context, binary)); assert(strstr(xlcGetLastError(context), "requires native feature GPU3D"));
    xlcDestroyContext(context);
    puts("Public API: same compiler context recomputes native capability selection across JIT/AOT and explicit linkage");
}

int main(int argc, char** argv)
{
    assert(argc == 4);
    XL_ContextOptions options = xlDefaultContextOptions(); options.workDir = ".";
    options.tccDllPath = argv[1]; options.tccLibPath = argv[2];
    registration(&options, argv[3]); reuse(&options, argv[3]); return 0;
}
