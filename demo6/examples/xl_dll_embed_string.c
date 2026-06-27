#include "include/xl.h"

#include <stdio.h>
#include <string.h>

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

static int run_script_step(XL_Context* ctx, const char* name, const char* source)
{
    if ( xlRunScriptString(ctx, source) ) {
        return 1;
    }
    fprintf(stderr, "%s failed: %s\n", name, xlGetLastError(ctx));
    return 0;
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
    int ok;

    options = xlDefaultContextOptions();
    options.workDir = ".";
    options.tempDir = "release\\tmp\\dll_api_temp";
    options.tccDllPath = "release\\libtcc.dll";

    ctx = xlCreateContext(&options);
    if ( ctx == NULL ) {
        fprintf(stderr, "cannot create xlang context\n");
        return 1;
    }

    if ( !xlSetWorkDir(ctx, ".") || !xlSetTempDir(ctx, "release\\tmp\\dll_api_temp") || !xlSetSaveTemps(ctx, 0) ||
         !xlAddIncludePath(ctx, ".") || !xlAddLibraryPath(ctx, "release\\tmp") ||
         !xlAddLinkLibrary(ctx, "xl6dlllink") ||
         !xlLoadDependencyLibrary(ctx, "release\\tmp\\xl_dll_loadlib_host.dll") ||
         !xlAddLinkLibrary(ctx, "release\\tmp\\xl_dll_loadlib_host.def") ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }

    if ( !expect_api_failure(ctx, "xlSetWorkDir empty", xlSetWorkDir(ctx, ""), "work dir path is empty") ||
         !expect_api_failure(ctx, "xlSetTempDir empty", xlSetTempDir(ctx, ""), "temp dir path is empty") ||
         !expect_api_failure(ctx, "xlAddIncludePath empty", xlAddIncludePath(ctx, ""), "cannot add include path") ||
         !expect_api_failure(ctx, "xlAddLibraryPath empty", xlAddLibraryPath(ctx, ""), "cannot add library path") ||
         !expect_api_failure(ctx, "xlAddLinkLibrary empty", xlAddLinkLibrary(ctx, ""), "cannot add link library") ||
         !expect_api_failure(ctx, "xlLoadDependencyLibrary empty", xlLoadDependencyLibrary(ctx, ""), "dependency library path is empty") ) {
        xlDestroyContext(ctx);
        return 1;
    }

    if ( !xlRegisterCFunction(ctx, "host_add", (void*)host_add) ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }

    hostScript =
        "extern \"host_add\" func hostAdd(int a, int b) as int\n"
        "\n"
        "assert dir.exists(\"release/tmp/dll_api_temp\"), \"temp dir was not prepared\"\n"
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
        "assert codec.base64Decode(codec.base64Encode(\"dll\")) == \"dll\", \"stdlib codec failed\"\n"
        "assert hash.u64(\"dll\") == hash.u64(\"dll\"), \"stdlib hash failed\"\n"
        "assert crypto.sha256(\"dll\") == crypto.sha256(\"dll\"), \"stdlib crypto failed\"\n"
        "assert string.len(id.xid()) == 32, \"stdlib id failed\"\n"
        "assert url.host(\"https://example.com/a\") == \"example.com\", \"stdlib url failed\"\n"
        "assert time.formatPattern(time.parsePattern(\"2026/06/18 12:30:59\", \"yyyy/mm/dd hh:mm:ss\"), \"yyyy-mm-dd hh:mm:ss\") == \"2026-06-18 12:30:59\", \"stdlib time pattern failed\"\n"
        "assert regex.find(\"dll-2026\", \"[0-9]+\") == 4, \"stdlib regex failed\"\n"
        "assert string.trim(process.exec(\"cmd /c echo dll-process\")) == \"dll-process\", \"stdlib process failed\"\n"
        "assert process.exitCode(\"cmd /c exit /b 6\") == 6, \"stdlib process exitCode failed\"\n"
        "dir.createAll(\"release/tmp/dll_dir_list\")\n"
        "file.writeText(\"release/tmp/dll_dir_list/a.txt\", \"a\")\n"
        "assert string.contains(json.stringify(dir.list(\"release/tmp/dll_dir_list\")), \"a.txt\"), \"stdlib dir.list failed\"\n"
        "assert string.contains(json.stringify(dir.files(\"release/tmp/dll_dir_list\")), \"a.txt\"), \"stdlib dir.files failed\"\n"
        "assert !string.contains(json.stringify(dir.dirs(\"release/tmp/dll_dir_list\")), \"a.txt\"), \"stdlib dir.dirs failed\"\n"
        "dir.delete(\"release/tmp/dll_dir_list\")\n";

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
         !run_script_step(ctx, "stdlib script", stdlibScript) ) {
        xlDestroyContext(ctx);
        return 1;
    }
    printf("xl.dll run string ok\n");

    ok = xlCompileScriptStringToExe(ctx, buildScript, "release\\tmp\\xl_dll_embed_string_script.exe");
    if ( !ok ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }

    if ( !write_text_file("release\\tmp\\xl_dll_embed_file_run.xl", fileScript) || !write_text_file("release\\tmp\\xl_dll_embed_file_build.xl", fileBuildScript) ) {
        fprintf(stderr, "cannot write xl.dll file scripts\n");
        xlDestroyContext(ctx);
        return 1;
    }

    ok = xlRunScriptFile(ctx, "release\\tmp\\xl_dll_embed_file_run.xl");
    if ( !ok ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }
    printf("xl.dll run file ok\n");

    ok = xlCompileScriptFileToExe(ctx, "release\\tmp\\xl_dll_embed_file_build.xl", "release\\tmp\\xl_dll_embed_file_script.exe");
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

    printf("xl.dll embed string ok\n");
    xlDestroyContext(ctx);
    return 0;
}
