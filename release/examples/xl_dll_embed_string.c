#ifndef XL6_EXAMPLE_BOUNDARY_TEST
#include "include/xl.h"

#include <stdio.h>

int main(void)
{
    XL_ContextOptions options = xlDefaultContextOptions();
    XL_Context* ctx = xlCreateContext(&options);

    if ( ctx == NULL ) {
        fprintf(stderr, "cannot create xl context\n");
        return 1;
    }
    if ( !xlRunScriptString(ctx, "println(\"xl dll embed string ok\")\nassert 40 + 2 == 42\n") ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 2;
    }
    if ( xlGetLastExitCode(ctx) != 0 ) {
        fprintf(stderr, "unexpected xl exit code: %d\n", xlGetLastExitCode(ctx));
        xlDestroyContext(ctx);
        return 3;
    }
    xlDestroyContext(ctx);
    return 0;
}
#else
#include "include/xl.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static long long host_add(long long a, long long b)
{
    return a + b;
}

static int write_text_file(const char* path, const char* text)
{
    FILE* fp = fopen(path, "wb");
    if ( fp == NULL ) {
        return 0;
    }
    if ( fputs(text, fp) < 0 ) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static int make_dir_if_missing(const char* path)
{
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0777) == 0 || errno == EEXIST;
#endif
}

static void cleanup_dynamic_vfs_probe(void)
{
#ifdef _WIN32
    remove("__xlvfs__\\module.c");
    _rmdir("__xlvfs__");
#else
    remove("__xlvfs__/module.c");
    rmdir("__xlvfs__");
#endif
}

static int prepare_dynamic_vfs_probe(void)
{
    cleanup_dynamic_vfs_probe();
    if ( !make_dir_if_missing("__xlvfs__") ) {
        return 0;
    }
#ifdef _WIN32
    return write_text_file("__xlvfs__\\module.c", "#error external dynamic VFS probe must be shadowed\n");
#else
    return write_text_file("__xlvfs__/module.c", "#error external dynamic VFS probe must be shadowed\n");
#endif
}

static int run_script_step(XL_Context* ctx, const char* name, const char* source)
{
    if ( xlRunScriptStringArg(ctx, source, NULL) ) {
        return 1;
    }
    fprintf(stderr, "%s failed: %s\n", name, xlGetLastError(ctx));
    return 0;
}

static int run_module_step(XL_Context* ctx, const char* name, const char* source)
{
    XL_Module* module;
    void* entry;
    int ok;

    module = xlCompileScriptStringToModule(ctx, source);
    if ( module == NULL ) {
        fprintf(stderr, "%s compile module failed: %s\n", name, xlGetLastError(ctx));
        return 0;
    }
    entry = xlModuleGetSymbol(module, "xlang_main");
    if ( entry == NULL ) {
        fprintf(stderr, "%s module entry missing\n", name);
        xlDestroyModule(module);
        return 0;
    }
    ok = xlRunModuleMain(ctx, module, NULL);
    if ( !ok ) {
        fprintf(stderr, "%s run module failed: %s\n", name, xlGetLastError(ctx));
    }
    xlDestroyModule(module);
    return ok;
}

static int run_resident_module_step(XL_Context* ctx)
{
    const char* source;
    XL_Module* module;
    int (*read_value)(void);
    int ok;

    source =
        "const string moduleName = \"resident-module\"\n"
        "int requestCount = 40\n"
        "export \"xl_dll_resident_read\" func read() as c_int\n"
        "    requestCount += 1\n"
        "    if moduleName != \"resident-module\" then return c_int(-1) end\n"
        "    return c_int(requestCount)\n"
        "end\n";
    module = xlCompileScriptStringToModule(ctx, source);
    if ( module == NULL ) {
        fprintf(stderr, "resident module compile failed: %s\n", xlGetLastError(ctx));
        return 0;
    }
    ok = xlActivateModule(ctx, module, NULL);
    read_value = (int (*)(void))xlModuleGetSymbol(module, "xl_dll_resident_read");
    ok = ok && read_value != NULL && read_value() == 41 && read_value() == 42;
    ok = ok && xlDeactivateModule(ctx, module) && xlDeactivateModule(ctx, module);
    ok = ok && xlActivateModule(ctx, module, NULL) && read_value() == 41;
    ok = ok && xlDeactivateModule(ctx, module);
    if ( !ok ) {
        fprintf(stderr, "resident module lifecycle failed: %s\n", xlGetLastError(ctx));
    }
    xlDestroyModule(module);
    return ok;
}

static int run_c_source_step(XL_Context* ctx)
{
    char* argv[2];
    XL_Module* module;
    int (*exported_value)(void);
    const char* cRunSource;
    const char* cModuleSource;
    const char* cFileSource;
    int ok;

    cRunSource =
        "int main(int argc, char** argv) {\n"
        "    return argc == 2 && argv != 0 && argv[1][0] == 'x' ? 0 : 7;\n"
        "}\n";
    argv[0] = "xl_dll_c";
    argv[1] = "x";
    if ( !xlRunCSourceString(ctx, cRunSource, 2, argv) ) {
        fprintf(stderr, "C source run failed: %s\n", xlGetLastError(ctx));
        return 0;
    }

    cModuleSource =
        "int exported_value(void) { return 42; }\n"
        "int main(int argc, char** argv) { return 0; }\n";
    module = xlCompileCSourceStringToModule(ctx, cModuleSource);
    if ( module == NULL ) {
        fprintf(stderr, "C source module compile failed: %s\n", xlGetLastError(ctx));
        return 0;
    }
    exported_value = (int (*)(void))xlModuleGetSymbol(module, "exported_value");
    ok = exported_value != NULL && exported_value() == 42;
    if ( !ok ) {
        fprintf(stderr, "C source module symbol failed\n");
    }
    xlDestroyModule(module);

    cFileSource =
        "int exported_file_value(void) { return 42; }\n"
        "int main(int argc, char** argv) { return 0; }\n";
    if ( !write_text_file("build\\test\\xl_dll_embed_c_file.c", cFileSource) ) {
        fprintf(stderr, "cannot write C source file\n");
        return 0;
    }
    if ( !xlRunCSourceFile(ctx, "build\\test\\xl_dll_embed_c_file.c", 0, NULL) ) {
        fprintf(stderr, "C source file run failed: %s\n", xlGetLastError(ctx));
        return 0;
    }
    module = xlCompileCSourceFileToModule(ctx, "build\\test\\xl_dll_embed_c_file.c");
    if ( module == NULL ) {
        fprintf(stderr, "C source file module compile failed: %s\n", xlGetLastError(ctx));
        return 0;
    }
    exported_value = (int (*)(void))xlModuleGetSymbol(module, "exported_file_value");
    ok = exported_value != NULL && exported_value() == 42;
    if ( !ok ) {
        fprintf(stderr, "C source file module symbol failed\n");
    }
    xlDestroyModule(module);
    return ok;
}

static int expect_api_failure(XL_Context* ctx, const char* name, int ok, const char* needle)
{
    const char* err;

    if ( ok ) {
        fprintf(stderr, "%s unexpectedly succeeded\n", name);
        return 0;
    }
    err = xlGetLastError(ctx);
    if ( err == NULL || err[0] == '\0' || (needle != NULL && strstr(err, needle) == NULL) ) {
        fprintf(stderr, "%s did not set expected lastError: %s\n", name, err != NULL ? err : "");
        return 0;
    }
    xlClearLastError(ctx);
    return 1;
}

int main(void)
{
    XL_ContextOptions options;
    XL_Context* ctx;
    const char* hostScript;
    const char* linkScript;
    const char* loadlibScript;
    const char* stdlibScript;
    const char* buildScript;
    const char* fileScript;
    const char* fileBuildScript;
    const char* err;
    XL_Module* fileModule;
    int ok;

    options = xlDefaultContextOptions();
    options.workDir = ".";
    options.tempDir = "build\\test\\dll_api_temp";

    ctx = xlCreateContext(&options);
    if ( ctx == NULL ) {
        fprintf(stderr, "cannot create xlang context\n");
        return 1;
    }

    if ( !xlSetWorkDir(ctx, ".") || !xlSetTempDir(ctx, "build\\test\\dll_api_temp") || !xlSetSaveTemps(ctx, 0) ||
         !xlAddIncludePath(ctx, ".") || !xlAddLibraryPath(ctx, "build\\tmp") ||
         !xlAddLinkLibrary(ctx, "xl6dlllink") ||
          !xlLoadDependencyLibrary(ctx, "build\\tmp\\xl_dll_loadlib_host.dll") ||
          !xlAddLinkLibrary(ctx, "build\\tmp\\xl_dll_loadlib_host.def") ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }

    if ( !expect_api_failure(ctx, "xlSetWorkDir empty", xlSetWorkDir(ctx, ""), "work dir path is empty") ||
         !expect_api_failure(ctx, "xlSetTempDir empty", xlSetTempDir(ctx, ""), "temp dir path is empty") ||
         !expect_api_failure(ctx, "xlAddIncludePath empty", xlAddIncludePath(ctx, ""), "cannot add include path") ||
         !expect_api_failure(ctx, "xlAddLibraryPath empty", xlAddLibraryPath(ctx, ""), "cannot add library path") ||
         !expect_api_failure(ctx, "xlAddLinkLibrary empty", xlAddLinkLibrary(ctx, ""), "cannot add link library") ||
         !expect_api_failure(ctx, "xlAddResourceFile empty", xlAddResourceFile(ctx, ""), "resource config path is empty") ||
         !expect_api_failure(ctx, "xlLoadDependencyLibrary empty", xlLoadDependencyLibrary(ctx, ""), "dependency library path is empty") ) {
        xlDestroyContext(ctx);
        return 1;
    }

    if ( !xlRegisterCFunction(ctx, "host_add", (void*)host_add) ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }

    if ( !prepare_dynamic_vfs_probe() ) {
        fprintf(stderr, "cannot prepare dynamic VFS priority probe\n");
        xlDestroyContext(ctx);
        return 1;
    }

    hostScript =
        "extern \"host_add\" func hostAdd(int a, int b) as int\n"
        "\n"
        "int value = hostAdd(20, 22)\n"
        "assert value == 42, \"host function call failed\"\n";

    linkScript =
        "extern \"xl6_dll_link_value\" func linkedValue() as int\n"
        "assert linkedValue() == 42, \"link library call failed\"\n";

    loadlibScript =
        "extern \"xl6_dll_loadlib_value\" func loadedValue() as int\n"
        "assert loadedValue() == 42, \"loadlib call failed\"\n";

    stdlibScript =
        "assert string.ltrim(\"  dll\") == \"dll\", \"stdlib ltrim failed\"\n"
        "assert string.filter(\"x-lang\", \"-\") == \"xlang\", \"stdlib filter failed\"\n"
        "assert string.startsWith(\"dll-string\", \"DLL\", true), \"stdlib startsWith failed\"\n"
        "assert string.endsWith(\"dll-string\", \"string\"), \"stdlib endsWith failed\"\n"
        "assert json.stringify(json.parse(\"[1,2,3]\")) == \"[1,2,3]\", \"stdlib json failed\"\n"
        "assert codec.base64Encode(\"dll\") == \"ZGxs\", \"stdlib codec encode failed\"\n"
        "assert codec.base64Decode(\"ZGxs\") == b\"dll\", \"stdlib codec bytes decode failed\"\n"
        "assert encoding.decodeUtf8(codec.base64Decode(\"ZGxs\")) == \"dll\", \"stdlib codec UTF-8 decode failed\"\n"
        "assert hash.u64(\"dll\") == hash.u64(\"dll\"), \"stdlib hash failed\"\n"
        "assert crypto.sha256(\"dll\") == crypto.sha256(\"dll\"), \"stdlib crypto failed\"\n"
        "assert string.len(id.xid()) == 32, \"stdlib id failed\"\n"
        "assert codec.percentEncode(\"dll path\") == \"dll%20path\", \"stdlib percent codec failed\"\n"
        "assert time.formatPattern(time.parsePattern(\"2026/06/18 12:30:59\", \"%Y/%m/%d %T\"), \"%F %T\") == \"2026-06-18 12:30:59\", \"stdlib time pattern failed\"\n"
        "assert string.indexOf(\"dll-2026\", \"2026\") == 4, \"stdlib string index failed\"\n"
        "assert string.trim(process.exec(\"cmd /c echo dll-process\")) == \"dll-process\", \"stdlib process failed\"\n"
        "assert process.exitCode(\"cmd /c exit /b 6\") == 6, \"stdlib process exitCode failed\"\n"
        "dir.createAll(\"build/test/dll_dir_list\")\n"
        "file.writeText(\"build/test/dll_dir_list/a.txt\", \"a\")\n"
        "assert string.contains(json.stringify(dir.list(\"build/test/dll_dir_list\")), \"a.txt\"), \"stdlib dir.list failed\"\n"
        "assert string.contains(json.stringify(dir.files(\"build/test/dll_dir_list\")), \"a.txt\"), \"stdlib dir.files failed\"\n"
        "assert !string.contains(json.stringify(dir.dirs(\"build/test/dll_dir_list\")), \"a.txt\"), \"stdlib dir.dirs failed\"\n"
        "dir.delete(\"build/test/dll_dir_list\")\n";

    buildScript =
        "func add(int a, int b) as int\n"
        "    return a + b\n"
        "end\n"
        "\n"
        "int value = add(1, 2)\n"
        "assert value == 3, \"compiled exe failed\"\n";

    fileScript =
        "int value = 6 * 7\n"
        "assert value == 42, \"file run failed\"\n";

    fileBuildScript =
        "int value = 40 + 2\n"
        "assert value == 42, \"file compile exe failed\"\n";

    if ( !run_script_step(ctx, "host script", hostScript) ||
         !run_script_step(ctx, "link script", linkScript) ||
         !run_script_step(ctx, "loadlib script", loadlibScript) ||
         !run_script_step(ctx, "stdlib script", stdlibScript) ||
         !run_module_step(ctx, "module script", "int value = 40 + 2\nassert value == 42, \"module run failed\"\n") ||
         !run_resident_module_step(ctx) ||
         !run_c_source_step(ctx) ) {
        cleanup_dynamic_vfs_probe();
        xlDestroyContext(ctx);
        return 1;
    }
    cleanup_dynamic_vfs_probe();
    printf("xl.dll run string ok\n");

    ok = xlCompileScriptStringToExe(ctx, buildScript, "build\\test\\xl_dll_embed_string_script.exe");
    if ( !ok ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }

    if ( !write_text_file("build\\test\\xl_dll_embed_file_run.xl", fileScript) || !write_text_file("build\\test\\xl_dll_embed_file_build.xl", fileBuildScript) ) {
        fprintf(stderr, "cannot write xl.dll file scripts\n");
        xlDestroyContext(ctx);
        return 1;
    }

    ok = xlRunScriptFile(ctx, "build\\test\\xl_dll_embed_file_run.xl");
    if ( !ok ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }
    fileModule = xlCompileScriptFileToModule(ctx, "build\\test\\xl_dll_embed_file_run.xl");
    if ( fileModule == NULL ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }
    ok = xlRunModuleMain(ctx, fileModule, NULL);
    xlDestroyModule(fileModule);
    if ( !ok ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }
    printf("xl.dll run file ok\n");

    ok = xlCompileScriptFileToExe(ctx, "build\\test\\xl_dll_embed_file_build.xl", "build\\test\\xl_dll_embed_file_script.exe");
    if ( !ok ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }

    ok = xlRunScriptString(ctx, "func broken(\n");
    if ( ok ) {
        fprintf(stderr, "invalid script unexpectedly succeeded\n");
        xlDestroyContext(ctx);
        return 1;
    }
    err = xlGetLastError(ctx);
    if ( err == NULL || err[0] == '\0' ||
         strstr(err, "__xl_string.xl:1:") == NULL ||
         strstr(err, "[parse]") == NULL ) {
        fprintf(stderr, "invalid script did not set lastError\n");
        xlDestroyContext(ctx);
        return 1;
    }
    xlClearLastError(ctx);
    if ( xlGetLastError(ctx)[0] != '\0' ) {
        fprintf(stderr, "xlClearLastError failed\n");
        xlDestroyContext(ctx);
        return 1;
    }

    if ( !xlRunScriptString(ctx, "func main(...) as int\n    return 7\nend\n") ) {
        fprintf(stderr, "exit code script failed: %s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }
    if ( xlGetLastExitCode(ctx) != 7 ) {
        fprintf(stderr, "xlGetLastExitCode failed: %d\n", xlGetLastExitCode(ctx));
        xlDestroyContext(ctx);
        return 1;
    }

    printf("xl.dll embed string ok\n");
    xlDestroyContext(ctx);
    return 0;
}
#endif
