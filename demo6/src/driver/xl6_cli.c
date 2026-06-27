#include "src/driver/xl6_cli.h"

#include "src/abi/xl6_abi_dump.h"
#include "src/abi/xl6_xapi_dump.h"
#include "src/ast/xl6_ast_dump.h"
#include "src/backend/xl6_build_backend.h"
#include "src/backend/xl6_toolchain.h"
#include "src/cgen/xl6_cgen.h"
#include "src/common/xl6_common.h"
#include "src/common/xl6_console.h"
#include "src/compile/xl6_compile_unit.h"
#include "src/ir/xl6_ir_dump.h"
#include "src/lexer/xl6_lexer_api.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"

#include "libtcc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
int tcc_tool_impdef(int argc, char** argv);
#endif

#define XL6_TCC_VFS_XRT_OBJ "lib/xrt/xrt.o"
#define XL6_TCC_VFS_LIBTCC1 "lib/libtcc1.a"

static void xl6CliPrintHelp(void)
{
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "Usage:\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl lex <file.xl> [--format text|json]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl parse <file.xl> [--format tree|json] [--emit output.ast]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl check <file.xl> [--json]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl ir <file.xl> [--format text|json] [--emit output.xlir]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl cgen <file.xl> [--emit output.c] [--emit-header output.h] [--dump-dir dir]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl abi <file.xl> [--json] [--emit output.abi.txt]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl xapi <file.xl> [--emit output.xapi.json]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl run <file.xl> [--save-temps] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll] [-- <args>...]\n");
    xl6ConsolePuts(XL6_CONSOLE_STDOUT, "  xl build <file.xl> -o <output.exe> [-shared] [--save-temps] [--emit-header output.h] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll]\n");
}

static const char* xl6CliFileArg(int argc, char** argv)
{
    if ( argc < 3 ) {
        return NULL;
    }
    return argv[2];
}

static int xl6CliHasJson(int argc, char** argv)
{
    int i;

    for ( i = 3; i < argc; ++i ) {
        if ( strcmp(argv[i], "--json") == 0 ) {
            return 1;
        }
        if ( strcmp(argv[i], "--format") == 0 && (i + 1) < argc && strcmp(argv[i + 1], "json") == 0 ) {
            return 1;
        }
    }
    return 0;
}

static const char* xl6CliFindEmitPath(int argc, char** argv)
{
    int i;

    for ( i = 3; i < argc; ++i ) {
        if ( strcmp(argv[i], "--emit") == 0 && (i + 1) < argc ) {
            return argv[i + 1];
        }
    }
    return NULL;
}

static const char* xl6CliFindEmitHeaderPath(int argc, char** argv)
{
    int i;

    for ( i = 3; i < argc; ++i ) {
        if ( strcmp(argv[i], "--emit-header") == 0 && (i + 1) < argc ) {
            return argv[i + 1];
        }
    }
    return NULL;
}

static const char* xl6CliFindDumpDir(int argc, char** argv)
{
    int i;

    for ( i = 3; i < argc; ++i ) {
        if ( strcmp(argv[i], "--dump-dir") == 0 && (i + 1) < argc ) {
            return argv[i + 1];
        }
    }
    return NULL;
}

static const char* xl6CliFindOutputPath(int argc, char** argv)
{
    int i;

    for ( i = 3; i < argc; ++i ) {
        if ( strcmp(argv[i], "-o") == 0 && (i + 1) < argc ) {
            return argv[i + 1];
        }
    }
    return NULL;
}

static char xl6CliGuardChar(char ch)
{
    if ( ch >= 'a' && ch <= 'z' ) {
        return (char)(ch - 'a' + 'A');
    }
    if ( (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ) {
        return ch;
    }
    return '_';
}

static void xl6CliMakeHeaderGuard(char* out, size_t outSize, const char* path)
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

    base = path;
    for ( p = path; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' ) {
            base = p + 1;
        }
    }

    dot = NULL;
    for ( p = base; *p != '\0'; ++p ) {
        if ( *p == '.' ) {
            dot = p;
        }
    }

    used = 0;
    if ( outSize > 1 ) {
        snprintf(out, outSize, "XL6_");
        used = strlen(out);
    }
    for ( p = base; *p != '\0' && p != dot && used + 3 < outSize; ++p ) {
        out[used++] = xl6CliGuardChar(*p);
    }
    if ( used + 3 < outSize ) {
        out[used++] = '_';
        out[used++] = 'H';
    }
    out[used] = '\0';
}

static int xl6CliMakeDir(const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "dump dir is empty\n");
        return 1;
    }
    if ( xrtDirCreateAll((char*)path) ) {
        return 0;
    }
    xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create dump dir: %s\n", path);
    return 1;
}

static int xl6CliOutputOpen(XL6_Output* out, const char* path)
{
    if ( path == NULL ) {
        return xl6OutputOpenStdFileBuffer(out, stdout);
    }
    return xl6OutputOpenFile(out, path);
}

static int xl6CliOutputClose(XL6_Output* out, const char* path)
{
    xl6OutputClose(out);
    if ( xl6OutputHasFailed(out) ) {
        if ( path != NULL ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot write output file: %s (%s)\n", path, xl6OutputError(out));
        } else {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot write output (%s)\n", xl6OutputError(out));
        }
        return 0;
    }
    return 1;
}

static int xl6CliAsciiLower(int ch)
{
    if ( ch >= 'A' && ch <= 'Z' ) {
        return ch - 'A' + 'a';
    }
    return ch;
}

static int xl6CliEndsWithExt(const char* path, const char* ext)
{
    size_t pathLen;
    size_t extLen;
    size_t i;

    if ( path == NULL || ext == NULL ) {
        return 0;
    }
    pathLen = strlen(path);
    extLen = strlen(ext);
    if ( pathLen < extLen ) {
        return 0;
    }
    for ( i = 0; i < extLen; ++i ) {
        if ( xl6CliAsciiLower((unsigned char)path[pathLen - extLen + i]) !=
             xl6CliAsciiLower((unsigned char)ext[i]) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CliTextEqualsNoCase(const char* a, const char* b)
{
    size_t i;

    if ( a == NULL || b == NULL ) {
        return 0;
    }
    for ( i = 0; a[i] != '\0' || b[i] != '\0'; ++i ) {
        if ( xl6CliAsciiLower((unsigned char)a[i]) != xl6CliAsciiLower((unsigned char)b[i]) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CliOptionTakesValue(const char* arg)
{
    return arg != NULL &&
           (strcmp(arg, "-o") == 0 ||
            strcmp(arg, "--emit") == 0 ||
            strcmp(arg, "--emit-header") == 0 ||
            strcmp(arg, "--dump-dir") == 0 ||
            strcmp(arg, "--inc-path") == 0 ||
            strcmp(arg, "--lib-path") == 0 ||
            strcmp(arg, "--link") == 0 ||
            strcmp(arg, "--loadlib") == 0 ||
            strcmp(arg, "--format") == 0);
}

static const char* xl6CliBaseName(const char* path);

static const XL6_BuildBackend* xl6CliDefaultBackend(void)
{
    static XL6_BuildBackend backend;
    static int ready = 0;

    if ( !ready ) {
        xl6BuildBackendInitDefault(&backend);
        ready = 1;
    }
    return &backend;
}

static int xl6CliCollectBuildInputs(
    int argc,
    char** argv,
    const char** outXlPath,
    const char** cSources,
    int maxCSources,
    int* outCSourceCount)
{
    int i;
    int cCount;

    if ( outXlPath == NULL || cSources == NULL || outCSourceCount == NULL ) {
        return 0;
    }

    *outXlPath = NULL;
    *outCSourceCount = 0;
    cCount = 0;
    for ( i = 2; i < argc; ++i ) {
        const char* arg = argv[i];

        if ( strcmp(arg, "--") == 0 ) {
            break;
        }
        if ( xl6CliOptionTakesValue(arg) ) {
            i += 1;
            continue;
        }
        if ( strcmp(arg, "-shared") == 0 || arg[0] == '-' ) {
            continue;
        }
        if ( xl6CliEndsWithExt(arg, ".xl") ) {
            if ( *outXlPath != NULL ) {
                xl6ConsolePrintf(XL6_CONSOLE_STDERR, "build currently accepts one .xl module; use import for additional modules later\n");
                return 0;
            }
            *outXlPath = arg;
        } else if ( xl6CliEndsWithExt(arg, ".c") ) {
            if ( cCount >= maxCSources ) {
                xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many C source files\n");
                return 0;
            }
            cSources[cCount++] = arg;
        } else {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "unsupported build input: %s\n", arg);
            return 0;
        }
    }
    if ( *outXlPath == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "build requires one .xl input\n");
        return 0;
    }
    *outCSourceCount = cCount;
    return 1;
}

static int xl6CliIsXrtLinkValue(const char* value)
{
    const char* base;

    if ( value == NULL ) {
        return 0;
    }
    base = xl6CliBaseName(value);
    return xl6CliTextEqualsNoCase(value, "xrt") ||
           xl6CliTextEqualsNoCase(base, "xrt.o") ||
           xl6CliTextEqualsNoCase(base, "xrt.obj") ||
           xl6CliTextEqualsNoCase(base, "xrt.lib") ||
           xl6CliTextEqualsNoCase(base, "libxrt.a");
}

static int xl6CliIsTccRuntimeLinkValue(const char* value)
{
    const char* base;

    if ( value == NULL ) {
        return 0;
    }
    base = xl6CliBaseName(value);
    return xl6CliTextEqualsNoCase(base, "libtcc1-64.a") ||
           xl6CliTextEqualsNoCase(base, "libtcc1.a");
}

static int xl6CliHasUserLink(
    int argc,
    char** argv,
    int (*match)(const char* value))
{
    int i;

    if ( match == NULL ) {
        return 0;
    }
    for ( i = 3; i < argc; ++i ) {
        if ( strcmp(argv[i], "--") == 0 ) {
            break;
        }
        if ( strcmp(argv[i], "--link") == 0 && (i + 1) < argc ) {
            if ( match(argv[i + 1]) ) {
                return 1;
            }
            i += 1;
        } else if ( xl6CliOptionTakesValue(argv[i]) ) {
            i += 1;
        }
    }
    return 0;
}

static int xl6CliHasSharedFlag(int argc, char** argv)
{
    int i;

    for ( i = 2; i < argc; ++i ) {
        if ( strcmp(argv[i], "--") == 0 ) {
            break;
        }
        if ( strcmp(argv[i], "-shared") == 0 ) {
            return 1;
        }
        if ( xl6CliOptionTakesValue(argv[i]) ) {
            i += 1;
        }
    }
    return 0;
}

static int xl6CliHasSaveTempsFlag(int argc, char** argv)
{
    int i;

    for ( i = 2; i < argc; ++i ) {
        if ( strcmp(argv[i], "--") == 0 ) {
            break;
        }
        if ( strcmp(argv[i], "--save-temps") == 0 ) {
            return 1;
        }
        if ( xl6CliOptionTakesValue(argv[i]) ) {
            i += 1;
        }
    }
    return 0;
}

static void xl6CliNormalizeIncludePath(char* out, size_t outSize, const char* path)
{
    char full[512];
    const char* src;
    size_t i;

    if ( out == NULL || outSize == 0 ) {
        return;
    }
    out[0] = '\0';
    if ( path == NULL ) {
        return;
    }
#ifdef _WIN32
    if ( _fullpath(full, path, sizeof(full)) != NULL ) {
        src = full;
    } else {
        src = path;
    }
#else
    src = path;
#endif
    for ( i = 0; src[i] != '\0' && i + 1 < outSize; ++i ) {
        out[i] = src[i] == '\\' ? '/' : src[i];
    }
    out[i] = '\0';
}

static void xl6CliFputsIncludeString(XL6_Output* out, const char* path)
{
    const char* p;

    xl6OutputPutc('"', out);
    for ( p = path != NULL ? path : ""; *p != '\0'; ++p ) {
        if ( *p == '"' || *p == '\\' ) {
            xl6OutputPutc('\\', out);
        }
        xl6OutputPutc(*p, out);
    }
    xl6OutputPutc('"', out);
}

static int xl6CliWriteCWrapper(const char* wrapperPath, const char* sourcePath, const char* headerPath)
{
    XL6_Output out;
    char sourceInclude[512];
    char headerInclude[512];
    char xrtInclude[512];

    if ( !xl6OutputOpenFile(&out, wrapperPath) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot write C wrapper: %s\n", wrapperPath);
        return 1;
    }

    xl6CliNormalizeIncludePath(sourceInclude, sizeof(sourceInclude), sourcePath);
    xl6CliNormalizeIncludePath(headerInclude, sizeof(headerInclude), headerPath);
    xl6CliNormalizeIncludePath(xrtInclude, sizeof(xrtInclude), "lib\\xrt\\xrt.h");

    xl6OutputPuts("/* Generated by xlang DEMO6 build: C interop wrapper. */\n", &out);
    xl6OutputPuts("#include ", &out);
    xl6CliFputsIncludeString(&out, xrtInclude);
    xl6OutputPutc('\n', &out);
    xl6OutputPuts("#include ", &out);
    xl6CliFputsIncludeString(&out, headerInclude);
    xl6OutputPutc('\n', &out);
    xl6OutputPuts("#include ", &out);
    xl6CliFputsIncludeString(&out, sourceInclude);
    xl6OutputPutc('\n', &out);

    return xl6CliOutputClose(&out, wrapperPath) ? 0 : 1;
}

static const char* xl6CliBaseName(const char* path)
{
    const char* base;
    const char* p;

    base = path != NULL ? path : "module";
    for ( p = base; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' ) {
            base = p + 1;
        }
    }
    return base;
}

static void xl6CliBaseNameNoExt(char* out, size_t outSize, const char* path)
{
    const char* base;
    const char* p;
    const char* dot;
    size_t len;

    if ( out == NULL || outSize == 0 ) {
        return;
    }

    base = xl6CliBaseName(path);
    dot = NULL;
    for ( p = base; *p != '\0'; ++p ) {
        if ( *p == '.' ) {
            dot = p;
        }
    }

    len = dot != NULL ? (size_t)(dot - base) : strlen(base);
    if ( len == 0 ) {
        base = "module";
        len = strlen(base);
    }
    if ( len >= outSize ) {
        len = outSize - 1;
    }
    memcpy(out, base, len);
    out[len] = '\0';
}

static int xl6CliJoinDumpPath(char* out, size_t outSize, const char* dir, const char* base, const char* suffix)
{
    const char* sep;
    size_t dirLen;
    int written;

    if ( out == NULL || outSize == 0 || dir == NULL || base == NULL || suffix == NULL ) {
        return 0;
    }

    dirLen = strlen(dir);
    sep = (dirLen > 0 && (dir[dirLen - 1] == '\\' || dir[dirLen - 1] == '/')) ? "" : "\\";
    written = snprintf(out, outSize, "%s%s%s%s", dir, sep, base, suffix);
    return written > 0 && (size_t)written < outSize;
}

static int xl6CliEnsureTmpDir(void)
{
    if ( xrtDirCreateAll("release\\tmp") ) {
        return 1;
    }
    xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create temporary directory: release\\tmp\n");
    return 0;
}

static unsigned long xl6CliNextTempId(void)
{
    static unsigned long counter = 0;

    counter += 1;
    return counter;
}

static unsigned long xl6CliProcessId(void)
{
#ifdef _WIN32
    return (unsigned long)_getpid();
#else
    return (unsigned long)getpid();
#endif
}

static unsigned long xl6CliRandomU32(void)
{
    uint32 value;

    value = 0;
    xrtRandomBytes((uint8*)&value, sizeof(value));
    return (unsigned long)value;
}

static int xl6CliMakeWorkDir(char* out, size_t outSize, const char* purpose)
{
    int written;

    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    if ( !xl6CliEnsureTmpDir() ) {
        return 0;
    }
    written = snprintf(
        out,
        outSize,
        "release\\tmp\\xl6_%s_%lu_%lu_%lu_%lu",
        purpose != NULL ? purpose : "work",
        (unsigned long)xrtNow(),
        xl6CliProcessId(),
        xl6CliRandomU32(),
        xl6CliNextTempId());
    if ( written <= 0 || (size_t)written >= outSize ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "temporary directory path is too long\n");
        return 0;
    }
    return xl6CliMakeDir(out) == 0;
}

static void xl6CliRemoveDir(const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        return;
    }
    (void)xrtDirDelete((char*)path);
}

static void xl6CliCleanupWorkDir(const char* workDir, const char** files, int fileCount, int saveTemps)
{
    int i;

    if ( saveTemps ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "temporary files kept: %s\n", workDir != NULL ? workDir : "<tmp>");
        return;
    }
    for ( i = 0; i < fileCount; ++i ) {
        if ( files[i] != NULL && files[i][0] != '\0' ) {
            (void)xrtFileDelete((char*)files[i]);
        }
    }
    xl6CliRemoveDir(workDir);
}

static int xl6CliJoinPath(char* out, size_t outSize, const char* dir, const char* name)
{
    const char* sep;
    int written;
    size_t dirLen;

    if ( out == NULL || dir == NULL || name == NULL ) {
        return 0;
    }
    dirLen = strlen(dir);
    sep = (dirLen > 0 && (dir[dirLen - 1] == '\\' || dir[dirLen - 1] == '/')) ? "" : "\\";
    written = snprintf(out, outSize, "%s%s%s", dir, sep, name);
    return written > 0 && (size_t)written < outSize;
}

static int xl6CliAppendText(char* buffer, size_t bufferSize, const char* text);

static void xl6CliDirName(char* out, size_t outSize, const char* path)
{
    const char* lastSep;
    const char* p;
    size_t len;

    if ( out == NULL || outSize == 0 ) {
        return;
    }
    out[0] = '\0';
    if ( path == NULL || path[0] == '\0' ) {
        snprintf(out, outSize, ".");
        return;
    }

    lastSep = NULL;
    for ( p = path; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' ) {
            lastSep = p;
        }
    }
    if ( lastSep == NULL ) {
        snprintf(out, outSize, ".");
        return;
    }

    len = (size_t)(lastSep - path);
    if ( *lastSep == ':' ) {
        len += 1;
    }
    if ( len >= outSize ) {
        len = outSize - 1;
    }
    memcpy(out, path, len);
    out[len] = '\0';
}

static int xl6CliAppendRunPathEnvDir(char* pathEnv, size_t pathEnvSize, int* started, const char* dllPath)
{
    char dir[512];

    if ( pathEnv == NULL || started == NULL || dllPath == NULL || dllPath[0] == '\0' ) {
        return 0;
    }
    xl6CliDirName(dir, sizeof(dir), dllPath);
    if ( !*started ) {
        if ( !xl6CliAppendText(pathEnv, pathEnvSize, "PATH=") ) {
            return 0;
        }
        *started = 1;
    } else {
        if ( !xl6CliAppendText(pathEnv, pathEnvSize, ";") ) {
            return 0;
        }
    }
    return xl6CliAppendText(pathEnv, pathEnvSize, dir);
}

static int xl6CliBuildRunPathEnv(
    char* pathEnv,
    size_t pathEnvSize,
    int argc,
    char** argv,
    const XL6_ImportDeps* deps,
    char** envItems,
    int* envCount)
{
    int i;
    int started;
    const char* oldPath;

    if ( pathEnv == NULL || pathEnvSize == 0 || envItems == NULL || envCount == NULL ) {
        return 0;
    }
    pathEnv[0] = '\0';
    envItems[0] = NULL;
    *envCount = 0;
    started = 0;
    for ( i = 3; i < argc; ++i ) {
        if ( strcmp(argv[i], "--") == 0 ) {
            break;
        }
        if ( strcmp(argv[i], "--loadlib") == 0 && (i + 1) < argc ) {
            if ( !xl6CliAppendRunPathEnvDir(pathEnv, pathEnvSize, &started, argv[i + 1]) ) {
                return 0;
            }
            i += 1;
        } else if ( xl6CliOptionTakesValue(argv[i]) ) {
            i += 1;
        }
    }
    if ( deps != NULL ) {
        size_t depIndex;
        for ( depIndex = 0; depIndex < deps->loadlibs.count; ++depIndex ) {
            if ( !xl6CliAppendRunPathEnvDir(pathEnv, pathEnvSize, &started, deps->loadlibs.items[depIndex]) ) {
                return 0;
            }
        }
    }
    if ( !started ) {
        return 1;
    }
    oldPath = getenv("PATH");
    if ( oldPath != NULL && oldPath[0] != '\0' ) {
        if ( !xl6CliAppendText(pathEnv, pathEnvSize, ";") ||
             !xl6CliAppendText(pathEnv, pathEnvSize, oldPath) ) {
            return 0;
        }
    }
    envItems[0] = pathEnv;
    *envCount = 1;
    return 1;
}

static int xl6CliAppendText(char* buffer, size_t bufferSize, const char* text)
{
    size_t used;
    size_t add;

    if ( buffer == NULL || text == NULL ) {
        return 0;
    }
    used = strlen(buffer);
    add = strlen(text);
    if ( used + add + 1 >= bufferSize ) {
        return 0;
    }
    memcpy(buffer + used, text, add + 1);
    return 1;
}

#define XL6_CLI_PROCESS_ARG_MAX 256

typedef struct XL6_CliProcessArgs {
    char* items[XL6_CLI_PROCESS_ARG_MAX];
    unsigned char owned[XL6_CLI_PROCESS_ARG_MAX];
    int count;
} XL6_CliProcessArgs;

static const char* xl6CliEnvPathOrDefault(const char* name, const char* fallback);

static void xl6CliProcessArgsInit(XL6_CliProcessArgs* args)
{
    if ( args != NULL ) {
        memset(args, 0, sizeof(*args));
    }
}

static void xl6CliProcessArgsFree(XL6_CliProcessArgs* args)
{
    int i;

    if ( args == NULL ) {
        return;
    }
    for ( i = 0; i < args->count; ++i ) {
        if ( args->owned[i] && args->items[i] != NULL ) {
            xrtFree(args->items[i]);
        }
    }
    xl6CliProcessArgsInit(args);
}

static int xl6CliProcessArgsAddBorrow(XL6_CliProcessArgs* args, const char* value)
{
    if ( args == NULL || args->count >= XL6_CLI_PROCESS_ARG_MAX ) {
        return 0;
    }
    args->items[args->count] = (char*)(value != NULL ? value : "");
    args->owned[args->count] = 0;
    args->count += 1;
    return 1;
}

static int xl6CliProcessArgsAddCopy(XL6_CliProcessArgs* args, const char* value)
{
    char* copy;
    size_t len;

    if ( args == NULL || args->count >= XL6_CLI_PROCESS_ARG_MAX ) {
        return 0;
    }
    value = value != NULL ? value : "";
    len = strlen(value);
    copy = (char*)xrtMalloc(len + 1u);
    if ( copy == NULL ) {
        return 0;
    }
    memcpy(copy, value, len + 1u);
    args->items[args->count] = copy;
    args->owned[args->count] = 1;
    args->count += 1;
    return 1;
}

static int xl6CliProcessArgsAddPrefixed(XL6_CliProcessArgs* args, const char* prefix, const char* value)
{
    char* copy;
    size_t prefixLen;
    size_t valueLen;

    if ( args == NULL || args->count >= XL6_CLI_PROCESS_ARG_MAX || prefix == NULL ) {
        return 0;
    }
    value = value != NULL ? value : "";
    prefixLen = strlen(prefix);
    valueLen = strlen(value);
    copy = (char*)xrtMalloc(prefixLen + valueLen + 1u);
    if ( copy == NULL ) {
        return 0;
    }
    memcpy(copy, prefix, prefixLen);
    memcpy(copy + prefixLen, value, valueLen + 1u);
    args->items[args->count] = copy;
    args->owned[args->count] = 1;
    args->count += 1;
    return 1;
}

static int xl6CliRunProcess(const char* program, XL6_CliProcessArgs* args, char** env, int envCount)
{
    xprocessconfig config;
    xprocessresult result;
    const char* errorText;
    int exitCode;

    if ( program == NULL || program[0] == '\0' ) {
        return 1;
    }

    /* 编译器内部调用工具链时不走 shell，避免 Windows shell 引用规则污染参数。 */
    xrtProcessConfigInit(&config);
    config.iTargetKind = XPROC_TARGET_EXEC;
    config.sProgram = (char*)program;
    config.arrArgs = args != NULL ? (str*)args->items : NULL;
    config.iArgCount = args != NULL ? (uint32)args->count : 0u;
    config.arrEnv = env != NULL ? (str*)env : NULL;
    config.iEnvCount = env != NULL && envCount > 0 ? (uint32)envCount : 0u;
    config.bHideWindow = true;

    memset(&result, 0, sizeof(result));
    if ( !xrtExecCapture(&config, &result, 0u) ) {
        errorText = xrtGetError();
        xl6ConsolePrintf(XL6_CONSOLE_STDERR,
                         "process spawn failed: %s\n",
                         errorText != NULL && errorText[0] != '\0' ? errorText : program);
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

static void xl6CliTccError(void* opaque, const char* msg)
{
    (void)opaque;
    xl6ConsolePrintf(XL6_CONSOLE_STDERR, "tcc: %s\n", msg != NULL ? msg : "unknown error");
}

static int xl6CliEmbeddedTccAddOption(TCCState* s, const char* arg, int* inOutSkipNext)
{
    if ( s == NULL || arg == NULL || inOutSkipNext == NULL ) {
        return 0;
    }

    if ( *inOutSkipNext ) {
        *inOutSkipNext = 0;
        return 1;
    }

    /* 输出路径由调用者统一传给 tcc_output_file，这里只跳过原命令行形态。 */
    if ( strcmp(arg, "-o") == 0 ) {
        *inOutSkipNext = 1;
        return 1;
    }

    if ( strcmp(arg, "-shared") == 0 ) {
        return 1;
    }

    if ( strncmp(arg, "-I", 2) == 0 ) {
        return tcc_add_include_path(s, arg + 2) == 0;
    }

    if ( strncmp(arg, "-L", 2) == 0 ) {
        return tcc_add_library_path(s, arg + 2) == 0;
    }

    if ( strncmp(arg, "-l", 2) == 0 ) {
        return tcc_add_library(s, arg + 2) == 0;
    }

    if ( arg[0] == '-' ) {
        return tcc_set_options(s, arg) == 0;
    }

    return tcc_add_file(s, arg) == 0;
}

static int xl6CliRunEmbeddedTcc(XL6_CliProcessArgs* args, const char* outputPath, int outputType)
{
    TCCState* s;
    int i;
    int skipNext;
    int ok;

    if ( args == NULL || outputPath == NULL || outputPath[0] == '\0' ) {
        return 1;
    }

    s = tcc_new();
    if ( s == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot create embedded tcc state\n");
        return 1;
    }

    tcc_set_error_func(s, NULL, xl6CliTccError);
    tcc_set_lib_path(s, xl6CliEnvPathOrDefault("XL6_TCC_ROOT", XL6_TCC_ROOT_PATH));
    if ( tcc_set_output_type(s, outputType) != 0 ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot set embedded tcc output type\n");
        tcc_delete(s);
        return 1;
    }

    ok = tcc_set_options(s, "-std=c11 -D_CRT_SECURE_NO_WARNINGS") == 0;
    skipNext = 0;
    for ( i = 0; ok && i < (int)args->count; ++i ) {
        ok = xl6CliEmbeddedTccAddOption(s, args->items[i], &skipNext);
    }
    if ( ok ) {
        ok = tcc_output_file(s, outputPath) == 0;
    }
    tcc_delete(s);

    if ( !ok ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "embedded tcc output failed: %s\n", outputPath);
        return 1;
    }
    return 0;
}

static int xl6CliLooksLikeLinkFile(const char* text)
{
    const char* p;

    if ( text == NULL ) {
        return 0;
    }
    for ( p = text; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' || *p == '.' ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CliProcessArgsAddLinkOption(XL6_CliProcessArgs* args, const char* value)
{
    if ( xl6CliLooksLikeLinkFile(value) ) {
        return xl6CliProcessArgsAddBorrow(args, value);
    }
    return xl6CliProcessArgsAddPrefixed(args, "-l", value != NULL ? value : "");
}

static const char* xl6CliEnvPathOrDefault(const char* name, const char* fallback)
{
    const char* envPath;

    if ( name == NULL || name[0] == '\0' ) {
        return fallback;
    }
    envPath = getenv(name);
    return envPath != NULL && envPath[0] != '\0' ? envPath : fallback;
}

static const char* xl6CliDefaultTccIncludePath(void)
{
    return xl6CliEnvPathOrDefault("XL6_TCC_INCLUDE", XL6_TCC_INCLUDE_PATH);
}

static const char* xl6CliDefaultTccWinIncludePath(void)
{
    return xl6CliEnvPathOrDefault("XL6_TCC_WIN_INCLUDE", XL6_TCC_WIN_INCLUDE_PATH);
}

static const char* xl6CliDefaultTccWinapiIncludePath(void)
{
    return xl6CliEnvPathOrDefault("XL6_TCC_WINAPI_INCLUDE", XL6_TCC_WINAPI_INCLUDE_PATH);
}

static const char* xl6CliDefaultTccLibPath(void)
{
    return xl6CliEnvPathOrDefault("XL6_TCC_LIB", XL6_TCC_LIB_PATH);
}

static const char* xl6CliDefaultTccLibtcc1Path(void)
{
    return xl6CliEnvPathOrDefault("XL6_TCC_LIBTCC1", XL6_TCC_LIBTCC1_PATH);
}

static int xl6CliProcessArgsAddDefaultToolchain(XL6_CliProcessArgs* args)
{
    return xl6CliProcessArgsAddPrefixed(args, "-I", "lib\\xrt") &&
           xl6CliProcessArgsAddPrefixed(args, "-I", "release\\tcc\\include_win") &&
           xl6CliProcessArgsAddPrefixed(args, "-I", xl6CliDefaultTccIncludePath()) &&
           xl6CliProcessArgsAddPrefixed(args, "-I", xl6CliDefaultTccWinIncludePath()) &&
           xl6CliProcessArgsAddPrefixed(args, "-I", xl6CliDefaultTccWinapiIncludePath()) &&
           xl6CliProcessArgsAddPrefixed(args, "-I", "release\\tmp") &&
           xl6CliProcessArgsAddPrefixed(args, "-L", xl6CliDefaultTccLibPath());
}

static const char* xl6CliDefaultXrtObjPath(void)
{
    return xl6CliEnvPathOrDefault("XL6_XRT_OBJ", XL6_XRT_OBJ_PATH);
}

static int xl6CliMakeLoadlibDef(const char* dllPath, int index, const char* workDir, char* outPath, size_t outSize)
{
#ifndef _WIN32
    XL6_CliProcessArgs args;
#endif
    int written;
    int rc;
#ifndef _WIN32
    const XL6_BuildBackend* backend;
#endif

    if ( dllPath == NULL || dllPath[0] == '\0' || outPath == NULL || outSize == 0 ) {
        return 0;
    }

    if ( workDir != NULL && workDir[0] != '\0' ) {
        char defName[64];
        written = snprintf(defName, sizeof(defName), "xl6_loadlib_%d.def", index);
        if ( written <= 0 || (size_t)written >= sizeof(defName) ||
             !xl6CliJoinPath(outPath, outSize, workDir, defName) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "loadlib def path is too long\n");
            return 0;
        }
    } else {
        if ( !xl6CliEnsureTmpDir() ) {
            return 0;
        }
        written = snprintf(outPath, outSize, "release\\tmp\\xl6_loadlib_%d.def", index);
        if ( written <= 0 || (size_t)written >= outSize ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "loadlib def path is too long\n");
            return 0;
        }
    }

#ifdef _WIN32
    {
        char* impdefArgs[5];

        impdefArgs[0] = (char*)"tcc";
        impdefArgs[1] = (char*)dllPath;
        impdefArgs[2] = (char*)"-o";
        impdefArgs[3] = outPath;
        impdefArgs[4] = NULL;
        rc = tcc_tool_impdef(4, impdefArgs);
    }
    if ( rc != 0 ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "loadlib import generation failed: %s\n", dllPath);
        return 0;
    }
    return 1;
#else
    backend = xl6CliDefaultBackend();
    xl6CliProcessArgsInit(&args);
    if ( !xl6CliProcessArgsAddBorrow(&args, "-impdef") ||
         !xl6CliProcessArgsAddBorrow(&args, dllPath) ||
         !xl6CliProcessArgsAddBorrow(&args, "-o") ||
         !xl6CliProcessArgsAddBorrow(&args, outPath) ) {
        xl6CliProcessArgsFree(&args);
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "loadlib impdef arguments are too long\n");
        return 0;
    }

    rc = xl6CliRunProcess(xl6BuildBackendCcPath(backend), &args, NULL, 0);
    xl6CliProcessArgsFree(&args);
    if ( rc != 0 ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "loadlib import generation failed: %s\n", dllPath);
        return 0;
    }
    return 1;
#endif
}

static int xl6CliAppendTccOptions(
    XL6_CliProcessArgs* args,
    int argc,
    char** argv,
    const char* workDir,
    char loadlibDefPaths[][512],
    int maxLoadlibDefs,
    int* outLoadlibDefCount)
{
    int i;
    int loadlibIndex;

    if ( outLoadlibDefCount != NULL ) {
        *outLoadlibDefCount = 0;
    }
    loadlibIndex = 0;
    for ( i = 3; i < argc; ++i ) {
        if ( strcmp(argv[i], "--") == 0 ) {
            break;
        }
        if ( strcmp(argv[i], "--inc-path") == 0 && (i + 1) < argc ) {
            if ( !xl6CliProcessArgsAddPrefixed(args, "-I", argv[i + 1]) ) {
                return 0;
            }
            i += 1;
        } else if ( strcmp(argv[i], "--lib-path") == 0 && (i + 1) < argc ) {
            if ( !xl6CliProcessArgsAddPrefixed(args, "-L", argv[i + 1]) ) {
                return 0;
            }
            i += 1;
        } else if ( strcmp(argv[i], "--link") == 0 && (i + 1) < argc ) {
            if ( !xl6CliProcessArgsAddLinkOption(args, argv[i + 1]) ) {
                return 0;
            }
            i += 1;
        } else if ( strcmp(argv[i], "--loadlib") == 0 && (i + 1) < argc ) {
            char defPath[512];
            if ( loadlibIndex >= maxLoadlibDefs ) {
                xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many loadlib files\n");
                return 0;
            }
            if ( !xl6CliMakeLoadlibDef(argv[i + 1], loadlibIndex, workDir, defPath, sizeof(defPath)) ||
                 !xl6CliProcessArgsAddCopy(args, defPath) ) {
                return 0;
            }
            if ( loadlibDefPaths != NULL ) {
                snprintf(loadlibDefPaths[loadlibIndex], 512, "%s", defPath);
            }
            loadlibIndex += 1;
            if ( outLoadlibDefCount != NULL ) {
                *outLoadlibDefCount = loadlibIndex;
            }
            i += 1;
        } else if ( strcmp(argv[i], "-shared") == 0 ) {
            if ( !xl6CliProcessArgsAddBorrow(args, "-shared") ||
                 !xl6CliProcessArgsAddBorrow(args, "-DXL6_BUILD_SHARED") ) {
                return 0;
            }
        } else if ( (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--emit-header") == 0 ||
                    strcmp(argv[i], "--dump-dir") == 0 ||
                    strcmp(argv[i], "--emit") == 0) && (i + 1) < argc ) {
            i += 1;
        }
    }
    return 1;
}

static int xl6CliAppendImportDepsTccOptions(
    XL6_CliProcessArgs* args,
    const XL6_ImportDeps* deps,
    const char* workDir,
    char loadlibDefPaths[][512],
    int maxLoadlibDefs,
    int* inOutLoadlibDefCount)
{
    size_t i;
    int loadlibIndex;

    if ( deps == NULL ) {
        return 1;
    }
    for ( i = 0; i < deps->links.count; ++i ) {
        if ( !xl6CliProcessArgsAddLinkOption(args, deps->links.items[i]) ) {
            return 0;
        }
    }

    loadlibIndex = inOutLoadlibDefCount != NULL ? *inOutLoadlibDefCount : 0;
    for ( i = 0; i < deps->loadlibs.count; ++i ) {
        char defPath[512];
        if ( loadlibIndex >= maxLoadlibDefs ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many import loadlib files\n");
            return 0;
        }
        if ( !xl6CliMakeLoadlibDef(deps->loadlibs.items[i], loadlibIndex, workDir, defPath, sizeof(defPath)) ||
             !xl6CliProcessArgsAddCopy(args, defPath) ) {
            return 0;
        }
        if ( loadlibDefPaths != NULL ) {
            snprintf(loadlibDefPaths[loadlibIndex], 512, "%s", defPath);
        }
        loadlibIndex += 1;
    }
    if ( inOutLoadlibDefCount != NULL ) {
        *inOutLoadlibDefCount = loadlibIndex;
    }
    return 1;
}

static int xl6CliAppendDefaultXrtLinks(XL6_CliProcessArgs* args, int argc, char** argv)
{
    const char* xrtObjPath;
    const char* tccRuntimePath;

    if ( !xl6CliProcessArgsAddBorrow(args, "-DXL6_USE_XRT_LIFECYCLE") ) {
        return 0;
    }
    if ( !xl6CliHasUserLink(argc, argv, xl6CliIsXrtLinkValue) ) {
        xrtObjPath = xl6CliDefaultXrtObjPath();
        if ( xrtFileExists((char*)xrtObjPath) ) {
            if ( !xl6CliProcessArgsAddLinkOption(args, xrtObjPath) ) {
                return 0;
            }
        } else {
            if ( !xl6CliProcessArgsAddLinkOption(args, XL6_TCC_VFS_XRT_OBJ) ) {
                return 0;
            }
        }
    }
    tccRuntimePath = xl6CliDefaultTccLibtcc1Path();
    if ( !xl6CliHasUserLink(argc, argv, xl6CliIsTccRuntimeLinkValue) ) {
        if ( !xl6CliProcessArgsAddLinkOption(
                 args,
                 xrtFileExists((char*)tccRuntimePath) ? tccRuntimePath : XL6_TCC_VFS_LIBTCC1) ) {
            return 0;
        }
    }
    return xl6CliProcessArgsAddLinkOption(args, "Ws2_32") &&
           xl6CliProcessArgsAddLinkOption(args, "IPHLPAPI") &&
           xl6CliProcessArgsAddLinkOption(args, "Shell32");
}

static int xl6CliCompileWithTccInputs(
    const char** cPaths,
    int cPathCount,
    const char* exePath,
    int argc,
    char** argv,
    const char* workDir,
    const XL6_ImportDeps* deps,
    char loadlibDefPaths[][512],
    int maxLoadlibDefs,
    int* outLoadlibDefCount)
{
    XL6_CliProcessArgs args;
    int i;
    int rc;
    int outputType;

    if ( cPaths == NULL || cPathCount <= 0 ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "no C input for tcc\n");
        return 1;
    }

    xl6CliProcessArgsInit(&args);
    if ( !xl6CliProcessArgsAddDefaultToolchain(&args) ) {
        xl6CliProcessArgsFree(&args);
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "tcc toolchain arguments are too long\n");
        return 1;
    }
    for ( i = 0; i < cPathCount; ++i ) {
        if ( !xl6CliProcessArgsAddBorrow(&args, cPaths[i]) ) {
            xl6CliProcessArgsFree(&args);
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many tcc source arguments\n");
            return 1;
        }
    }
    if ( !xl6CliProcessArgsAddBorrow(&args, "-o") ||
         !xl6CliProcessArgsAddBorrow(&args, exePath) ) {
        xl6CliProcessArgsFree(&args);
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many tcc output arguments\n");
        return 1;
    }
    if ( !xl6CliAppendTccOptions(&args, argc, argv, workDir, loadlibDefPaths, maxLoadlibDefs, outLoadlibDefCount) ) {
        xl6CliProcessArgsFree(&args);
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "tcc options are too many\n");
        return 1;
    }
    if ( !xl6CliAppendImportDepsTccOptions(&args, deps, workDir, loadlibDefPaths, maxLoadlibDefs, outLoadlibDefCount) ) {
        xl6CliProcessArgsFree(&args);
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "import package tcc options are too many\n");
        return 1;
    }
    if ( !xl6CliAppendDefaultXrtLinks(&args, argc, argv) ) {
        xl6CliProcessArgsFree(&args);
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "tcc link arguments are too many\n");
        return 1;
    }
    outputType = xl6CliHasSharedFlag(argc, argv) ? TCC_OUTPUT_DLL : TCC_OUTPUT_EXE;
    rc = xl6CliRunEmbeddedTcc(&args, exePath, outputType);
    xl6CliProcessArgsFree(&args);
    return rc;
}

static int xl6CliCompileWithTcc(
    const char* cPath,
    const char* exePath,
    int argc,
    char** argv,
    const char* workDir,
    const XL6_ImportDeps* deps,
    char loadlibDefPaths[][512],
    int maxLoadlibDefs,
    int* outLoadlibDefCount)
{
    const char* inputs[1];

    inputs[0] = cPath;
    return xl6CliCompileWithTccInputs(inputs, 1, exePath, argc, argv, workDir, deps, loadlibDefPaths, maxLoadlibDefs, outLoadlibDefCount);
}

static int xl6CliWriteHeader(const char* headerPath, XL6_AstNode* root, XL6_DiagList* diags)
{
    XL6_Output out;
    char guard[256];
    int ok;

    if ( headerPath == NULL ) {
        return 0;
    }

    if ( !xl6OutputOpenFile(&out, headerPath) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot open header output file: %s\n", headerPath);
        return 1;
    }

    xl6CliMakeHeaderGuard(guard, sizeof(guard), headerPath);
    ok = xl6CgenEmitHeader(&out, root, guard, diags);
    if ( !xl6CliOutputClose(&out, headerPath) ) {
        return 1;
    }
    return ok ? 0 : 1;
}

static int xl6CliLoadCheckedUnit(XL6_CompileUnit* unit, const char* path)
{
    xl6CompileUnitInit(unit);
    if ( !xl6CompileUnitLoadFile(unit, path, 1) ) {
        xl6DiagPrintTextWithPhase(&unit->parse.diags, path, unit->phase);
        return 0;
    }
    return 1;
}

static int xl6CliCgenUnitToFile(XL6_CompileUnit* unit, const char* cPath)
{
    XL6_Output out;
    int ok;

    if ( !xl6OutputOpenFile(&out, cPath) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot open c output file: %s\n", cPath);
        return 1;
    }

    ok = xl6CgenEmitC(&out, unit->parse.root, &unit->parse.diags);
    if ( !xl6CliOutputClose(&out, cPath) ) {
        return 1;
    }
    if ( !ok ) {
        xl6DiagPrintText(&unit->parse.diags, unit->path);
        return 1;
    }
    return 0;
}

static int xl6CliHeaderUnitToFile(XL6_CompileUnit* unit, const char* headerPath)
{
    int rc;

    rc = xl6CliWriteHeader(headerPath, unit->parse.root, &unit->parse.diags);
    if ( rc != 0 ) {
        xl6DiagPrintText(&unit->parse.diags, unit->path);
    }
    return rc;
}

static int xl6CliXapiUnitToFile(XL6_CompileUnit* unit, const char* xapiPath, XL6_XapiModule* outModule)
{
    XL6_XapiModule localModule;
    XL6_XapiModule* module;
    XL6_Output out;
    int ok;

    if ( outModule != NULL ) {
        module = outModule;
        xl6XapiModuleInit(module);
    } else {
        module = &localModule;
        xl6XapiModuleInit(module);
    }

    ok = xl6XapiModuleBuild(module, unit->parse.root, unit->path, unit->moduleName);
    if ( !ok ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot build XAPI metadata\n");
        if ( outModule == NULL ) {
            xl6XapiModuleFree(module);
        }
        return 1;
    }

    if ( !xl6CliOutputOpen(&out, xapiPath) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot open emit file: %s\n", xapiPath);
        if ( outModule == NULL ) {
            xl6XapiModuleFree(module);
        }
        return 1;
    }
    xl6XapiDumpJsonModule(&out, module);
    if ( !xl6CliOutputClose(&out, xapiPath) ) {
        if ( outModule == NULL ) {
            xl6XapiModuleFree(module);
        }
        return 1;
    }

    if ( outModule == NULL ) {
        xl6XapiModuleFree(module);
    }
    return 0;
}

static int xl6CliHeaderToFile(const char* path, const char* headerPath)
{
    XL6_CompileUnit unit;
    int rc;

    if ( headerPath == NULL ) {
        return 0;
    }
    if ( !xl6CliLoadCheckedUnit(&unit, path) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }
    rc = xl6CliHeaderUnitToFile(&unit, headerPath);
    xl6CompileUnitFree(&unit);
    return rc;
}

static int xl6CliCgenToFile(const char* path, const char* cPath)
{
    XL6_CompileUnit unit;
    int rc;

    if ( !xl6CliLoadCheckedUnit(&unit, path) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }
    rc = xl6CliCgenUnitToFile(&unit, cPath);
    xl6CompileUnitFree(&unit);
    return rc;
}

static int xl6CliParseCommand(const char* path, int dumpAst, int runSem, const char* emitPath, int jsonOutput)
{
    XL6_ParseResult result;
    XL6_Output out;
    int ok;

    xl6ParseResultInit(&result);
    ok = xl6ParseFile(path, &result);
    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    if ( runSem ) {
        ok = xl6SemCheckModule(result.root, &result.diags);
        if ( !ok ) {
            xl6DiagPrintText(&result.diags, path);
            xl6ParseResultFree(&result);
            return 1;
        }
    }

    if ( dumpAst ) {
        if ( !xl6CliOutputOpen(&out, emitPath) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot open emit file: %s\n", emitPath);
            xl6ParseResultFree(&result);
            return 1;
        }
        if ( jsonOutput ) {
            xl6AstDumpJson(&out, result.root);
        } else {
            xl6AstDumpTextFile(&out, result.root, 0);
        }
        if ( !xl6CliOutputClose(&out, emitPath) ) {
            xl6ParseResultFree(&result);
            return 1;
        }
    } else {
        xl6ConsolePuts(XL6_CONSOLE_STDOUT, "ok\n");
    }

    xl6ParseResultFree(&result);
    return 0;
}

static int xl6CliCheckCommand(const char* path, int jsonOutput)
{
    XL6_CompileUnit unit;
    XL6_Output out;
    int success;

    xl6CompileUnitInit(&unit);
    success = xl6CompileUnitLoadFile(&unit, path, 1);

    if ( jsonOutput ) {
        xl6OutputInitStdFile(&out, stdout);
        xl6DiagPrintJson(&out, &unit.parse.diags, path, unit.phase, success);
        xl6OutputClose(&out);
    } else if ( success ) {
        xl6ConsolePuts(XL6_CONSOLE_STDOUT, "ok\n");
    } else {
        xl6DiagPrintTextWithPhase(&unit.parse.diags, path, unit.phase);
    }

    xl6CompileUnitFree(&unit);
    return success ? 0 : 1;
}

static int xl6CliLexCommand(const char* path, const char* emitPath, int jsonOutput)
{
    XL6_Output out;
    int rc;

    if ( !xl6CliOutputOpen(&out, emitPath) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot open emit file: %s\n", emitPath);
        return 1;
    }
    rc = xl6LexerDumpFileTo(&out, path, jsonOutput);
    if ( !xl6CliOutputClose(&out, emitPath) ) {
        return 1;
    }
    return rc;
}

static int xl6CliIrCommand(const char* path, const char* emitPath, int jsonOutput)
{
    XL6_CompileUnit unit;
    XL6_Output out;

    if ( !xl6CliLoadCheckedUnit(&unit, path) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }

    if ( !xl6CliOutputOpen(&out, emitPath) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot open emit file: %s\n", emitPath);
        xl6CompileUnitFree(&unit);
        return 1;
    }

    if ( jsonOutput ) {
        xl6IrDumpJson(&out, unit.parse.root);
    } else {
        xl6IrDumpText(&out, unit.parse.root);
    }

    if ( !xl6CliOutputClose(&out, emitPath) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }
    xl6CompileUnitFree(&unit);
    return 0;
}

static int xl6CliCgenDumpDirCommand(const char* path, const char* dumpDir);

static int xl6CliCgenCommand(const char* path, const char* emitPath, const char* emitHeaderPath, const char* dumpDir)
{
    XL6_CompileUnit unit;
    XL6_Output out;
    int ok;
    int rc;

    if ( dumpDir != NULL ) {
        rc = xl6CliCgenDumpDirCommand(path, dumpDir);
        if ( rc != 0 || (emitPath == NULL && emitHeaderPath == NULL) ) {
            return rc;
        }
    }

    if ( !xl6CliLoadCheckedUnit(&unit, path) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }

    if ( !xl6CliOutputOpen(&out, emitPath) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot open emit file: %s\n", emitPath);
        xl6CompileUnitFree(&unit);
        return 1;
    }

    ok = xl6CgenEmitC(&out, unit.parse.root, &unit.parse.diags);

    if ( !xl6CliOutputClose(&out, emitPath) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }
    if ( !ok ) {
        xl6DiagPrintText(&unit.parse.diags, path);
        xl6CompileUnitFree(&unit);
        return 1;
    }

    rc = xl6CliHeaderUnitToFile(&unit, emitHeaderPath);
    if ( rc != 0 ) {
        xl6CompileUnitFree(&unit);
        return rc;
    }

    xl6CompileUnitFree(&unit);
    return 0;
}

static int xl6CliAbiCommand(const char* path, const char* emitPath, int jsonOutput)
{
    XL6_CompileUnit unit;
    XL6_Output out;

    if ( !xl6CliLoadCheckedUnit(&unit, path) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }

    if ( !xl6CliOutputOpen(&out, emitPath) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot open emit file: %s\n", emitPath);
        xl6CompileUnitFree(&unit);
        return 1;
    }

    if ( jsonOutput ) {
        xl6AbiDumpJson(&out, unit.parse.root);
    } else {
        xl6AbiDumpText(&out, unit.parse.root);
    }

    if ( !xl6CliOutputClose(&out, emitPath) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }
    xl6CompileUnitFree(&unit);
    return 0;
}

static int xl6CliXapiCommand(const char* path, const char* emitPath)
{
    XL6_CompileUnit unit;
    int rc;

    if ( !xl6CliLoadCheckedUnit(&unit, path) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }
    rc = xl6CliXapiUnitToFile(&unit, emitPath, NULL);
    xl6CompileUnitFree(&unit);
    return rc;
}

static int xl6CliCgenDumpDirCommand(const char* path, const char* dumpDir)
{
    char base[256];
    char tokensPath[512];
    char astPath[512];
    char irPath[512];
    char abiPath[512];
    char xapiPath[512];
    char cPath[512];
    char headerPath[512];
    XL6_CompileUnit unit;
    XL6_Output out;
    int rc;

    if ( xl6CliMakeDir(dumpDir) != 0 ) {
        return 1;
    }

    xl6CliBaseNameNoExt(base, sizeof(base), path);
    if ( !xl6CliJoinDumpPath(tokensPath, sizeof(tokensPath), dumpDir, base, ".tokens.json") ||
         !xl6CliJoinDumpPath(astPath, sizeof(astPath), dumpDir, base, ".ast.json") ||
         !xl6CliJoinDumpPath(irPath, sizeof(irPath), dumpDir, base, ".xlir") ||
         !xl6CliJoinDumpPath(abiPath, sizeof(abiPath), dumpDir, base, ".abi.json") ||
         !xl6CliJoinDumpPath(xapiPath, sizeof(xapiPath), dumpDir, base, ".xapi.json") ||
         !xl6CliJoinDumpPath(cPath, sizeof(cPath), dumpDir, base, ".c") ||
         !xl6CliJoinDumpPath(headerPath, sizeof(headerPath), dumpDir, base, ".h") ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "dump path is too long\n");
        return 1;
    }

    rc = xl6CliLexCommand(path, tokensPath, 1);
    if ( rc != 0 ) {
        return rc;
    }

    if ( !xl6CliLoadCheckedUnit(&unit, path) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }

    if ( !xl6OutputOpenFile(&out, astPath) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot open emit file: %s\n", astPath);
        xl6CompileUnitFree(&unit);
        return 1;
    }
    xl6AstDumpJson(&out, unit.parse.root);
    if ( !xl6CliOutputClose(&out, astPath) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }

    if ( !xl6OutputOpenFile(&out, irPath) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot open emit file: %s\n", irPath);
        xl6CompileUnitFree(&unit);
        return 1;
    }
    xl6IrDumpText(&out, unit.parse.root);
    if ( !xl6CliOutputClose(&out, irPath) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }

    if ( !xl6OutputOpenFile(&out, abiPath) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "cannot open emit file: %s\n", abiPath);
        xl6CompileUnitFree(&unit);
        return 1;
    }
    xl6AbiDumpJson(&out, unit.parse.root);
    if ( !xl6CliOutputClose(&out, abiPath) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }

    rc = xl6CliXapiUnitToFile(&unit, xapiPath, NULL);
    if ( rc != 0 ) {
        xl6CompileUnitFree(&unit);
        return rc;
    }
    rc = xl6CliHeaderUnitToFile(&unit, headerPath);
    if ( rc != 0 ) {
        xl6CompileUnitFree(&unit);
        return rc;
    }
    rc = xl6CliCgenUnitToFile(&unit, cPath);
    xl6CompileUnitFree(&unit);
    return rc;
}

static int xl6CliBuildCommand(const char* outputPath, const char* emitHeaderPath, int argc, char** argv)
{
    const char* xlPath;
    const char* cSources[32];
    const char* cInputs[128];
    char wrapperPaths[32][512];
    char workDir[512];
    char cPath[512];
    char headerPath[512];
    char xapiPath[512];
    char loadlibDefPaths[32][512];
    XL6_CompileUnit unit;
    XL6_XapiModule xapi;
    int cSourceCount;
    int cInputCount;
    int wrapperPathCount;
    int loadlibDefCount;
    int rc;
    int i;
    int saveTemps;
    int workDirReady;
    int unitReady;
    int xapiReady;
    size_t depIndex;

    workDir[0] = '\0';
    cPath[0] = '\0';
    headerPath[0] = '\0';
    xapiPath[0] = '\0';
    wrapperPathCount = 0;
    loadlibDefCount = 0;
    workDirReady = 0;
    unitReady = 0;
    xapiReady = 0;
    rc = 1;

    if ( outputPath == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "build requires -o <output.exe>\n");
        return 1;
    }
    if ( !xl6CliCollectBuildInputs(argc, argv, &xlPath, cSources, 32, &cSourceCount) ) {
        return 1;
    }
    saveTemps = xl6CliHasSaveTempsFlag(argc, argv);

    if ( !xl6CliMakeWorkDir(workDir, sizeof(workDir), "build") ) {
        return 1;
    }
    workDirReady = 1;
    if ( !xl6CliJoinPath(cPath, sizeof(cPath), workDir, "module.c") ||
         !xl6CliJoinPath(headerPath, sizeof(headerPath), workDir, "module.h") ||
         !xl6CliJoinPath(xapiPath, sizeof(xapiPath), workDir, "module.xapi.json") ) {
        rc = 1;
        goto cleanup;
    }

    unitReady = 1;
    if ( !xl6CliLoadCheckedUnit(&unit, xlPath) ) {
        rc = 1;
        goto cleanup;
    }

    rc = xl6CliHeaderUnitToFile(&unit, headerPath);
    if ( rc != 0 ) {
        goto cleanup;
    }

    xl6XapiModuleInit(&xapi);
    xapiReady = 1;
    rc = xl6CliXapiUnitToFile(&unit, xapiPath, &xapi);
    if ( rc != 0 ) {
        goto cleanup;
    }
    if ( xl6CliHasSharedFlag(argc, argv) && xl6XapiModuleHasExecutableEntry(&xapi) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "shared build cannot contain xlang main or top-level executable statements\n");
        rc = 1;
        goto cleanup;
    }
    if ( emitHeaderPath != NULL ) {
        rc = xl6CliHeaderUnitToFile(&unit, emitHeaderPath);
        if ( rc != 0 ) {
            goto cleanup;
        }
    }
    rc = xl6CliCgenUnitToFile(&unit, cPath);
    if ( rc != 0 ) {
        goto cleanup;
    }

    cInputCount = 0;
    cInputs[cInputCount++] = cPath;
    for ( depIndex = 0; depIndex < unit.importDeps.cSources.count; ++depIndex ) {
        if ( cInputCount >= (int)XL6_ARRAY_COUNT(cInputs) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many import C source files\n");
            rc = 1;
            goto cleanup;
        }
        cInputs[cInputCount++] = unit.importDeps.cSources.items[depIndex];
    }
    for ( i = 0; i < cSourceCount; ++i ) {
        char wrapperName[64];
        int written = snprintf(wrapperName, sizeof(wrapperName), "csrc_%d.c", i);
        if ( written <= 0 || (size_t)written >= sizeof(wrapperName) ||
             !xl6CliJoinPath(wrapperPaths[i], sizeof(wrapperPaths[i]), workDir, wrapperName) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "C wrapper path is too long\n");
            rc = 1;
            goto cleanup;
        }
        wrapperPathCount = i + 1;
        rc = xl6CliWriteCWrapper(wrapperPaths[i], cSources[i], headerPath);
        if ( rc != 0 ) {
            goto cleanup;
        }
        if ( cInputCount >= (int)XL6_ARRAY_COUNT(cInputs) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many C source files\n");
            rc = 1;
            goto cleanup;
        }
        cInputs[cInputCount++] = wrapperPaths[i];
    }

    rc = xl6CliCompileWithTccInputs(cInputs, cInputCount, outputPath, argc, argv, workDir, &unit.importDeps, loadlibDefPaths, 32, &loadlibDefCount);
    if ( rc != 0 ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "tcc build failed\n");
        rc = 1;
        goto cleanup;
    }
    rc = 0;

cleanup:
    if ( xapiReady ) {
        xl6XapiModuleFree(&xapi);
    }
    if ( unitReady ) {
        xl6CompileUnitFree(&unit);
    }
    if ( workDirReady ) {
        const char* tempFiles[96];
        int tempCount = 0;

        tempFiles[tempCount++] = cPath;
        tempFiles[tempCount++] = headerPath;
        tempFiles[tempCount++] = xapiPath;
        for ( i = 0; i < wrapperPathCount; ++i ) {
            tempFiles[tempCount++] = wrapperPaths[i];
        }
        for ( i = 0; i < loadlibDefCount; ++i ) {
            tempFiles[tempCount++] = loadlibDefPaths[i];
        }
        xl6CliCleanupWorkDir(workDir, tempFiles, tempCount, saveTemps);
    }
    return rc;
}

static int xl6CliRunCommand(const char* path, int argc, char** argv)
{
    XL6_CliProcessArgs runArgs;
    char workDir[512];
    char cPath[512];
    char exePath[512];
    char pathEnv[8192];
    char* envItems[1];
    char loadlibDefPaths[32][512];
    const char* cInputs[64];
    XL6_CompileUnit unit;
    int rc;
    int loadlibDefCount;
    int cInputCount;
    int i;
    int argStart;
    int envCount;
    int saveTemps;
    int workDirReady;
    int unitReady;
    size_t depIndex;

    xl6CliProcessArgsInit(&runArgs);
    workDir[0] = '\0';
    cPath[0] = '\0';
    exePath[0] = '\0';
    loadlibDefCount = 0;
    workDirReady = 0;
    unitReady = 0;
    rc = 1;

    saveTemps = xl6CliHasSaveTempsFlag(argc, argv);
    if ( !xl6CliMakeWorkDir(workDir, sizeof(workDir), "run") ) {
        return 1;
    }
    workDirReady = 1;
    if ( !xl6CliJoinPath(cPath, sizeof(cPath), workDir, "module.c") ||
         !xl6CliJoinPath(exePath, sizeof(exePath), workDir, "app.exe") ) {
        rc = 1;
        goto cleanup;
    }
    unitReady = 1;
    if ( !xl6CliLoadCheckedUnit(&unit, path) ) {
        rc = 1;
        goto cleanup;
    }
    rc = xl6CliCgenUnitToFile(&unit, cPath);
    if ( rc != 0 ) {
        goto cleanup;
    }
    cInputCount = 0;
    cInputs[cInputCount++] = cPath;
    for ( depIndex = 0; depIndex < unit.importDeps.cSources.count; ++depIndex ) {
        if ( cInputCount >= (int)XL6_ARRAY_COUNT(cInputs) ) {
            xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many import C source files\n");
            rc = 1;
            goto cleanup;
        }
        cInputs[cInputCount++] = unit.importDeps.cSources.items[depIndex];
    }
    rc = xl6CliCompileWithTccInputs(cInputs, cInputCount, exePath, argc, argv, workDir, &unit.importDeps, loadlibDefPaths, 32, &loadlibDefCount);
    if ( rc != 0 ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "tcc run build failed\n");
        rc = 1;
        goto cleanup;
    }

    argStart = 0;
    for ( i = 3; i < argc; ++i ) {
        if ( strcmp(argv[i], "--") == 0 ) {
            argStart = i + 1;
            break;
        }
    }
    if ( argStart > 0 ) {
        for ( i = argStart; i < argc; ++i ) {
            if ( !xl6CliProcessArgsAddBorrow(&runArgs, argv[i]) ) {
                xl6ConsolePrintf(XL6_CONSOLE_STDERR, "too many program arguments\n");
                rc = 1;
                goto cleanup;
            }
        }
    }
    if ( !xl6CliBuildRunPathEnv(pathEnv, sizeof(pathEnv), argc, argv, &unit.importDeps, envItems, &envCount) ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "run PATH environment is too long\n");
        rc = 1;
        goto cleanup;
    }

    rc = xl6CliRunProcess(exePath, &runArgs, envItems, envCount);
    if ( rc != 0 ) {
        rc = 1;
        goto cleanup;
    }
    rc = 0;

cleanup:
    xl6CliProcessArgsFree(&runArgs);
    if ( unitReady ) {
        xl6CompileUnitFree(&unit);
    }
    if ( workDirReady ) {
        const char* tempFiles[34];
        int tempCount;

        tempCount = 0;
        tempFiles[tempCount++] = cPath;
        tempFiles[tempCount++] = exePath;
        for ( i = 0; i < loadlibDefCount; ++i ) {
            tempFiles[tempCount++] = loadlibDefPaths[i];
        }
        xl6CliCleanupWorkDir(workDir, tempFiles, tempCount, saveTemps);
    }
    return rc;
}

int xl6CliMain(int argc, char** argv)
{
    const char* cmd;
    const char* file;

    if ( argc < 2 ) {
        xl6CliPrintHelp();
        return 0;
    }

    cmd = argv[1];
    if ( strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0 ) {
        xl6CliPrintHelp();
        return 0;
    }

    file = xl6CliFileArg(argc, argv);
    if ( file == NULL ) {
        xl6ConsolePrintf(XL6_CONSOLE_STDERR, "missing file argument\n");
        return 1;
    }

    if ( strcmp(cmd, "lex") == 0 ) {
        return xl6CliLexCommand(file, xl6CliFindEmitPath(argc, argv), xl6CliHasJson(argc, argv));
    }
    if ( strcmp(cmd, "parse") == 0 ) {
        return xl6CliParseCommand(file, 1, 0, xl6CliFindEmitPath(argc, argv), xl6CliHasJson(argc, argv));
    }
    if ( strcmp(cmd, "check") == 0 ) {
        return xl6CliCheckCommand(file, xl6CliHasJson(argc, argv));
    }
    if ( strcmp(cmd, "ir") == 0 ) {
        return xl6CliIrCommand(file, xl6CliFindEmitPath(argc, argv), xl6CliHasJson(argc, argv));
    }

    if ( strcmp(cmd, "cgen") == 0 ) {
        return xl6CliCgenCommand(file, xl6CliFindEmitPath(argc, argv), xl6CliFindEmitHeaderPath(argc, argv), xl6CliFindDumpDir(argc, argv));
    }

    if ( strcmp(cmd, "run") == 0 ) {
        return xl6CliRunCommand(file, argc, argv);
    }
    if ( strcmp(cmd, "build") == 0 ) {
        return xl6CliBuildCommand(xl6CliFindOutputPath(argc, argv), xl6CliFindEmitHeaderPath(argc, argv), argc, argv);
    }

    if ( strcmp(cmd, "abi") == 0 ) {
        return xl6CliAbiCommand(file, xl6CliFindEmitPath(argc, argv), xl6CliHasJson(argc, argv));
    }

    if ( strcmp(cmd, "xapi") == 0 ) {
        return xl6CliXapiCommand(file, xl6CliFindEmitPath(argc, argv));
    }

    xl6ConsolePrintf(XL6_CONSOLE_STDERR, "unknown command: %s\n", cmd);
    return 1;
}
