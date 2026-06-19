#include "src/driver/xl6_cli.h"

#include "src/abi/xl6_abi_dump.h"
#include "src/abi/xl6_xapi_dump.h"
#include "src/ast/xl6_ast_dump.h"
#include "src/backend/xl6_build_backend.h"
#include "src/cgen/xl6_cgen.h"
#include "src/common/xl6_common.h"
#include "src/compile/xl6_compile_unit.h"
#include "src/ir/xl6_ir_dump.h"
#include "src/lexer/xl6_lexer_api.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

static void xl6CliPrintHelp(void)
{
    printf("Usage:\n");
    printf("  xl lex <file.xl> [--format text|json]\n");
    printf("  xl parse <file.xl> [--format tree|json] [--emit output.ast]\n");
    printf("  xl check <file.xl> [--json]\n");
    printf("  xl ir <file.xl> [--format text|json] [--emit output.xlir]\n");
    printf("  xl cgen <file.xl> [--emit output.c] [--emit-header output.h] [--dump-dir dir]\n");
    printf("  xl abi <file.xl> [--json] [--emit output.abi.txt]\n");
    printf("  xl xapi <file.xl> [--emit output.xapi.json]\n");
    printf("  xl run <file.xl> [--save-temps] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll] [-- <args>...]\n");
    printf("  xl build <file.xl> -o <output.exe> [-shared] [--save-temps] [--emit-header output.h] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll]\n");
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
        strcpy(out, "XL6_");
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
        fprintf(stderr, "dump dir is empty\n");
        return 1;
    }
    if ( xrtDirCreateAll((char*)path) ) {
        return 0;
    }
    fprintf(stderr, "cannot create dump dir: %s\n", path);
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
            fprintf(stderr, "cannot write output file: %s (%s)\n", path, xl6OutputError(out));
        } else {
            fprintf(stderr, "cannot write output (%s)\n", xl6OutputError(out));
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
                fprintf(stderr, "build currently accepts one .xl module; use import for additional modules later\n");
                return 0;
            }
            *outXlPath = arg;
        } else if ( xl6CliEndsWithExt(arg, ".c") ) {
            if ( cCount >= maxCSources ) {
                fprintf(stderr, "too many C source files\n");
                return 0;
            }
            cSources[cCount++] = arg;
        } else {
            fprintf(stderr, "unsupported build input: %s\n", arg);
            return 0;
        }
    }
    if ( *outXlPath == NULL ) {
        fprintf(stderr, "build requires one .xl input\n");
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
        fprintf(stderr, "cannot write C wrapper: %s\n", wrapperPath);
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
    (void)system("if not exist release\\tmp mkdir release\\tmp >nul 2>nul");
    return 1;
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

static int xl6CliMakeWorkDir(char* out, size_t outSize, const char* purpose)
{
    int written;

    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    xl6CliEnsureTmpDir();
    written = snprintf(
        out,
        outSize,
        "release\\tmp\\xl6_%s_%lu_%lu_%lu",
        purpose != NULL ? purpose : "work",
        (unsigned long)time(NULL),
        xl6CliProcessId(),
        xl6CliNextTempId());
    if ( written <= 0 || (size_t)written >= outSize ) {
        fprintf(stderr, "temporary directory path is too long\n");
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
        fprintf(stderr, "temporary files kept: %s\n", workDir != NULL ? workDir : "<tmp>");
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

static int xl6CliQuoteCommand(char* out, size_t outSize, const char* text)
{
    int written;

    written = snprintf(out, outSize, "\"%s\"", text != NULL ? text : "");
    return written > 0 && (size_t)written < outSize;
}

static int xl6CliAppendCommandText(char* command, size_t commandSize, const char* text);

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

static int xl6CliAppendRunLoadlibPathPrefix(char* command, size_t commandSize, int argc, char** argv)
{
    int i;
    int started;

    started = 0;
    for ( i = 3; i < argc; ++i ) {
        if ( strcmp(argv[i], "--") == 0 ) {
            break;
        }
        if ( strcmp(argv[i], "--loadlib") == 0 && (i + 1) < argc ) {
            char dir[512];
            xl6CliDirName(dir, sizeof(dir), argv[i + 1]);
            if ( !started ) {
                if ( !xl6CliAppendCommandText(command, commandSize, "set \"PATH=") ) {
                    return 0;
                }
                started = 1;
            } else {
                if ( !xl6CliAppendCommandText(command, commandSize, ";") ) {
                    return 0;
                }
            }
            if ( !xl6CliAppendCommandText(command, commandSize, dir) ) {
                return 0;
            }
            i += 1;
        } else if ( xl6CliOptionTakesValue(argv[i]) ) {
            i += 1;
        }
    }
    if ( started ) {
        return xl6CliAppendCommandText(command, commandSize, ";%PATH%\" && ");
    }
    return 1;
}

static int xl6CliAppendCommandText(char* command, size_t commandSize, const char* text)
{
    size_t used;
    size_t add;

    if ( command == NULL || text == NULL ) {
        return 0;
    }
    used = strlen(command);
    add = strlen(text);
    if ( used + add + 1 >= commandSize ) {
        return 0;
    }
    strcat(command, text);
    return 1;
}

static int xl6CliAppendPathOption(char* command, size_t commandSize, const char* flag, const char* path)
{
    char quoted[512];

    if ( !xl6CliQuoteCommand(quoted, sizeof(quoted), path) ) {
        return 0;
    }
    return xl6CliAppendCommandText(command, commandSize, " ") &&
           xl6CliAppendCommandText(command, commandSize, flag) &&
           xl6CliAppendCommandText(command, commandSize, quoted);
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

static int xl6CliAppendLinkOption(char* command, size_t commandSize, const char* value)
{
    char quoted[512];

    if ( xl6CliLooksLikeLinkFile(value) ) {
        if ( !xl6CliQuoteCommand(quoted, sizeof(quoted), value) ) {
            return 0;
        }
        return xl6CliAppendCommandText(command, commandSize, " ") &&
               xl6CliAppendCommandText(command, commandSize, quoted);
    }
    return xl6CliAppendCommandText(command, commandSize, " -l") &&
           xl6CliAppendCommandText(command, commandSize, value != NULL ? value : "");
}

static int xl6CliMakeLoadlibDef(const char* dllPath, int index, const char* workDir, char* outPath, size_t outSize)
{
    char quotedDll[512];
    char quotedDef[512];
    char command[1400];
    int written;
    int rc;
    const XL6_BuildBackend* backend;

    if ( dllPath == NULL || dllPath[0] == '\0' || outPath == NULL || outSize == 0 ) {
        return 0;
    }

    if ( workDir != NULL && workDir[0] != '\0' ) {
        char defName[64];
        written = snprintf(defName, sizeof(defName), "xl6_loadlib_%d.def", index);
        if ( written <= 0 || (size_t)written >= sizeof(defName) ||
             !xl6CliJoinPath(outPath, outSize, workDir, defName) ) {
            fprintf(stderr, "loadlib def path is too long\n");
            return 0;
        }
    } else {
        xl6CliEnsureTmpDir();
        written = snprintf(outPath, outSize, "release\\tmp\\xl6_loadlib_%d.def", index);
        if ( written <= 0 || (size_t)written >= outSize ) {
            fprintf(stderr, "loadlib def path is too long\n");
            return 0;
        }
    }

    if ( !xl6CliQuoteCommand(quotedDll, sizeof(quotedDll), dllPath) ||
         !xl6CliQuoteCommand(quotedDef, sizeof(quotedDef), outPath) ) {
        fprintf(stderr, "loadlib path is too long\n");
        return 0;
    }

    backend = xl6CliDefaultBackend();
    written = snprintf(command, sizeof(command),
                       "%s -impdef %s -o %s",
                       xl6BuildBackendCcPath(backend),
                       quotedDll,
                       quotedDef);
    if ( written <= 0 || (size_t)written >= sizeof(command) ) {
        fprintf(stderr, "loadlib impdef command is too long\n");
        return 0;
    }

    rc = system(command);
    if ( rc != 0 ) {
        fprintf(stderr, "loadlib import generation failed: %s\n", dllPath);
        return 0;
    }
    return 1;
}

static int xl6CliAppendTccOptions(
    char* command,
    size_t commandSize,
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
            if ( !xl6CliAppendPathOption(command, commandSize, "-I", argv[i + 1]) ) {
                return 0;
            }
            i += 1;
        } else if ( strcmp(argv[i], "--lib-path") == 0 && (i + 1) < argc ) {
            if ( !xl6CliAppendPathOption(command, commandSize, "-L", argv[i + 1]) ) {
                return 0;
            }
            i += 1;
        } else if ( strcmp(argv[i], "--link") == 0 && (i + 1) < argc ) {
            if ( !xl6CliAppendLinkOption(command, commandSize, argv[i + 1]) ) {
                return 0;
            }
            i += 1;
        } else if ( strcmp(argv[i], "--loadlib") == 0 && (i + 1) < argc ) {
            char defPath[512];
            if ( loadlibIndex >= maxLoadlibDefs ) {
                fprintf(stderr, "too many loadlib files\n");
                return 0;
            }
            if ( !xl6CliMakeLoadlibDef(argv[i + 1], loadlibIndex, workDir, defPath, sizeof(defPath)) ||
                 !xl6CliAppendLinkOption(command, commandSize, defPath) ) {
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
            if ( !xl6CliAppendCommandText(command, commandSize, " -shared -DXL6_BUILD_SHARED") ) {
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

static int xl6CliAppendDefaultXrtLinks(char* command, size_t commandSize, int argc, char** argv)
{
    if ( !xl6CliAppendCommandText(command, commandSize, " -DXL6_USE_XRT_LIFECYCLE") ) {
        return 0;
    }
    if ( !xl6CliHasUserLink(argc, argv, xl6CliIsXrtLinkValue) ) {
        if ( xrtFileExists("..\\..\\xrt\\release\\x64\\xrt.o") ) {
            if ( !xl6CliAppendLinkOption(command, commandSize, "..\\..\\xrt\\release\\x64\\xrt.o") ) {
                return 0;
            }
        } else {
            fprintf(stderr, "missing XRT object: ..\\..\\xrt\\release\\x64\\xrt.o\n");
            return 0;
        }
    }
    if ( !xl6CliHasUserLink(argc, argv, xl6CliIsTccRuntimeLinkValue) &&
         xrtFileExists("..\\demo5\\release\\tcc\\lib\\libtcc1-64.a") ) {
        if ( !xl6CliAppendLinkOption(command, commandSize, "..\\demo5\\release\\tcc\\lib\\libtcc1-64.a") ) {
            return 0;
        }
    }
    return xl6CliAppendLinkOption(command, commandSize, "Ws2_32") &&
           xl6CliAppendLinkOption(command, commandSize, "IPHLPAPI") &&
           xl6CliAppendLinkOption(command, commandSize, "Shell32");
}

static int xl6CliCompileWithTccInputs(
    const char** cPaths,
    int cPathCount,
    const char* exePath,
    int argc,
    char** argv,
    const char* workDir,
    char loadlibDefPaths[][512],
    int maxLoadlibDefs,
    int* outLoadlibDefCount)
{
    char quotedExe[512];
    char command[8192];
    int written;
    int i;
    const XL6_BuildBackend* backend;

    if ( cPaths == NULL || cPathCount <= 0 ) {
        fprintf(stderr, "no C input for tcc\n");
        return 1;
    }
    if ( !xl6CliQuoteCommand(quotedExe, sizeof(quotedExe), exePath) ) {
        fprintf(stderr, "command path is too long\n");
        return 1;
    }

    backend = xl6CliDefaultBackend();
    written = snprintf(command, sizeof(command), "%s", xl6BuildBackendCommandPrefix(backend));
    if ( written <= 0 || (size_t)written >= sizeof(command) ) {
        fprintf(stderr, "tcc command is too long\n");
        return 1;
    }
    for ( i = 0; i < cPathCount; ++i ) {
        char quotedC[512];

        if ( !xl6CliQuoteCommand(quotedC, sizeof(quotedC), cPaths[i]) ||
             !xl6CliAppendCommandText(command, sizeof(command), " ") ||
             !xl6CliAppendCommandText(command, sizeof(command), quotedC) ) {
            fprintf(stderr, "tcc source path is too long\n");
            return 1;
        }
    }
    if ( !xl6CliAppendDefaultXrtLinks(command, sizeof(command), argc, argv) ||
         !xl6CliAppendCommandText(command, sizeof(command), " -o ") ||
         !xl6CliAppendCommandText(command, sizeof(command), quotedExe) ) {
        fprintf(stderr, "tcc command is too long\n");
        return 1;
    }
    if ( !xl6CliAppendTccOptions(command, sizeof(command), argc, argv, workDir, loadlibDefPaths, maxLoadlibDefs, outLoadlibDefCount) ) {
        fprintf(stderr, "tcc options are too long\n");
        return 1;
    }
    return system(command);
}

static int xl6CliCompileWithTcc(
    const char* cPath,
    const char* exePath,
    int argc,
    char** argv,
    const char* workDir,
    char loadlibDefPaths[][512],
    int maxLoadlibDefs,
    int* outLoadlibDefCount)
{
    const char* inputs[1];

    inputs[0] = cPath;
    return xl6CliCompileWithTccInputs(inputs, 1, exePath, argc, argv, workDir, loadlibDefPaths, maxLoadlibDefs, outLoadlibDefCount);
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
        fprintf(stderr, "cannot open header output file: %s\n", headerPath);
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
        fprintf(stderr, "cannot open c output file: %s\n", cPath);
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
        fprintf(stderr, "cannot build XAPI metadata\n");
        if ( outModule == NULL ) {
            xl6XapiModuleFree(module);
        }
        return 1;
    }

    if ( !xl6CliOutputOpen(&out, xapiPath) ) {
        fprintf(stderr, "cannot open emit file: %s\n", xapiPath);
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
            fprintf(stderr, "cannot open emit file: %s\n", emitPath);
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
        printf("ok\n");
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
        printf("ok\n");
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
        fprintf(stderr, "cannot open emit file: %s\n", emitPath);
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
        fprintf(stderr, "cannot open emit file: %s\n", emitPath);
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
        fprintf(stderr, "cannot open emit file: %s\n", emitPath);
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
        fprintf(stderr, "cannot open emit file: %s\n", emitPath);
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
        fprintf(stderr, "dump path is too long\n");
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
        fprintf(stderr, "cannot open emit file: %s\n", astPath);
        xl6CompileUnitFree(&unit);
        return 1;
    }
    xl6AstDumpJson(&out, unit.parse.root);
    if ( !xl6CliOutputClose(&out, astPath) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }

    if ( !xl6OutputOpenFile(&out, irPath) ) {
        fprintf(stderr, "cannot open emit file: %s\n", irPath);
        xl6CompileUnitFree(&unit);
        return 1;
    }
    xl6IrDumpText(&out, unit.parse.root);
    if ( !xl6CliOutputClose(&out, irPath) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }

    if ( !xl6OutputOpenFile(&out, abiPath) ) {
        fprintf(stderr, "cannot open emit file: %s\n", abiPath);
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
    const char* cInputs[34];
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
    int loadlibDefCount;
    int rc;
    int i;
    int saveTemps;

    if ( outputPath == NULL ) {
        fprintf(stderr, "build requires -o <output.exe>\n");
        return 1;
    }
    if ( !xl6CliCollectBuildInputs(argc, argv, &xlPath, cSources, 32, &cSourceCount) ) {
        return 1;
    }
    saveTemps = xl6CliHasSaveTempsFlag(argc, argv);

    if ( !xl6CliMakeWorkDir(workDir, sizeof(workDir), "build") ||
         !xl6CliJoinPath(cPath, sizeof(cPath), workDir, "module.c") ||
         !xl6CliJoinPath(headerPath, sizeof(headerPath), workDir, "module.h") ||
         !xl6CliJoinPath(xapiPath, sizeof(xapiPath), workDir, "module.xapi.json") ) {
        return 1;
    }

    if ( !xl6CliLoadCheckedUnit(&unit, xlPath) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }

    rc = xl6CliHeaderUnitToFile(&unit, headerPath);
    if ( rc != 0 ) {
        xl6CompileUnitFree(&unit);
        return rc;
    }

    xl6XapiModuleInit(&xapi);
    rc = xl6CliXapiUnitToFile(&unit, xapiPath, &xapi);
    if ( rc != 0 ) {
        xl6XapiModuleFree(&xapi);
        xl6CompileUnitFree(&unit);
        return rc;
    }
    if ( xl6CliHasSharedFlag(argc, argv) && xl6XapiModuleHasExecutableEntry(&xapi) ) {
        fprintf(stderr, "shared build cannot contain xlang main or top-level executable statements\n");
        xl6XapiModuleFree(&xapi);
        xl6CompileUnitFree(&unit);
        return 1;
    }
    if ( emitHeaderPath != NULL ) {
        rc = xl6CliHeaderUnitToFile(&unit, emitHeaderPath);
        if ( rc != 0 ) {
            xl6XapiModuleFree(&xapi);
            xl6CompileUnitFree(&unit);
            return rc;
        }
    }
    rc = xl6CliCgenUnitToFile(&unit, cPath);
    if ( rc != 0 ) {
        xl6XapiModuleFree(&xapi);
        xl6CompileUnitFree(&unit);
        return rc;
    }

    cInputCount = 0;
    cInputs[cInputCount++] = cPath;
    for ( i = 0; i < cSourceCount; ++i ) {
        char wrapperName[64];
        int written = snprintf(wrapperName, sizeof(wrapperName), "csrc_%d.c", i);
        if ( written <= 0 || (size_t)written >= sizeof(wrapperName) ||
             !xl6CliJoinPath(wrapperPaths[i], sizeof(wrapperPaths[i]), workDir, wrapperName) ) {
            fprintf(stderr, "C wrapper path is too long\n");
            xl6XapiModuleFree(&xapi);
            xl6CompileUnitFree(&unit);
            return 1;
        }
        rc = xl6CliWriteCWrapper(wrapperPaths[i], cSources[i], headerPath);
        if ( rc != 0 ) {
            xl6XapiModuleFree(&xapi);
            xl6CompileUnitFree(&unit);
            return rc;
        }
        cInputs[cInputCount++] = wrapperPaths[i];
    }

    loadlibDefCount = 0;
    rc = xl6CliCompileWithTccInputs(cInputs, cInputCount, outputPath, argc, argv, workDir, loadlibDefPaths, 32, &loadlibDefCount);
    if ( rc != 0 ) {
        fprintf(stderr, "tcc build failed\n");
        xl6XapiModuleFree(&xapi);
        xl6CompileUnitFree(&unit);
        return 1;
    }
    xl6XapiModuleFree(&xapi);
    xl6CompileUnitFree(&unit);
    {
        const char* tempFiles[35];
        int tempCount = 0;

        tempFiles[tempCount++] = cPath;
        tempFiles[tempCount++] = headerPath;
        tempFiles[tempCount++] = xapiPath;
        for ( i = 0; i < cSourceCount; ++i ) {
            tempFiles[tempCount++] = wrapperPaths[i];
        }
        for ( i = 0; i < loadlibDefCount; ++i ) {
            tempFiles[tempCount++] = loadlibDefPaths[i];
        }
        xl6CliCleanupWorkDir(workDir, tempFiles, tempCount, saveTemps);
    }
    return 0;
}

static int xl6CliRunCommand(const char* path, int argc, char** argv)
{
    char quotedExe[512];
    char command[2048];
    char workDir[512];
    char cPath[512];
    char exePath[512];
    char loadlibDefPaths[32][512];
    XL6_CompileUnit unit;
    int rc;
    int loadlibDefCount;
    int i;
    int argStart;
    int saveTemps;
    size_t used;

    saveTemps = xl6CliHasSaveTempsFlag(argc, argv);
    if ( !xl6CliMakeWorkDir(workDir, sizeof(workDir), "run") ||
         !xl6CliJoinPath(cPath, sizeof(cPath), workDir, "module.c") ||
         !xl6CliJoinPath(exePath, sizeof(exePath), workDir, "app.exe") ) {
        return 1;
    }
    if ( !xl6CliLoadCheckedUnit(&unit, path) ) {
        xl6CompileUnitFree(&unit);
        return 1;
    }
    rc = xl6CliCgenUnitToFile(&unit, cPath);
    if ( rc != 0 ) {
        xl6CompileUnitFree(&unit);
        return rc;
    }
    loadlibDefCount = 0;
    rc = xl6CliCompileWithTcc(cPath, exePath, argc, argv, workDir, loadlibDefPaths, 32, &loadlibDefCount);
    if ( rc != 0 ) {
        fprintf(stderr, "tcc run build failed\n");
        xl6CompileUnitFree(&unit);
        return 1;
    }
    xl6CompileUnitFree(&unit);

    if ( !xl6CliQuoteCommand(quotedExe, sizeof(quotedExe), exePath) ) {
        fprintf(stderr, "run path is too long\n");
        return 1;
    }
    command[0] = '\0';
    if ( !xl6CliAppendRunLoadlibPathPrefix(command, sizeof(command), argc, argv) ||
         !xl6CliAppendCommandText(command, sizeof(command), quotedExe) ) {
        fprintf(stderr, "run command is too long\n");
        return 1;
    }
    used = strlen(command);

    argStart = 0;
    for ( i = 3; i < argc; ++i ) {
        if ( strcmp(argv[i], "--") == 0 ) {
            argStart = i + 1;
            break;
        }
    }
    if ( argStart > 0 ) {
        for ( i = argStart; i < argc; ++i ) {
            char quotedArg[512];
            if ( !xl6CliQuoteCommand(quotedArg, sizeof(quotedArg), argv[i]) ) {
                fprintf(stderr, "program argument is too long\n");
                return 1;
            }
            if ( used + strlen(quotedArg) + 2 >= sizeof(command) ) {
                fprintf(stderr, "run command is too long\n");
                return 1;
            }
            command[used++] = ' ';
            command[used] = '\0';
            strcat(command, quotedArg);
            used = strlen(command);
        }
    }

    rc = system(command);
    if ( rc != 0 ) {
        return 1;
    }
    {
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
    return 0;
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
        fprintf(stderr, "missing file argument\n");
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

    fprintf(stderr, "unknown command: %s\n", cmd);
    return 1;
}
