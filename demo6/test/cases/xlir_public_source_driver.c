/* Link public API and compiler implementation from source, without including
 * xl_api.c, replacing lookup hooks or pre-generating the program under test. */
#include "include/xlc.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
static int recovery(XLC_Context* compiler, const char* prefix)
{
    char path[2400], text[16] = {0}; FILE* file; unsigned round;
    snprintf(path, sizeof(path), "%s.guard.c", prefix);
    for (round = 0; round < 10; ++round) {
        if (!xlcAddSourceString(compiler, "__public_recovery.xl", "assert 6 * 7 == 42\n") ||
            !xlcCompile(compiler) || !xlcRun(compiler, 0, NULL) || xlcGetLastExitCode(compiler) != 0) return 0;
        file = fopen(path, "wb"); if (!file) return 0;
        if (fwrite("KEEP", 1, 4, file) != 4) { fclose(file); return 0; } fclose(file);
        if (!xlcAddSourceString(compiler, "__public_recovery.xl", "int broken = missingPublicName\n") ||
            xlcCompile(compiler) || !xlcGetLastError(compiler) || !*xlcGetLastError(compiler) ||
            xlcEmitC(compiler, path)) return 0;
        file = fopen(path, "rb"); if (!file) return 0;
        if (fread(text, 1, sizeof(text), file) != 4 || memcmp(text, "KEEP", 4)) { fclose(file); return 0; } fclose(file);
        if (!xlcAddSourceString(compiler, "__public_recovery.xl", "assert false\n") ||
            !xlcCompile(compiler) || xlcRun(compiler, 0, NULL) || !xlcGetLastError(compiler)) return 0;
        if (!xlcAddSourceString(compiler, "__public_recovery.xl", "func twice(int value) as int\n return value * 2\nend\nassert twice(21) == 42\n") ||
            !xlcCompile(compiler) || !xlcRun(compiler, 0, NULL) || xlcGetLastExitCode(compiler) != 0 ||
            (xlcGetLastError(compiler) && *xlcGetLastError(compiler))) return 0;
    }
    printf("public recovery: 10 rounds, compile failure preserves output, runtime failure, source/cache replacement and successful reuse\n");
    return 1;
}
int main(int argc, char** argv)
{
    XLC_ContextOptions options = xlcDefaultContextOptions();
    XLC_Context* compiler;
    char root[2048], runtime[2200], libraries[2200], path[2400], headers[2400], temporary[2400];
    char* slash;
    const char* phase = "create";
    int ok = 0, code = 1, index;
    if (argc < 4) { fprintf(stderr, "usage: source-driver source.xl emit|run|module|exe|dll output-prefix [sidecar.c ...]\n"); return 2; }
    if (!GetCurrentDirectoryA(sizeof(root), root)) return 3;
    snprintf(runtime, sizeof(runtime), "%s/release/xl.dll", root);
    snprintf(libraries, sizeof(libraries), "%s/res/tcc/win64/x86_64", root);
    /* A batch may execute independent public drivers concurrently. Each
     * invocation owns its artifact prefix and the adjacent native/TCC temps. */
    if (snprintf(temporary, sizeof(temporary), "%s.temps", argv[3]) >= (int)sizeof(temporary)) return 3;
    options.workDir = root; options.tempDir = temporary;
    options.tccDllPath = runtime; options.tccLibPath = libraries;
    compiler = xlcCreateContext(&options);
    if (!compiler) { fprintf(stderr, "public source context creation failed\n"); return 4; }
    phase = "configure private runtime headers";
    if (!GetModuleFileNameA(NULL, headers, sizeof(headers))) goto done;
    slash = strrchr(headers, '\\'); if (!slash) goto done;
    if ((size_t)(slash - headers) + sizeof("\\include") > sizeof(headers)) goto done;
    memcpy(slash, "\\include", sizeof("\\include"));
    if (!xlcAddIncludePath(compiler, headers)) goto done;
    if (!strcmp(argv[2], "recovery")) { phase = "recovery"; ok = recovery(compiler, argv[3]); code = ok ? 0 : 1; goto done; }
    /* Deliberately exercise the actual default backend, not an override. */
    phase = "load"; if (!xlcAddSourceFile(compiler, argv[1])) goto done;
    phase = "load C sidecar";
    for (index = 4; index < argc; ++index)
        if (!xlcAddCSourceFile(compiler, argv[index])) goto done;
    phase = "compile"; if (!xlcCompile(compiler)) goto done;
    if (!strcmp(argv[2], "emit")) {
        phase = "emit C"; snprintf(path, sizeof(path), "%s.c", argv[3]);
        if (!xlcEmitC(compiler, path)) goto done;
        phase = "emit header"; snprintf(path, sizeof(path), "%s.h", argv[3]);
        if (!xlcEmitHeader(compiler, path, "XLIR_PUBLIC_SOURCE")) goto done;
        phase = "emit lowered IR"; snprintf(path, sizeof(path), "%s.json", argv[3]);
        ok = xlcEmitLoweredIr(compiler, path, 1);
    } else if (!strcmp(argv[2], "exe")) {
        phase = "emit exe"; snprintf(path, sizeof(path), "%s.exe", argv[3]);
        ok = xlcEmitExe(compiler, path);
    } else if (!strcmp(argv[2], "dll")) {
        phase = "emit dll"; snprintf(path, sizeof(path), "%s.dll", argv[3]);
        ok = xlcEmitSharedLibrary(compiler, path);
    } else if (!strcmp(argv[2], "run")) {
        phase = "run"; ok = xlcRun(compiler, 0, NULL);
    } else if (!strcmp(argv[2], "module")) {
        XL_Module* module;
        phase = "relocate"; module = xlcRelocateModule(compiler);
        if (!module) goto done;
        phase = "module main"; ok = xlRunModuleMain(xlcRuntimeContext(compiler), module, NULL);
        xlDestroyModule(module);
    } else { phase = "unknown mode"; goto done; }
    code = ok ? xlcGetLastExitCode(compiler) : 1;
done:
    if (!ok) fprintf(stderr, "public source %s failed: %s\n", phase,
        xlcGetLastError(compiler) ? xlcGetLastError(compiler) : "without diagnostic");
    xlcDestroyContext(compiler);
    if (ok && code == 0) printf("public source %s complete\n", argv[2]);
    return code;
}
