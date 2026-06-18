#include "src/driver/xl6_cli.h"

#include "src/abi/xl6_abi_dump.h"
#include "src/ast/xl6_ast_dump.h"
#include "src/cgen/xl6_cgen.h"
#include "src/ir/xl6_ir_dump.h"
#include "src/lexer/xl6_lexer_api.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"

#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
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
    printf("  xl run <file.xl> [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll] [-- <args>...]\n");
    printf("  xl build <file.xl> -o <output.exe> [-shared] [--emit-header output.h] [--inc-path dir] [--lib-path dir] [--link lib] [--loadlib dll]\n");
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
    int rc;

    if ( path == NULL || path[0] == '\0' ) {
        fprintf(stderr, "dump dir is empty\n");
        return 1;
    }
#ifdef _WIN32
    rc = _mkdir(path);
#else
    rc = mkdir(path, 0777);
#endif
    if ( rc == 0 || errno == EEXIST ) {
        return 0;
    }
    fprintf(stderr, "cannot create dump dir: %s\n", path);
    return 1;
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

static int xl6CliQuoteCommand(char* out, size_t outSize, const char* text)
{
    int written;

    written = snprintf(out, outSize, "\"%s\"", text != NULL ? text : "");
    return written > 0 && (size_t)written < outSize;
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

static int xl6CliMakeLoadlibDef(const char* dllPath, int index, char* outPath, size_t outSize)
{
    char quotedDll[512];
    char quotedDef[512];
    char command[1400];
    int written;
    int rc;

    if ( dllPath == NULL || dllPath[0] == '\0' || outPath == NULL || outSize == 0 ) {
        return 0;
    }

    xl6CliEnsureTmpDir();
    written = snprintf(outPath, outSize, "release\\tmp\\xl6_loadlib_%d.def", index);
    if ( written <= 0 || (size_t)written >= outSize ) {
        fprintf(stderr, "loadlib def path is too long\n");
        return 0;
    }

    if ( !xl6CliQuoteCommand(quotedDll, sizeof(quotedDll), dllPath) ||
         !xl6CliQuoteCommand(quotedDef, sizeof(quotedDef), outPath) ) {
        fprintf(stderr, "loadlib path is too long\n");
        return 0;
    }

    written = snprintf(command, sizeof(command),
                       "..\\demo5\\lib\\tcc\\tcc.exe -impdef %s -o %s",
                       quotedDll, quotedDef);
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

static int xl6CliAppendTccOptions(char* command, size_t commandSize, int argc, char** argv)
{
    int i;
    int loadlibIndex;

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
            if ( !xl6CliMakeLoadlibDef(argv[i + 1], loadlibIndex, defPath, sizeof(defPath)) ||
                 !xl6CliAppendLinkOption(command, commandSize, defPath) ) {
                return 0;
            }
            loadlibIndex += 1;
            i += 1;
        } else if ( strcmp(argv[i], "-shared") == 0 ) {
            if ( !xl6CliAppendCommandText(command, commandSize, " -shared") ) {
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

static int xl6CliCompileWithTcc(const char* cPath, const char* exePath, int argc, char** argv)
{
    char quotedC[512];
    char quotedExe[512];
    char command[2048];
    int written;

    if ( !xl6CliQuoteCommand(quotedC, sizeof(quotedC), cPath) ||
         !xl6CliQuoteCommand(quotedExe, sizeof(quotedExe), exePath) ) {
        fprintf(stderr, "command path is too long\n");
        return 1;
    }

    written = snprintf(command, sizeof(command),
                       "..\\demo5\\lib\\tcc\\tcc.exe -I..\\demo5\\lib\\tcc\\include -I..\\demo5\\release\\tcc\\include_win -L..\\demo5\\release\\tcc\\lib %s -o %s",
                       quotedC, quotedExe);
    if ( written <= 0 || (size_t)written >= sizeof(command) ) {
        fprintf(stderr, "tcc command is too long\n");
        return 1;
    }
    if ( !xl6CliAppendTccOptions(command, sizeof(command), argc, argv) ) {
        fprintf(stderr, "tcc options are too long\n");
        return 1;
    }
    return system(command);
}

static int xl6CliWriteHeader(const char* headerPath, XL6_AstNode* root, XL6_DiagList* diags)
{
    FILE* out;
    char guard[256];
    int ok;

    if ( headerPath == NULL ) {
        return 0;
    }

    out = fopen(headerPath, "wb");
    if ( out == NULL ) {
        fprintf(stderr, "cannot open header output file: %s\n", headerPath);
        return 1;
    }

    xl6CliMakeHeaderGuard(guard, sizeof(guard), headerPath);
    ok = xl6CgenEmitHeader(out, root, guard, diags);
    fclose(out);
    return ok ? 0 : 1;
}

static int xl6CliHeaderToFile(const char* path, const char* headerPath)
{
    XL6_ParseResult result;
    int ok;
    int rc;

    if ( headerPath == NULL ) {
        return 0;
    }

    xl6ParseResultInit(&result);
    ok = xl6ParseFile(path, &result);
    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    ok = xl6SemCheckModule(result.root, &result.diags);
    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    rc = xl6CliWriteHeader(headerPath, result.root, &result.diags);
    if ( rc != 0 ) {
        xl6DiagPrintText(&result.diags, path);
    }
    xl6ParseResultFree(&result);
    return rc;
}

static int xl6CliCgenToFile(const char* path, const char* cPath)
{
    XL6_ParseResult result;
    FILE* out;
    int ok;

    xl6ParseResultInit(&result);
    ok = xl6ParseFile(path, &result);
    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    ok = xl6SemCheckModule(result.root, &result.diags);
    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    out = fopen(cPath, "wb");
    if ( out == NULL ) {
        fprintf(stderr, "cannot open c output file: %s\n", cPath);
        xl6ParseResultFree(&result);
        return 1;
    }

    ok = xl6CgenEmitC(out, result.root, &result.diags);
    fclose(out);

    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    xl6ParseResultFree(&result);
    return 0;
}

static int xl6CliParseCommand(const char* path, int dumpAst, int runSem, const char* emitPath, int jsonOutput)
{
    XL6_ParseResult result;
    FILE* out;
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
        out = stdout;
        if ( emitPath != NULL ) {
            out = fopen(emitPath, "wb");
            if ( out == NULL ) {
                fprintf(stderr, "cannot open emit file: %s\n", emitPath);
                xl6ParseResultFree(&result);
                return 1;
            }
        }
        if ( jsonOutput ) {
            xl6AstDumpJson(out, result.root);
        } else {
            xl6AstDumpTextFile(out, result.root, 0);
        }
        if ( emitPath != NULL ) {
            fclose(out);
        }
    } else {
        printf("ok\n");
    }

    xl6ParseResultFree(&result);
    return 0;
}

static int xl6CliCheckCommand(const char* path, int jsonOutput)
{
    XL6_ParseResult result;
    int ok;
    int success;

    xl6ParseResultInit(&result);
    ok = xl6ParseFile(path, &result);
    if ( ok ) {
        ok = xl6SemCheckModule(result.root, &result.diags);
    }
    success = ok && result.diags.count == 0;

    if ( jsonOutput ) {
        xl6DiagPrintJson(stdout, &result.diags, path, success);
    } else if ( success ) {
        printf("ok\n");
    } else {
        xl6DiagPrintText(&result.diags, path);
    }

    xl6ParseResultFree(&result);
    return success ? 0 : 1;
}

static int xl6CliLexCommand(const char* path, const char* emitPath, int jsonOutput)
{
    FILE* out;
    int rc;

    out = stdout;
    if ( emitPath != NULL ) {
        out = fopen(emitPath, "wb");
        if ( out == NULL ) {
            fprintf(stderr, "cannot open emit file: %s\n", emitPath);
            return 1;
        }
    }
    rc = xl6LexerDumpFileTo(out, path, jsonOutput);
    if ( emitPath != NULL ) {
        fclose(out);
    }
    return rc;
}

static int xl6CliIrCommand(const char* path, const char* emitPath, int jsonOutput)
{
    XL6_ParseResult result;
    FILE* out;
    int ok;

    xl6ParseResultInit(&result);
    ok = xl6ParseFile(path, &result);
    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    ok = xl6SemCheckModule(result.root, &result.diags);
    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    out = stdout;
    if ( emitPath != NULL ) {
        out = fopen(emitPath, "wb");
        if ( out == NULL ) {
            fprintf(stderr, "cannot open emit file: %s\n", emitPath);
            xl6ParseResultFree(&result);
            return 1;
        }
    }

    if ( jsonOutput ) {
        xl6IrDumpJson(out, result.root);
    } else {
        xl6IrDumpText(out, result.root);
    }

    if ( emitPath != NULL ) {
        fclose(out);
    }
    xl6ParseResultFree(&result);
    return 0;
}

static int xl6CliCgenDumpDirCommand(const char* path, const char* dumpDir);

static int xl6CliCgenCommand(const char* path, const char* emitPath, const char* emitHeaderPath, const char* dumpDir)
{
    XL6_ParseResult result;
    FILE* out;
    int ok;
    int rc;

    if ( dumpDir != NULL ) {
        rc = xl6CliCgenDumpDirCommand(path, dumpDir);
        if ( rc != 0 || (emitPath == NULL && emitHeaderPath == NULL) ) {
            return rc;
        }
    }

    xl6ParseResultInit(&result);
    ok = xl6ParseFile(path, &result);
    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    ok = xl6SemCheckModule(result.root, &result.diags);
    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    out = stdout;
    if ( emitPath != NULL ) {
        out = fopen(emitPath, "wb");
        if ( out == NULL ) {
            fprintf(stderr, "cannot open emit file: %s\n", emitPath);
            xl6ParseResultFree(&result);
            return 1;
        }
    }

    ok = xl6CgenEmitC(out, result.root, &result.diags);

    if ( emitPath != NULL ) {
        fclose(out);
    }
    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    rc = xl6CliWriteHeader(emitHeaderPath, result.root, &result.diags);
    if ( rc != 0 ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return rc;
    }

    xl6ParseResultFree(&result);
    return 0;
}

static int xl6CliAbiCommand(const char* path, const char* emitPath, int jsonOutput)
{
    XL6_ParseResult result;
    FILE* out;
    int ok;

    xl6ParseResultInit(&result);
    ok = xl6ParseFile(path, &result);
    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    ok = xl6SemCheckModule(result.root, &result.diags);
    if ( !ok ) {
        xl6DiagPrintText(&result.diags, path);
        xl6ParseResultFree(&result);
        return 1;
    }

    out = stdout;
    if ( emitPath != NULL ) {
        out = fopen(emitPath, "wb");
        if ( out == NULL ) {
            fprintf(stderr, "cannot open emit file: %s\n", emitPath);
            xl6ParseResultFree(&result);
            return 1;
        }
    }

    if ( jsonOutput ) {
        xl6AbiDumpJson(out, result.root);
    } else {
        xl6AbiDumpText(out, result.root);
    }

    if ( emitPath != NULL ) {
        fclose(out);
    }
    xl6ParseResultFree(&result);
    return 0;
}

static int xl6CliCgenDumpDirCommand(const char* path, const char* dumpDir)
{
    char base[256];
    char tokensPath[512];
    char astPath[512];
    char irPath[512];
    char abiPath[512];
    char cPath[512];
    char headerPath[512];
    int rc;

    if ( xl6CliMakeDir(dumpDir) != 0 ) {
        return 1;
    }

    xl6CliBaseNameNoExt(base, sizeof(base), path);
    if ( !xl6CliJoinDumpPath(tokensPath, sizeof(tokensPath), dumpDir, base, ".tokens.json") ||
         !xl6CliJoinDumpPath(astPath, sizeof(astPath), dumpDir, base, ".ast.json") ||
         !xl6CliJoinDumpPath(irPath, sizeof(irPath), dumpDir, base, ".xlir") ||
         !xl6CliJoinDumpPath(abiPath, sizeof(abiPath), dumpDir, base, ".abi.json") ||
         !xl6CliJoinDumpPath(cPath, sizeof(cPath), dumpDir, base, ".c") ||
         !xl6CliJoinDumpPath(headerPath, sizeof(headerPath), dumpDir, base, ".h") ) {
        fprintf(stderr, "dump path is too long\n");
        return 1;
    }

    rc = xl6CliLexCommand(path, tokensPath, 1);
    if ( rc != 0 ) {
        return rc;
    }
    rc = xl6CliParseCommand(path, 1, 0, astPath, 1);
    if ( rc != 0 ) {
        return rc;
    }
    rc = xl6CliIrCommand(path, irPath, 0);
    if ( rc != 0 ) {
        return rc;
    }
    rc = xl6CliAbiCommand(path, abiPath, 1);
    if ( rc != 0 ) {
        return rc;
    }
    rc = xl6CliCgenToFile(path, cPath);
    if ( rc != 0 ) {
        return rc;
    }
    return xl6CliHeaderToFile(path, headerPath);
}

static int xl6CliBuildCommand(const char* path, const char* outputPath, const char* emitHeaderPath, int argc, char** argv)
{
    const char* cPath;
    int rc;

    if ( outputPath == NULL ) {
        fprintf(stderr, "build requires -o <output.exe>\n");
        return 1;
    }

    xl6CliEnsureTmpDir();
    cPath = "release\\tmp\\xl6_build_tmp.c";
    rc = xl6CliCgenToFile(path, cPath);
    if ( rc != 0 ) {
        return rc;
    }
    rc = xl6CliHeaderToFile(path, emitHeaderPath);
    if ( rc != 0 ) {
        return rc;
    }

    rc = xl6CliCompileWithTcc(cPath, outputPath, argc, argv);
    if ( rc != 0 ) {
        fprintf(stderr, "tcc build failed\n");
        return 1;
    }
    return 0;
}

static int xl6CliRunCommand(const char* path, int argc, char** argv)
{
    char quotedExe[512];
    char command[2048];
    const char* cPath;
    const char* exePath;
    int rc;
    int i;
    int argStart;
    size_t used;

    xl6CliEnsureTmpDir();
    cPath = "release\\tmp\\xl6_run_tmp.c";
    exePath = "release\\tmp\\xl6_run_tmp.exe";

    rc = xl6CliCgenToFile(path, cPath);
    if ( rc != 0 ) {
        return rc;
    }
    rc = xl6CliCompileWithTcc(cPath, exePath, argc, argv);
    if ( rc != 0 ) {
        fprintf(stderr, "tcc run build failed\n");
        return 1;
    }

    if ( !xl6CliQuoteCommand(quotedExe, sizeof(quotedExe), exePath) ) {
        fprintf(stderr, "run path is too long\n");
        return 1;
    }
    snprintf(command, sizeof(command), "%s", quotedExe);
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
        return xl6CliBuildCommand(file, xl6CliFindOutputPath(argc, argv), xl6CliFindEmitHeaderPath(argc, argv), argc, argv);
    }

    if ( strcmp(cmd, "abi") == 0 ) {
        return xl6CliAbiCommand(file, xl6CliFindEmitPath(argc, argv), xl6CliHasJson(argc, argv));
    }

    fprintf(stderr, "unknown command: %s\n", cmd);
    return 1;
}
