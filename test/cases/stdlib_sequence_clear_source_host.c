/* Current full-source public compiler, genuine TCC and original callback
 * probe. This is not an embedded-VFS shipping product or a provider override. */
#include "xlc.h"
#include "xruntime_decl.h"
#include <assert.h>
#include <stdio.h>

void clearProbeInstallV1(xvalue*);
int64_t clearProbeDropsV1(void);
void clearProbeFinishV1(void);

static void transaction(const char* headers, const char* runtime, const char* libraries,
    const char* source, const char* temporary)
{
    XLC_ContextOptions options = xlcDefaultContextOptions();
    options.workDir = "."; options.tempDir = temporary;
    options.tccDllPath = runtime; options.tccLibPath = libraries;
    XLC_Context* compiler = xlcCreateContext(&options); assert(compiler);
    assert(xlcAddIncludePath(compiler, headers));
    assert(xlcRegisterCFunction(compiler, "clearProbeInstallV1", (void*)clearProbeInstallV1));
    assert(xlcRegisterCFunction(compiler, "clearProbeDropsV1", (void*)clearProbeDropsV1));
    assert(xlcRegisterCFunction(compiler, "clearProbeFinishV1", (void*)clearProbeFinishV1));
    assert(xlcAddSourceFile(compiler, source));
    if (!xlcCompile(compiler)) { fprintf(stderr, "clear source compile: %s\n", xlcGetLastError(compiler)); assert(false); }
    XL_Module* module = xlcRelocateModule(compiler); assert(module);
    XL_Context* context = xlcRuntimeContext(compiler);
    int ok = xlRunModuleMain(context, module, NULL);
    if (!ok) fprintf(stderr, "clear source main: %s\n", xlcGetLastError(compiler));
    assert(ok && xlcGetLastExitCode(compiler) == 0 && !xrtGetError());
    xlDestroyModule(module); xlcDestroyContext(compiler); xlRuntimeUnit();
    assert(!xrtGetError());
}

int main(int argc, char** argv)
{
    assert(argc == 6 && xrtMemDebugEnable(true));
    transaction(argv[1], argv[2], argv[3], argv[4], argv[5]);
    transaction(argv[1], argv[2], argv[3], argv[4], argv[5]);
    for (unsigned round = 0; round < 25; ++round) {
        xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
        transaction(argv[1], argv[2], argv[3], argv[4], argv[5]); xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
            fprintf(stderr, "clear source round=%u live=%llu->%llu bytes=%llu->%llu\n", round,
                (unsigned long long)before.LiveCount, (unsigned long long)after.LiveCount,
                (unsigned long long)before.LiveBytes, (unsigned long long)after.LiveBytes);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
            before.InvalidFreeCount == after.InvalidFreeCount &&
            before.DoubleFreeCount == after.DoubleFreeCount && before.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    puts("clear source: 25 complete language lifecycles, first error and async/generator cleanup, memory balanced");
    return 0;
}
