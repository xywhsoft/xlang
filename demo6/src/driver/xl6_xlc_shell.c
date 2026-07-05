#include "include/xlc.h"
#include "lib/xrt/xrt.h"
#include "src/common/xl6_console.h"
#include "src/debug/xl6_dap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

static char xlcShellKeptTempDir[512];

static void xlcShellPrintHelp(void)
{
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "Usage:\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl lex <file.xl> [--format text|json] [--emit output.tokens]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl parse <file.xl> [--format tree|json] [--emit output.ast]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl check <file.xl> [--json]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl ir <file.xl> [--format text|json] [--emit output.xlir]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl cgen <file.xl> [--emit output.c] [--emit-header output.h] [--dump-dir dir]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl abi <file.xl> [--json] [--emit output.abi.txt]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl xapi <file.xl> [--emit output.xapi.json]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl run <file.xl|file.c> [helper.c ...] [--save-temps] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll] [-- <args>...]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl dbg <file.xl> [--break line] [--stop-on-entry] [--commands file] [--save-temps] [-- <args>...]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl dap\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl build <file.xl|file.c> [helper.c ...] -o <output.exe|dll> [-shared] [--save-temps] [--emit-header output.h] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl impdef <file.dll> -o <output.def>\n");
}

static const char* xlcShellFileArg(int argc, char** argv)
{
    return argc >= 3 ? argv[2] : NULL;
}

static int xlcShellTextEquals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static int xlcShellHasJson(int argc, char** argv)
{
    int i;

    for ( i = 3; i < argc; ++i ) {
        if ( xlcShellTextEquals(argv[i], "--json") ) {
            return 1;
        }
        if ( xlcShellTextEquals(argv[i], "--format") && i + 1 < argc && xlcShellTextEquals(argv[i + 1], "json") ) {
            return 1;
        }
    }
    return 0;
}

static int xlcShellHasFlag(int argc, char** argv, const char* flag)
{
    int i;

    for ( i = 3; i < argc; ++i ) {
        if ( xlcShellTextEquals(argv[i], "--") ) {
            break;
        }
        if ( xlcShellTextEquals(argv[i], flag) ) {
            return 1;
        }
    }
    return 0;
}

static const char* xlcShellFindValue(int argc, char** argv, const char* option, int start)
{
    int i;

    for ( i = start; i < argc; ++i ) {
        if ( xlcShellTextEquals(argv[i], "--") ) {
            break;
        }
        if ( xlcShellTextEquals(argv[i], option) && i + 1 < argc ) {
            return argv[i + 1];
        }
    }
    return NULL;
}

static int xlcShellFindArgSplit(int argc, char** argv)
{
    int i;

    for ( i = 3; i < argc; ++i ) {
        if ( xlcShellTextEquals(argv[i], "--") ) {
            return i + 1;
        }
    }
    return argc;
}

static int xlcShellMakeDir(const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    return xrtDirCreateAll((str)path) != 0;
}

static int xlcShellJoinDirPath(char* out, size_t outSize, const char* dir, const char* name)
{
    const char* sep;
    size_t len;
    int written;

    if ( out == NULL || outSize == 0 || dir == NULL || name == NULL ) {
        return 0;
    }
    len = strlen(dir);
    sep = (len > 0 && (dir[len - 1u] == '\\' || dir[len - 1u] == '/')) ? "" : "\\";
    written = snprintf(out, outSize, "%s%s%s", dir, sep, name);
    return written > 0 && (size_t)written < outSize;
}

static int xlcShellBuildTmpDir(char* out, size_t outSize)
{
    int written;

    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    written = snprintf(out, outSize, "build\\tmp");
    if ( written <= 0 || (size_t)written >= outSize ) {
        return 0;
    }
    return xlcShellMakeDir(out);
}

static int xlcShellDefaultTempDir(char* out, size_t outSize)
{
    char tmpDir[512];

    if ( !xlcShellBuildTmpDir(tmpDir, sizeof(tmpDir)) ) {
        return 0;
    }
    return xlcShellJoinDirPath(out, outSize, tmpDir, "xlc_shell");
}

static unsigned long xlcShellProcessId(void)
{
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

static unsigned long xlcShellTick(void)
{
    return (unsigned long)xrtNow();
}

static int xlcShellMakeTaggedTempDir(char* out, size_t outSize, const char* tag)
{
    unsigned long pid;
    unsigned long tick;
    char tmpDir[512];
    int i;

    if ( out == NULL || outSize == 0 || tag == NULL ) {
        return 0;
    }
    if ( !xlcShellBuildTmpDir(tmpDir, sizeof(tmpDir)) ) {
        return 0;
    }
    pid = xlcShellProcessId();
    tick = xlcShellTick();
    for ( i = 0; i < 100; ++i ) {
        int written = snprintf(out, outSize, "%s\\xl6_%s_%lu_%lu_%d", tmpDir, tag, pid, tick, i);
        if ( written <= 0 || (size_t)written >= outSize ) {
            return 0;
        }
        if ( xlcShellMakeDir(out) ) {
            return 1;
        }
    }
    return 0;
}

static const char* xlcShellBaseName(const char* path)
{
    const char* lastSlash;
    const char* lastBackslash;

    if ( path == NULL ) {
        return "";
    }
    lastSlash = strrchr(path, '/');
    lastBackslash = strrchr(path, '\\');
    if ( lastBackslash != NULL && (lastSlash == NULL || lastBackslash > lastSlash) ) {
        lastSlash = lastBackslash;
    }
    return lastSlash != NULL ? lastSlash + 1 : path;
}

static char xlcShellHeaderGuardChar(char ch)
{
    if ( ch >= 'a' && ch <= 'z' ) {
        return (char)(ch - 'a' + 'A');
    }
    if ( (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ) {
        return ch;
    }
    return '_';
}

static void xlcShellMakeHeaderGuard(char* out, size_t outSize, const char* path)
{
    const char* base;
    const char* p;
    const char* dot;
    size_t used;

    if ( out == NULL || outSize == 0 ) {
        return;
    }
    snprintf(out, outSize, "XL6_GENERATED_HEADER_H");
    if ( path == NULL || path[0] == '\0' ) {
        return;
    }
    base = xlcShellBaseName(path);
    dot = strrchr(base, '.');
    if ( dot == base || base[0] == '\0' ) {
        return;
    }
    snprintf(out, outSize, "XL6_");
    used = strlen(out);
    for ( p = base; *p != '\0' && p != dot && used + 3 < outSize; ++p ) {
        out[used++] = xlcShellHeaderGuardChar(*p);
    }
    if ( used + 3 < outSize ) {
        out[used++] = '_';
        out[used++] = 'H';
    }
    out[used] = '\0';
}

static int xlcShellEmitHeader(XLC_Context* ctx, const char* outputPath)
{
    char guard[256];

    xlcShellMakeHeaderGuard(guard, sizeof(guard), outputPath);
    return xlcEmitHeader(ctx, outputPath, guard);
}

static int xlcShellTextEqualsNoCase(const char* a, const char* b)
{
    unsigned char ca;
    unsigned char cb;

    if ( a == NULL || b == NULL ) {
        return 0;
    }
    while ( *a != '\0' && *b != '\0' ) {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if ( ca >= 'A' && ca <= 'Z' ) {
            ca = (unsigned char)(ca - 'A' + 'a');
        }
        if ( cb >= 'A' && cb <= 'Z' ) {
            cb = (unsigned char)(cb - 'A' + 'a');
        }
        if ( ca != cb ) {
            return 0;
        }
    }
    return *a == '\0' && *b == '\0';
}

static int xlcShellHasExtensionNoCase(const char* path, const char* ext)
{
    const char* base;
    const char* dot;

    if ( path == NULL || ext == NULL ) {
        return 0;
    }
    base = xlcShellBaseName(path);
    dot = strrchr(base, '.');
    return dot != NULL && xlcShellTextEqualsNoCase(dot, ext);
}

static int xlcShellIsCSourcePath(const char* path)
{
    return xlcShellHasExtensionNoCase(path, ".c") ||
           xlcShellHasExtensionNoCase(path, ".cc") ||
           xlcShellHasExtensionNoCase(path, ".cpp") ||
           xlcShellHasExtensionNoCase(path, ".cxx");
}

static int xlcShellOptionTakesValue(const char* arg);

#define XLC_SHELL_MAX_C_INPUTS 64

static int xlcShellCollectCInputs(int argc, char** argv, const char** inputs, int maxInputs, int* outCount)
{
    int i;
    int count;

    if ( inputs == NULL || outCount == NULL || maxInputs <= 0 ) {
        return 0;
    }
    count = 0;
    for ( i = 2; i < argc; ++i ) {
        const char* arg = argv[i];

        if ( xlcShellTextEquals(arg, "--") ) {
            break;
        }
        if ( xlcShellOptionTakesValue(arg) ) {
            i += 1;
            continue;
        }
        if ( arg == NULL || arg[0] == '-' ) {
            continue;
        }
        if ( xlcShellIsCSourcePath(arg) ) {
            if ( count >= maxInputs ) {
                xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many C source files\n");
                return 0;
            }
            inputs[count++] = arg;
        }
    }
    if ( count <= 0 ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "missing C source file\n");
        return 0;
    }
    *outCount = count;
    return 1;
}

static int xlcShellIsDefaultRuntimeLink(const char* value)
{
    const char* base;

    if ( value == NULL ) {
        return 0;
    }
    base = xlcShellBaseName(value);
    return xlcShellTextEqualsNoCase(value, "xrt") ||
           xlcShellTextEqualsNoCase(base, "xrt.o") ||
           xlcShellTextEqualsNoCase(base, "xrt.obj") ||
           xlcShellTextEqualsNoCase(base, "xrt.lib") ||
           xlcShellTextEqualsNoCase(base, "libxrt.a") ||
           xlcShellTextEqualsNoCase(base, "libtcc1.a") ||
           xlcShellTextEqualsNoCase(base, "libtcc1-64.a");
}

static void xlcShellBaseNoExt(char* out, size_t outSize, const char* path)
{
    const char* base;
    const char* dot;
    size_t len;

    if ( out == NULL || outSize == 0 ) {
        return;
    }
    base = xlcShellBaseName(path);
    dot = strrchr(base, '.');
    len = dot != NULL && dot > base ? (size_t)(dot - base) : strlen(base);
    if ( len >= outSize ) {
        len = outSize - 1;
    }
    memcpy(out, base, len);
    out[len] = '\0';
}

static int xlcShellJoinPath(char* out, size_t outSize, const char* dir, const char* base, const char* suffix)
{
    int written;

    if ( out == NULL || outSize == 0 || dir == NULL || base == NULL || suffix == NULL ) {
        return 0;
    }
    written = snprintf(out, outSize, "%s\\%s%s", dir, base, suffix);
    return written > 0 && (size_t)written < outSize;
}

static int xlcShellOptionTakesValue(const char* arg)
{
    return xlcShellTextEquals(arg, "--emit") ||
           xlcShellTextEquals(arg, "--emit-header") ||
           xlcShellTextEquals(arg, "--dump-dir") ||
           xlcShellTextEquals(arg, "--format") ||
           xlcShellTextEquals(arg, "--inc-path") ||
           xlcShellTextEquals(arg, "--lib-path") ||
           xlcShellTextEquals(arg, "--link") ||
           xlcShellTextEquals(arg, "--loadlib") ||
           xlcShellTextEquals(arg, "--break") ||
           xlcShellTextEquals(arg, "--commands") ||
           xlcShellTextEquals(arg, "-o");
}

static int xlcShellCollectBreakpoints(char* out, size_t outSize, int argc, char** argv)
{
    int i;
    int started;

    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    out[0] = '\0';
    started = 0;
    for ( i = 3; i < argc; ++i ) {
        if ( xlcShellTextEquals(argv[i], "--") ) {
            break;
        }
        if ( xlcShellTextEquals(argv[i], "--break") && i + 1 < argc ) {
            size_t used;
            size_t add;

            used = strlen(out);
            add = strlen(argv[i + 1]);
            if ( used + add + (started ? 2u : 1u) >= outSize ) {
                return 0;
            }
            if ( started ) {
                out[used++] = ',';
            }
            memcpy(out + used, argv[i + 1], add + 1u);
            started = 1;
            i += 1;
        } else if ( xlcShellOptionTakesValue(argv[i]) ) {
            i += 1;
        }
    }
    return 1;
}

static char* xlcShellReadTextFile(const char* path)
{
    size_t size;
    char* text;

    if ( path == NULL || path[0] == '\0' ) {
        return NULL;
    }
    size = 0;
    text = (char*)xrtFileReadAll((char*)path, XRT_CP_UTF8, &size);
    if ( text == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot read command file: %s\n", path);
        return NULL;
    }
    return text;
}

static char* xlcShellMakeEnvItem(const char* name, const char* value)
{
    char* text;
    size_t nameLen;
    size_t valueLen;

    if ( name == NULL || name[0] == '\0' ) {
        return NULL;
    }
    value = value != NULL ? value : "";
    nameLen = strlen(name);
    valueLen = strlen(value);
    text = (char*)malloc(nameLen + 1u + valueLen + 1u);
    if ( text == NULL ) {
        return NULL;
    }
    memcpy(text, name, nameLen);
    text[nameLen] = '=';
    memcpy(text + nameLen + 1u, value, valueLen + 1u);
    return text;
}

static int xlcShellRunProcess(const char* program, char** args, int argCount, char** env, int envCount)
{
    xprocessconfig config;
    xprocessresult result;
    const char* errorText;
    int exitCode;

    if ( program == NULL || program[0] == '\0' ) {
        return 1;
    }
    xrtProcessConfigInit(&config);
    config.iTargetKind = XPROC_TARGET_EXEC;
    config.sProgram = (char*)program;
    config.arrArgs = args != NULL ? (str*)args : NULL;
    config.iArgCount = args != NULL && argCount > 0 ? (uint32)argCount : 0u;
    config.arrEnv = env != NULL ? (str*)env : NULL;
    config.iEnvCount = env != NULL && envCount > 0 ? (uint32)envCount : 0u;
    config.bHideWindow = true;

    memset(&result, 0, sizeof(result));
    if ( !xrtExecCapture(&config, &result, 0u) ) {
        errorText = xrtGetError();
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "process spawn failed: %s\n", errorText != NULL && errorText[0] != '\0' ? errorText : program);
        xrtProcessResultUnit(&result);
        return 1;
    }
    if ( result.pStdout != NULL && result.iStdoutSize > 0u ) {
        (void)xl6ConsoleWrite(XL6_CONSOLE_STDOUT, (const char*)result.pStdout, result.iStdoutSize);
    }
    if ( result.pStderr != NULL && result.iStderrSize > 0u ) {
        (void)xl6ConsoleWrite(XL6_CONSOLE_STDERR, (const char*)result.pStderr, result.iStderrSize);
    }
    exitCode = (result.ExitInfo.iKind == XPROC_EXIT_NORMAL && !result.ExitInfo.bTimedOut) ? result.iExitCode : 1;
    xrtProcessResultUnit(&result);
    return exitCode;
}

static void xlcShellCleanupWorkDir(const char* workDir, const char* cPath, const char* exePath, int saveTemps)
{
    if ( workDir == NULL || workDir[0] == '\0' ) {
        return;
    }
    if ( saveTemps ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "temporary files kept: %s\n", workDir);
        return;
    }
    if ( cPath != NULL && cPath[0] != '\0' ) {
        (void)xrtFileDelete((str)cPath);
    }
    if ( exePath != NULL && exePath[0] != '\0' ) {
        (void)xrtFileDelete((str)exePath);
    }
    (void)xrtDirDelete((str)workDir);
}

static int xlcShellApplyContextOptions(XLC_Context* ctx, int argc, char** argv)
{
    int i;

    if ( ctx == NULL ) {
        return 0;
    }
    if ( xlcShellHasFlag(argc, argv, "--save-temps") ) {
        if ( !xlcSetSaveTemps(ctx, 1) ) {
            return 0;
        }
    }
    for ( i = 3; i < argc; ++i ) {
        if ( xlcShellTextEquals(argv[i], "--") ) {
            break;
        }
        if ( i + 1 < argc && xlcShellTextEquals(argv[i], "--inc-path") ) {
            if ( !xlcAddIncludePath(ctx, argv[++i]) ) {
                return 0;
            }
        } else if ( i + 1 < argc && xlcShellTextEquals(argv[i], "--lib-path") ) {
            if ( !xlcAddLibraryPath(ctx, argv[++i]) ) {
                return 0;
            }
        } else if ( i + 1 < argc && xlcShellTextEquals(argv[i], "--link") ) {
            i += 1;
            if ( xlcShellIsDefaultRuntimeLink(argv[i]) ) {
                continue;
            }
            if ( !xlcAddLinkLibrary(ctx, argv[i]) ) {
                return 0;
            }
        } else if ( i + 1 < argc && xlcShellTextEquals(argv[i], "--loadlib") ) {
            if ( !xlcLoadDependencyLibrary(ctx, argv[++i]) ) {
                return 0;
            }
        } else if ( i + 1 < argc && (xlcShellTextEquals(argv[i], "--emit") ||
                                    xlcShellTextEquals(argv[i], "--emit-header") ||
                                    xlcShellTextEquals(argv[i], "--dump-dir") ||
                                    xlcShellTextEquals(argv[i], "--format") ||
                                    xlcShellTextEquals(argv[i], "-o")) ) {
            i += 1;
        } else if ( xlcShellOptionTakesValue(argv[i]) ) {
            i += 1;
        } else if ( argv[i] != NULL && argv[i][0] != '-' ) {
            /* Extra C sources are wrapped with the generated module header; other paths stay link inputs. */
            if ( xlcShellIsCSourcePath(argv[i]) ) {
                if ( !xlcAddCSourceFile(ctx, argv[i]) ) {
                    return 0;
                }
                continue;
            }
            if ( !xlcAddLinkLibrary(ctx, argv[i]) ) {
                return 0;
            }
        }
    }
    return 1;
}

static XLC_Context* xlcShellCreateContext(int argc, char** argv)
{
    XLC_ContextOptions options;
    XLC_Context* ctx;
    char defaultTempDir[512];
    char tempDir[512];
    const char* command;

    options = xlcDefaultContextOptions();
    options.workDir = ".";
    options.tempDir = xlcShellDefaultTempDir(defaultTempDir, sizeof(defaultTempDir)) ? defaultTempDir : NULL;
    xlcShellKeptTempDir[0] = '\0';
    command = argc >= 2 ? argv[1] : "";
    if ( xlcShellHasFlag(argc, argv, "--save-temps") ) {
        const char* tag = xlcShellTextEquals(command, "run") ? "run" : "build";
        if ( !xlcShellMakeTaggedTempDir(tempDir, sizeof(tempDir), tag) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create temporary directory\n");
            return NULL;
        }
        options.tempDir = tempDir;
        snprintf(xlcShellKeptTempDir, sizeof(xlcShellKeptTempDir), "%s", tempDir);
    }
    ctx = xlcCreateContext(&options);
    if ( ctx == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create xlc context\n");
        return NULL;
    }
    if ( !xlcShellApplyContextOptions(ctx, argc, argv) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", xlcGetLastError(ctx));
        xlcDestroyContext(ctx);
        return NULL;
    }
    return ctx;
}

static int xlcShellAddFile(XLC_Context* ctx, const char* file)
{
    if ( file == NULL || file[0] == '\0' ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "missing input file\n");
        return 0;
    }
    if ( !xlcAddSourceFile(ctx, file) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", xlcGetLastError(ctx));
        return 0;
    }
    return 1;
}

static int xlcShellRunWithFileEx(int argc, char** argv, int (*proc)(XLC_Context*, int, char**), int printLastError)
{
    XLC_Context* ctx;
    int rc;

    ctx = xlcShellCreateContext(argc, argv);
    if ( ctx == NULL ) {
        return 1;
    }
    if ( !xlcShellAddFile(ctx, xlcShellFileArg(argc, argv)) ) {
        xlcDestroyContext(ctx);
        return 1;
    }
    rc = proc(ctx, argc, argv) ? 0 : 1;
    if ( rc != 0 && printLastError ) {
        const char* errorText = xlcGetLastError(ctx);
        if ( errorText != NULL && errorText[0] != '\0' ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", errorText);
        }
    }
    xlcDestroyContext(ctx);
    return rc;
}

static int xlcShellRunWithFile(int argc, char** argv, int (*proc)(XLC_Context*, int, char**))
{
    return xlcShellRunWithFileEx(argc, argv, proc, 1);
}

static int xlcShellDoLex(XLC_Context* ctx, int argc, char** argv)
{
    return xlcEmitTokens(ctx, xlcShellFindValue(argc, argv, "--emit", 3), xlcShellHasJson(argc, argv));
}

static int xlcShellDoParse(XLC_Context* ctx, int argc, char** argv)
{
    return xlcEmitAst(ctx, xlcShellFindValue(argc, argv, "--emit", 3), xlcShellHasJson(argc, argv));
}

static int xlcShellWriteFileToConsole(const char* path, XL6_ConsoleStream stream)
{
    char* text;
    size_t size;

    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    size = 0;
    text = (char*)xrtFileReadAll((str)path, XRT_CP_BINARY, &size);
    if ( text == NULL ) {
        return 0;
    }
    if ( size > 0 ) {
        (void)xl6ConsoleWrite(stream == XL6_CONSOLE_STDERR ? XL6_CONSOLE_STDERR : XL6_CONSOLE_STDOUT, text, size);
    }
    xrtFree(text);
    return 1;
}

static int xlcShellDoCheck(XLC_Context* ctx, int argc, char** argv)
{
    char outputPath[512];
    int ok;

    (void)argv;
    if ( xlcShellHasJson(argc, argv) ) {
        return xlcCheck(ctx, NULL, 1);
    }
    if ( !xlcShellBuildTmpDir(outputPath, sizeof(outputPath)) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create temporary directory\n");
        return 0;
    }
    if ( snprintf(outputPath,
                  sizeof(outputPath),
                  "build\\tmp\\xlc_check_%lu_%lu.txt",
                  xlcShellProcessId(),
                  xlcShellTick()) <= 0 ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create temporary check path\n");
        return 0;
    }
    ok = xlcCheck(ctx, outputPath, 0);
    if ( !xlcShellWriteFileToConsole(outputPath, ok ? XL6_CONSOLE_STDOUT : XL6_CONSOLE_STDERR) ) {
        const char* errorText = xlcGetLastError(ctx);
        if ( errorText != NULL && errorText[0] != '\0' ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", errorText);
        }
    }
    (void)xrtFileDelete((str)outputPath);
    return ok;
}

static int xlcShellDoIr(XLC_Context* ctx, int argc, char** argv)
{
    return xlcEmitIr(ctx, xlcShellFindValue(argc, argv, "--emit", 3), xlcShellHasJson(argc, argv));
}

static int xlcShellDoAbi(XLC_Context* ctx, int argc, char** argv)
{
    return xlcEmitAbi(ctx, xlcShellFindValue(argc, argv, "--emit", 3), xlcShellHasJson(argc, argv));
}

static int xlcShellDoXapi(XLC_Context* ctx, int argc, char** argv)
{
    return xlcEmitXapi(ctx, xlcShellFindValue(argc, argv, "--emit", 3));
}

static int xlcShellDoRun(XLC_Context* ctx, int argc, char** argv)
{
    char* runArgs[129];
    int runArgCount;
    int argStart;
    int i;

    runArgCount = 0;
    runArgs[runArgCount++] = (char*)xlcShellFileArg(argc, argv);
    argStart = xlcShellFindArgSplit(argc, argv);
    for ( i = argStart; i < argc; ++i ) {
        if ( runArgCount >= (int)(sizeof(runArgs) / sizeof(runArgs[0])) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many program arguments\n");
            return 0;
        }
        runArgs[runArgCount++] = argv[i];
    }
    return xlcRun(ctx, runArgCount, runArgs);
}

static int xlcShellRunC(int argc, char** argv)
{
    XLC_Context* ctx;
    XL_Context* runtime;
    const char* cInputs[XLC_SHELL_MAX_C_INPUTS];
    char* runArgs[129];
    int cInputCount;
    int runArgCount;
    int argStart;
    int ok;
    int i;

    if ( !xlcShellCollectCInputs(argc, argv, cInputs, XLC_SHELL_MAX_C_INPUTS, &cInputCount) ) {
        return 1;
    }
    ctx = xlcShellCreateContext(argc, argv);
    if ( ctx == NULL ) {
        return 1;
    }
    runtime = xlcRuntimeContext(ctx);
    runArgCount = 0;
    runArgs[runArgCount++] = (char*)cInputs[0];
    argStart = xlcShellFindArgSplit(argc, argv);
    for ( i = argStart; i < argc; ++i ) {
        if ( runArgCount >= (int)(sizeof(runArgs) / sizeof(runArgs[0])) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many program arguments\n");
            xlcDestroyContext(ctx);
            return 1;
        }
        runArgs[runArgCount++] = argv[i];
    }
    ok = runtime != NULL && xlRunCSourceFiles(runtime, cInputs, cInputCount, runArgCount, runArgs);
    if ( !ok ) {
        const char* errorText = runtime != NULL ? xlGetLastError(runtime) : xlcGetLastError(ctx);
        if ( errorText != NULL && errorText[0] != '\0' ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", errorText);
        }
    }
    xlcDestroyContext(ctx);
    return ok ? 0 : 1;
}

static int xlcShellCgenDumpDir(XLC_Context* ctx, const char* inputPath, const char* dumpDir)
{
    char base[256];
    char path[512];

    if ( dumpDir == NULL ) {
        return 1;
    }
    if ( !xlcShellMakeDir(dumpDir) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create dump dir: %s\n", dumpDir);
        return 0;
    }
    xlcShellBaseNoExt(base, sizeof(base), inputPath);
    if ( !xlcShellJoinPath(path, sizeof(path), dumpDir, base, ".tokens.json") ||
         !xlcEmitTokens(ctx, path, 1) ||
         !xlcShellJoinPath(path, sizeof(path), dumpDir, base, ".ast.json") ||
         !xlcEmitAst(ctx, path, 1) ||
         !xlcShellJoinPath(path, sizeof(path), dumpDir, base, ".xlir") ||
         !xlcEmitIr(ctx, path, 0) ||
         !xlcShellJoinPath(path, sizeof(path), dumpDir, base, ".abi.json") ||
         !xlcEmitAbi(ctx, path, 1) ||
         !xlcShellJoinPath(path, sizeof(path), dumpDir, base, ".xapi.json") ||
         !xlcEmitXapi(ctx, path) ||
         !xlcShellJoinPath(path, sizeof(path), dumpDir, base, ".c") ||
         !xlcEmitC(ctx, path) ||
         !xlcShellJoinPath(path, sizeof(path), dumpDir, base, ".h") ||
         !xlcShellEmitHeader(ctx, path) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", xlcGetLastError(ctx));
        return 0;
    }
    return 1;
}

static int xlcShellEmitCStdout(XLC_Context* ctx)
{
    char workDir[512];
    char cPath[512];
    char* text;
    size_t size;
    int ok;

    workDir[0] = '\0';
    cPath[0] = '\0';
    text = NULL;
    ok = 0;

    if ( !xlcShellMakeTaggedTempDir(workDir, sizeof(workDir), "cgen") ||
         !xlcShellJoinPath(cPath, sizeof(cPath), workDir, "module", ".c") ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create temporary C output\n");
        goto cleanup;
    }
    if ( !xlcEmitC(ctx, cPath) ) {
        goto cleanup;
    }
    size = 0;
    text = (char*)xrtFileReadAll((char*)cPath, XRT_CP_UTF8, &size);
    if ( text == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot read temporary C output: %s\n", cPath);
        goto cleanup;
    }
    if ( !xl6ConsoleWrite(XL6_CONSOLE_STDOUT, text, size) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot write C output to stdout\n");
        goto cleanup;
    }
    ok = 1;

cleanup:
    xrtFree(text);
    if ( cPath[0] != '\0' ) {
        (void)xrtFileDelete((str)cPath);
    }
    if ( workDir[0] != '\0' ) {
        (void)xrtDirDelete((str)workDir);
    }
    return ok;
}

static int xlcShellDoCgen(XLC_Context* ctx, int argc, char** argv)
{
    const char* dumpDir;
    const char* emit;
    const char* emitHeader;
    int ok;

    dumpDir = xlcShellFindValue(argc, argv, "--dump-dir", 3);
    if ( dumpDir != NULL && !xlcShellCgenDumpDir(ctx, xlcShellFileArg(argc, argv), dumpDir) ) {
        return 0;
    }
    emit = xlcShellFindValue(argc, argv, "--emit", 3);
    emitHeader = xlcShellFindValue(argc, argv, "--emit-header", 3);
    if ( emit == NULL && emitHeader == NULL && dumpDir != NULL ) {
        return 1;
    }
    if ( emit == NULL && emitHeader == NULL ) {
        return xlcShellEmitCStdout(ctx);
    }
    ok = 1;
    if ( emit != NULL || emitHeader == NULL ) {
        ok = xlcEmitC(ctx, emit);
    }
    if ( ok && emitHeader != NULL ) {
        ok = xlcShellEmitHeader(ctx, emitHeader);
    }
    return ok;
}

static int xlcShellDoBuild(XLC_Context* ctx, int argc, char** argv)
{
    const char* output;
    const char* emitHeader;
    int ok;

    output = xlcShellFindValue(argc, argv, "-o", 3);
    if ( output == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "build requires -o <output.exe>\n");
        return 0;
    }
    emitHeader = xlcShellFindValue(argc, argv, "--emit-header", 3);
    if ( emitHeader != NULL && !xlcShellEmitHeader(ctx, emitHeader) ) {
        return 0;
    }
    if ( xlcShellHasFlag(argc, argv, "-shared") || xlcShellHasFlag(argc, argv, "--shared") ) {
        ok = xlcEmitSharedLibrary(ctx, output);
    } else {
        ok = xlcEmitExe(ctx, output);
    }
    if ( ok && xlcShellHasFlag(argc, argv, "--save-temps") && xlcShellKeptTempDir[0] != '\0' ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "temporary files kept: %s\n", xlcShellKeptTempDir);
    }
    return ok;
}

static int xlcShellBuildC(int argc, char** argv)
{
    XLC_Context* ctx;
    XL_Context* runtime;
    const char* cInputs[XLC_SHELL_MAX_C_INPUTS];
    const char* output;
    int cInputCount;
    int ok;

    output = xlcShellFindValue(argc, argv, "-o", 3);
    if ( output == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "build requires -o <output.exe>\n");
        return 1;
    }
    if ( !xlcShellCollectCInputs(argc, argv, cInputs, XLC_SHELL_MAX_C_INPUTS, &cInputCount) ) {
        return 1;
    }
    ctx = xlcShellCreateContext(argc, argv);
    if ( ctx == NULL ) {
        return 1;
    }
    runtime = xlcRuntimeContext(ctx);
    if ( xlcShellHasFlag(argc, argv, "-shared") || xlcShellHasFlag(argc, argv, "--shared") ) {
        ok = runtime != NULL && xlCompileCSourceFilesToSharedLibrary(runtime, cInputs, cInputCount, output);
    } else {
        ok = runtime != NULL && xlCompileCSourceFilesToExe(runtime, cInputs, cInputCount, output);
    }
    if ( !ok ) {
        const char* errorText = runtime != NULL ? xlGetLastError(runtime) : xlcGetLastError(ctx);
        if ( errorText != NULL && errorText[0] != '\0' ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", errorText);
        }
    }
    xlcDestroyContext(ctx);
    return ok ? 0 : 1;
}

static int xlcShellImpdef(int argc, char** argv)
{
    const char* dllPath;
    const char* output;

    dllPath = xlcShellFileArg(argc, argv);
    output = xlcShellFindValue(argc, argv, "-o", 3);
    if ( dllPath == NULL || dllPath[0] == '\0' || output == NULL || output[0] == '\0' ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "impdef requires <file.dll> -o <output.def>\n");
        return 1;
    }
    if ( !xlTccImpdef(dllPath, output) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "impdef failed: %s\n", dllPath);
        return 1;
    }
    return 0;
}

#define XLC_SHELL_MAX_RUN_ARGS 128
#define XLC_SHELL_MAX_ENV_ITEMS 4

static int xlcShellDoDebug(int argc, char** argv)
{
    XLC_ContextOptions options;
    XLC_Context* ctx;
    const char* file;
    const char* commandsPath;
    char* commandsText;
    char* envItems[XLC_SHELL_MAX_ENV_ITEMS];
    char* runArgs[XLC_SHELL_MAX_RUN_ARGS];
    char workDir[512];
    char cPath[512];
    char exePath[512];
    char breakpoints[1024];
    int envCount;
    int runArgCount;
    int argStart;
    int saveTemps;
    int rc;
    int i;

    ctx = NULL;
    commandsText = NULL;
    envCount = 0;
    runArgCount = 0;
    workDir[0] = '\0';
    cPath[0] = '\0';
    exePath[0] = '\0';
    for ( i = 0; i < XLC_SHELL_MAX_ENV_ITEMS; ++i ) {
        envItems[i] = NULL;
    }

    file = xlcShellFileArg(argc, argv);
    if ( file == NULL || file[0] == '\0' ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "missing input file\n");
        return 1;
    }
    if ( !xlcShellCollectBreakpoints(breakpoints, sizeof(breakpoints), argc, argv) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "debug breakpoint list is too long\n");
        return 1;
    }
    commandsPath = xlcShellFindValue(argc, argv, "--commands", 3);
    if ( commandsPath != NULL ) {
        commandsText = xlcShellReadTextFile(commandsPath);
        if ( commandsText == NULL ) {
            return 1;
        }
    }

    saveTemps = xlcShellHasFlag(argc, argv, "--save-temps");
    if ( !xlcShellMakeTaggedTempDir(workDir, sizeof(workDir), "dbg") ||
         !xlcShellJoinPath(cPath, sizeof(cPath), workDir, "module", ".c") ||
         !xlcShellJoinPath(exePath, sizeof(exePath), workDir, "app", ".exe") ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create debug temporary paths\n");
        xrtFree(commandsText);
        return 1;
    }

    options = xlcDefaultContextOptions();
    options.workDir = ".";
    options.tempDir = workDir;
    options.saveTemps = saveTemps;
    ctx = xlcCreateContext(&options);
    if ( ctx == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create xlc context\n");
        xrtFree(commandsText);
        xlcShellCleanupWorkDir(workDir, cPath, exePath, saveTemps);
        return 1;
    }
    if ( !xlcShellApplyContextOptions(ctx, argc, argv) || !xlcShellAddFile(ctx, file) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", xlcGetLastError(ctx));
        xlcDestroyContext(ctx);
        xrtFree(commandsText);
        xlcShellCleanupWorkDir(workDir, cPath, exePath, saveTemps);
        return 1;
    }
    if ( saveTemps && !xlcEmitDebugC(ctx, cPath, file) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", xlcGetLastError(ctx));
        xlcDestroyContext(ctx);
        xrtFree(commandsText);
        xlcShellCleanupWorkDir(workDir, cPath, exePath, saveTemps);
        return 1;
    }
    if ( !xlcEmitDebugExe(ctx, exePath, file) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "%s\n", xlcGetLastError(ctx));
        xlcDestroyContext(ctx);
        xrtFree(commandsText);
        xlcShellCleanupWorkDir(workDir, cPath, exePath, saveTemps);
        return 1;
    }

    argStart = xlcShellFindArgSplit(argc, argv);
    for ( i = argStart; i < argc; ++i ) {
        if ( runArgCount >= XLC_SHELL_MAX_RUN_ARGS ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many program arguments\n");
            xlcDestroyContext(ctx);
            xrtFree(commandsText);
            xlcShellCleanupWorkDir(workDir, cPath, exePath, saveTemps);
            return 1;
        }
        runArgs[runArgCount++] = argv[i];
    }
    if ( breakpoints[0] != '\0' ) {
        envItems[envCount] = xlcShellMakeEnvItem("XL6_DBG_BREAKPOINTS", breakpoints);
        if ( envItems[envCount] == NULL ) {
            rc = 1;
            goto cleanup;
        }
        envCount += 1;
    }
    if ( xlcShellHasFlag(argc, argv, "--stop-on-entry") ) {
        envItems[envCount] = xlcShellMakeEnvItem("XL6_DBG_STOP_ON_ENTRY", "1");
        if ( envItems[envCount] == NULL ) {
            rc = 1;
            goto cleanup;
        }
        envCount += 1;
    }
    if ( commandsText != NULL ) {
        envItems[envCount] = xlcShellMakeEnvItem("XL6_DBG_COMMANDS", commandsText);
        if ( envItems[envCount] == NULL ) {
            rc = 1;
            goto cleanup;
        }
        envCount += 1;
    }

    rc = xlcShellRunProcess(exePath, runArgs, runArgCount, envItems, envCount);

cleanup:
    for ( i = 0; i < XLC_SHELL_MAX_ENV_ITEMS; ++i ) {
        free(envItems[i]);
    }
    xlcDestroyContext(ctx);
    xrtFree(commandsText);
    xlcShellCleanupWorkDir(workDir, cPath, exePath, saveTemps);
    return rc == 0 ? 0 : 1;
}

int xlcShellMain(int argc, char** argv)
{
    const char* cmd;

    if ( argc < 2 ) {
        xlcShellPrintHelp();
        return 0;
    }
    cmd = argv[1];
    if ( xlcShellTextEquals(cmd, "help") || xlcShellTextEquals(cmd, "--help") || xlcShellTextEquals(cmd, "-h") ) {
        xlcShellPrintHelp();
        return 0;
    }
    if ( xlcShellTextEquals(cmd, "dap") ) {
        return xl6DapMain(argc, argv);
    }
    if ( xlcShellTextEquals(cmd, "impdef") ) {
        return xlcShellImpdef(argc, argv);
    }
    if ( xlcShellTextEquals(cmd, "lex") ) {
        return xlcShellRunWithFile(argc, argv, xlcShellDoLex);
    }
    if ( xlcShellTextEquals(cmd, "parse") ) {
        return xlcShellRunWithFile(argc, argv, xlcShellDoParse);
    }
    if ( xlcShellTextEquals(cmd, "check") ) {
        return xlcShellRunWithFileEx(argc, argv, xlcShellDoCheck, 0);
    }
    if ( xlcShellTextEquals(cmd, "ir") ) {
        return xlcShellRunWithFile(argc, argv, xlcShellDoIr);
    }
    if ( xlcShellTextEquals(cmd, "cgen") ) {
        return xlcShellRunWithFile(argc, argv, xlcShellDoCgen);
    }
    if ( xlcShellTextEquals(cmd, "abi") ) {
        return xlcShellRunWithFile(argc, argv, xlcShellDoAbi);
    }
    if ( xlcShellTextEquals(cmd, "xapi") ) {
        return xlcShellRunWithFile(argc, argv, xlcShellDoXapi);
    }
    if ( xlcShellTextEquals(cmd, "run") ) {
        if ( xlcShellIsCSourcePath(xlcShellFileArg(argc, argv)) ) {
            return xlcShellRunC(argc, argv);
        }
        return xlcShellRunWithFile(argc, argv, xlcShellDoRun);
    }
    if ( xlcShellTextEquals(cmd, "build") ) {
        if ( xlcShellIsCSourcePath(xlcShellFileArg(argc, argv)) ) {
            return xlcShellBuildC(argc, argv);
        }
        return xlcShellRunWithFile(argc, argv, xlcShellDoBuild);
    }
    if ( xlcShellTextEquals(cmd, "dbg") ) {
        return xlcShellDoDebug(argc, argv);
    }
    xl6ConsolePrintf(XL6_CONSOLE_STDERR, "unknown command: %s\n", cmd);
    return 1;
}

#ifdef _WIN32
static char** xlcShellMakeUtf8Argv(int* outArgc)
{
    int argcW;
    int i;
    LPWSTR* argvW;
    char** argv;

    if ( outArgc == NULL ) {
        return NULL;
    }
    *outArgc = 0;
    argvW = CommandLineToArgvW(GetCommandLineW(), &argcW);
    if ( argvW == NULL || argcW <= 0 ) {
        return NULL;
    }
    argv = (char**)calloc((size_t)argcW + 1, sizeof(char*));
    if ( argv == NULL ) {
        LocalFree(argvW);
        return NULL;
    }
    for ( i = 0; i < argcW; ++i ) {
        int need = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
        if ( need <= 0 ) {
            continue;
        }
        argv[i] = (char*)malloc((size_t)need);
        if ( argv[i] != NULL ) {
            WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, argv[i], need, NULL, NULL);
        }
    }
    LocalFree(argvW);
    *outArgc = argcW;
    return argv;
}

static void xlcShellFreeUtf8Argv(int argc, char** argv)
{
    int i;

    if ( argv == NULL ) {
        return;
    }
    for ( i = 0; i < argc; ++i ) {
        free(argv[i]);
    }
    free(argv);
}

int main(void)
{
    int argc;
    char** argv;
    int rc;

    argv = xlcShellMakeUtf8Argv(&argc);
    if ( argv == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot decode command line\n");
        return 1;
    }
    rc = xlcShellMain(argc, argv);
    xlcShellFreeUtf8Argv(argc, argv);
    return rc;
}
#else
int main(int argc, char** argv)
{
    return xlcShellMain(argc, argv);
}
#endif
