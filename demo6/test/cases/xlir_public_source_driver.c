/* Link public API and compiler implementation from source, without including
 * xl_api.c, replacing lookup hooks or pre-generating the program under test. */
#include "include/xlc.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
static int recovery(XLC_Context* compiler, const char* prefix)
{
    char path[2400], text[16] = {0}; FILE* file; unsigned round;
    int pathLength = snprintf(path, sizeof(path), "%s.guard.c", prefix);
    if (pathLength < 0 || (size_t)pathLength >= sizeof(path)) return 0;
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
    if (!xlcSetBackend(compiler, XLC_BACKEND_LEGACY)) return 0;
    file = fopen(path, "wb"); if (!file) return 0;
    if (fwrite("KEEP", 1, 4, file) != 4) { fclose(file); return 0; } fclose(file);
    if (!xlcAddSourceString(compiler, "__public_recovery.xl",
            "int tooBig = 999999999999999999999999999999999999999999999999999999999999999999\n") ||
        !xlcCompile(compiler) || xlcEmitC(compiler, path) ||
        !xlcGetLastError(compiler) || !*xlcGetLastError(compiler)) return 0;
    file = fopen(path, "rb"); if (!file) return 0;
    if (fread(text, 1, sizeof(text), file) != 4 || memcmp(text, "KEEP", 4)) { fclose(file); return 0; } fclose(file);
    if (!xlcSetBackend(compiler, XLC_BACKEND_XLIR) ||
        !xlcAddSourceString(compiler, "__public_recovery.xl", "assert 6 * 7 == 42\n") ||
        !xlcCompile(compiler) || !xlcRun(compiler, 0, NULL) || xlcGetLastExitCode(compiler) != 0) return 0;
    printf("public recovery: 10 rounds, compile/Cgen failure preserves output, runtime failure, source/cache/backend replacement and successful reuse\n");
    return 1;
}
int main(int argc, char** argv)
{
    XLC_ContextOptions options = xlcDefaultContextOptions();
    XLC_Context* compiler;
    char root[2048], runtime[2200], libraries[2200], path[2400], headers[2400], temporary[2400];
    char* slash;
    const char* requestedMode;
    const char* phase = "create";
    DWORD rootLength, moduleLength;
    int exactCgen = 0, ok = 0, code = 1, index, pathLength;
    if (argc < 4) {
        fprintf(stderr, "usage: source-driver source.xl emit|ir-typed|ir-lowered|run|module|exe|dll|recovery|cgen-default|cgen-xlir|cgen-legacy output [sidecar.c ...]\n");
        return 2;
    }
    requestedMode = argv[2];
    if (strcmp(requestedMode, "emit") && strcmp(requestedMode, "ir-typed") &&
        strcmp(requestedMode, "ir-lowered") && strcmp(requestedMode, "run") &&
        strcmp(requestedMode, "module") && strcmp(requestedMode, "exe") &&
        strcmp(requestedMode, "dll") && strcmp(requestedMode, "recovery") &&
        strcmp(requestedMode, "cgen-default") && strcmp(requestedMode, "cgen-xlir") &&
        strcmp(requestedMode, "cgen-legacy")) {
        fprintf(stderr, "public source unknown mode: %s\n", requestedMode);
        return 2;
    }
    rootLength = GetCurrentDirectoryA((DWORD)sizeof(root), root);
    if (!rootLength || rootLength >= sizeof(root)) return 3;
    pathLength = snprintf(runtime, sizeof(runtime), "%s/release/xl.dll", root);
    if (pathLength < 0 || (size_t)pathLength >= sizeof(runtime)) return 3;
    pathLength = snprintf(libraries, sizeof(libraries), "%s/res/tcc/win64/x86_64", root);
    if (pathLength < 0 || (size_t)pathLength >= sizeof(libraries)) return 3;
    /* A batch may execute independent public drivers concurrently. Each
     * invocation owns its artifact prefix and the adjacent native/TCC temps. */
    pathLength = snprintf(temporary, sizeof(temporary), "%s.temps", argv[3]);
    if (pathLength < 0 || (size_t)pathLength >= sizeof(temporary)) return 3;
    options.workDir = root; options.tempDir = temporary;
    options.tccDllPath = runtime; options.tccLibPath = libraries;
    compiler = xlcCreateContext(&options);
    if (!compiler) { fprintf(stderr, "public source context creation failed\n"); return 4; }
    /* cgen-default deliberately leaves the context's source-defined default
     * untouched; the explicit modes are the differential controls. */
    if (!strcmp(requestedMode, "cgen-default")) {
        exactCgen = 1;
    } else if (!strcmp(requestedMode, "cgen-xlir")) {
        phase = "select XLIR backend";
        if (!xlcSetBackend(compiler, XLC_BACKEND_XLIR)) goto done;
        exactCgen = 1;
    } else if (!strcmp(requestedMode, "cgen-legacy")) {
        phase = "select legacy backend";
        if (!xlcSetBackend(compiler, XLC_BACKEND_LEGACY)) goto done;
        exactCgen = 1;
    }
    phase = "configure private runtime headers";
    moduleLength = GetModuleFileNameA(NULL, headers, (DWORD)sizeof(headers));
    if (!moduleLength || moduleLength >= sizeof(headers)) goto done;
    slash = strrchr(headers, '\\'); if (!slash) goto done;
    if ((size_t)(slash - headers) + sizeof("\\include") > sizeof(headers)) goto done;
    memcpy(slash, "\\include", sizeof("\\include"));
    if (!xlcAddIncludePath(compiler, headers)) goto done;
    if (!strcmp(requestedMode, "recovery")) { phase = "recovery"; ok = recovery(compiler, argv[3]); code = ok ? 0 : 1; goto done; }
    phase = "load"; if (!xlcAddSourceFile(compiler, argv[1])) goto done;
    phase = "load C sidecar";
    for (index = 4; index < argc; ++index)
        if (!xlcAddCSourceFile(compiler, argv[index])) goto done;
    phase = "compile"; if (!xlcCompile(compiler)) goto done;
    if (exactCgen) {
        phase = "emit exact C";
        ok = xlcEmitC(compiler, argv[3]);
    } else if (!strcmp(requestedMode, "ir-typed")) {
        phase = "emit typed IR";
        ok = xlcEmitIr(compiler, argv[3], 1);
    } else if (!strcmp(requestedMode, "ir-lowered")) {
        phase = "emit lowered IR";
        ok = xlcEmitLoweredIr(compiler, argv[3], 1);
    } else if (!strcmp(requestedMode, "emit")) {
        phase = "emit C"; pathLength = snprintf(path, sizeof(path), "%s.c", argv[3]);
        if (pathLength < 0 || (size_t)pathLength >= sizeof(path)) goto done;
        if (!xlcEmitC(compiler, path)) goto done;
        phase = "emit header"; pathLength = snprintf(path, sizeof(path), "%s.h", argv[3]);
        if (pathLength < 0 || (size_t)pathLength >= sizeof(path)) goto done;
        if (!xlcEmitHeader(compiler, path, "XLIR_PUBLIC_SOURCE")) goto done;
        phase = "emit lowered IR"; pathLength = snprintf(path, sizeof(path), "%s.json", argv[3]);
        if (pathLength < 0 || (size_t)pathLength >= sizeof(path)) goto done;
        ok = xlcEmitLoweredIr(compiler, path, 1);
    } else if (!strcmp(requestedMode, "exe")) {
        phase = "emit exe"; pathLength = snprintf(path, sizeof(path), "%s.exe", argv[3]);
        if (pathLength < 0 || (size_t)pathLength >= sizeof(path)) goto done;
        ok = xlcEmitExe(compiler, path);
    } else if (!strcmp(requestedMode, "dll")) {
        phase = "emit dll"; pathLength = snprintf(path, sizeof(path), "%s.dll", argv[3]);
        if (pathLength < 0 || (size_t)pathLength >= sizeof(path)) goto done;
        ok = xlcEmitSharedLibrary(compiler, path);
    } else if (!strcmp(requestedMode, "run")) {
        phase = "run"; ok = xlcRun(compiler, 0, NULL);
    } else if (!strcmp(requestedMode, "module")) {
        XL_Module* module;
        phase = "relocate"; module = xlcRelocateModule(compiler);
        if (!module) goto done;
        phase = "module main"; ok = xlRunModuleMain(xlcRuntimeContext(compiler), module, NULL);
        xlDestroyModule(module);
    }
    code = ok ? xlcGetLastExitCode(compiler) : 1;
done:
    if (!ok) fprintf(stderr, "public source %s failed: %s\n", phase,
        xlcGetLastError(compiler) ? xlcGetLastError(compiler) : "without diagnostic");
    xlcDestroyContext(compiler);
    if (ok && code == 0) printf("public source %s complete\n", requestedMode);
    return code;
}
