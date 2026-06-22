#include "include/xl.h"

#include <stdio.h>

static long long host_add(long long a, long long b)
{
    return a + b;
}

int main(void)
{
    XL_ContextOptions options;
    XL_Context* ctx;
    const char* script;
    const char* buildScript;
    int ok;

    options = xlDefaultContextOptions();
    options.workDir = ".";
    options.tempDir = "release\\tmp";

    ctx = xlCreateContext(&options);
    if ( ctx == NULL ) {
        fprintf(stderr, "cannot create xlang context\n");
        return 1;
    }

    if ( !xlRegisterCFunction(ctx, "host_add", (void*)host_add) ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }

    script =
        "extern \"host_add\" func hostAdd(int a, int b) -> int\n"
        "\n"
        "int value = hostAdd(20, 22)\n"
        "assert value == 42, \"host function call failed\"\n"
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
        "func add(int a, int b) -> int\n"
        "    return a + b\n"
        "end\n"
        "\n"
        "int value = add(1, 2)\n"
        "assert value == 3, \"compiled exe failed\"\n";

    ok = xlRunScriptString(ctx, script);
    if ( !ok ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
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

    printf("xl.dll embed string ok\n");
    xlDestroyContext(ctx);
    return 0;
}
