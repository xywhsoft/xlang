#include "include/xl.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static void dummy_host_function(void)
{
}

static int expect_false(const char* name, int value)
{
    if ( value ) {
        fprintf(stderr, "%s unexpectedly succeeded\n", name);
        return 0;
    }
    return 1;
}

static int expect_empty_error(const char* name)
{
    const char* text = xlGetLastError(NULL);
    if ( text == NULL || strcmp(text, "") != 0 ) {
        fprintf(stderr, "%s returned non-empty NULL-context lastError\n", name);
        return 0;
    }
    return 1;
}

static int expect_api_failure(XL_Context* ctx, const char* name, int ok, const char* needle)
{
    const char* text;

    if ( ok ) {
        fprintf(stderr, "%s unexpectedly succeeded\n", name);
        return 0;
    }
    text = xlGetLastError(ctx);
    if ( text == NULL || text[0] == '\0' || (needle != NULL && strstr(text, needle) == NULL) ) {
        fprintf(stderr, "%s did not set expected lastError: %s\n", name, text != NULL ? text : "");
        return 0;
    }
    xlClearLastError(ctx);
    return 1;
}

static int get_current_dir(char* out, size_t outSize)
{
#ifdef _WIN32
    DWORD got = GetCurrentDirectoryA((DWORD)outSize, out);
    return got > 0 && got < outSize;
#else
    return getcwd(out, outSize) != NULL;
#endif
}

static int make_full_path(const char* base, const char* rel, char* out, size_t outSize)
{
    char joined[1024];
    int written = snprintf(joined, sizeof(joined), "%s\\%s", base, rel);
    if ( written <= 0 || (size_t)written >= sizeof(joined) ) {
        return 0;
    }
#ifdef _WIN32
    {
        DWORD got = GetFullPathNameA(joined, (DWORD)outSize, out, NULL);
        return got > 0 && got < outSize;
    }
#else
    return realpath(joined, out) != NULL;
#endif
}

static int make_dir(const char* path)
{
#ifdef _WIN32
    if ( CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS ) {
        return 1;
    }
    return 0;
#else
    return mkdir(path, 0777) == 0 || errno == EEXIST;
#endif
}

static void delete_file_if_exists(const char* path)
{
#ifdef _WIN32
    DeleteFileA(path);
#else
    remove(path);
#endif
}

static int file_exists(const char* path)
{
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

static int expect_cwd(const char* name, const char* expected)
{
    char now[1024];
    if ( !get_current_dir(now, sizeof(now)) ) {
        fprintf(stderr, "%s cannot query current directory\n", name);
        return 0;
    }
    if ( strcmp(now, expected) != 0 ) {
        fprintf(stderr, "%s changed cwd: %s\n", name, now);
        return 0;
    }
    return 1;
}

static int add_default_include_paths(XL_Context* ctx, const char* root)
{
    char path[1024];

    if ( !xlAddIncludePath(ctx, root) ) {
        return 0;
    }
    if ( !make_full_path(root, "src", path, sizeof(path)) || !xlAddIncludePath(ctx, path) ) {
        return 0;
    }
    if ( !make_full_path(root, "lib\\xrt", path, sizeof(path)) || !xlAddIncludePath(ctx, path) ) {
        return 0;
    }
    if ( !make_full_path(root, "..\\demo5\\lib\\tcc\\include", path, sizeof(path)) || !xlAddIncludePath(ctx, path) ) {
        return 0;
    }
    if ( !make_full_path(root, "..\\demo5\\release\\tcc\\include_win", path, sizeof(path)) || !xlAddIncludePath(ctx, path) ) {
        return 0;
    }
    if ( !make_full_path(root, "..\\demo5\\release\\tcc\\include_win\\winapi", path, sizeof(path)) || !xlAddIncludePath(ctx, path) ) {
        return 0;
    }
    if ( !make_full_path(root, "..\\demo5\\release\\tcc\\lib", path, sizeof(path)) || !xlAddLibraryPath(ctx, path) ) {
        return 0;
    }
    return 1;
}

static XL_Context* create_context_for_workdir(const char* root, const char* workDir)
{
    XL_ContextOptions options;
    XL_Context* ctx;
    char path[1024];

    options = xlDefaultContextOptions();
    options.workDir = workDir;
    options.tempDir = "release\\tmp";
    if ( !make_full_path(root, "release\\libtcc.dll", path, sizeof(path)) ) {
        return NULL;
    }
    options.tccDllPath = path;
    {
        char tccLibPath[1024];
        if ( !make_full_path(root, "..\\demo5\\release\\tcc", tccLibPath, sizeof(tccLibPath)) ) {
            return NULL;
        }
        options.tccLibPath = tccLibPath;
        ctx = xlCreateContext(&options);
    }
    if ( ctx == NULL ) {
        return NULL;
    }
    if ( !add_default_include_paths(ctx, root) ) {
        fprintf(stderr, "cannot add absolute include paths: %s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return NULL;
    }
    return ctx;
}

static int run_cwd_success_case(const char* root, const char* originalCwd)
{
    XL_Context* ctx;
    const char* workDir = "release\\tmp\\dll_cwd_a";
    const char* marker = "release\\tmp\\dll_cwd_a\\cwd_probe.txt";
    const char* script =
        "file.writeText(\"cwd_probe.txt\", \"ok\")\n"
        "assert file.exists(\"cwd_probe.txt\"), \"cwd probe missing\"\n";

    if ( !make_dir("release\\tmp\\dll_cwd_a") ) {
        fprintf(stderr, "cannot create dll_cwd_a\n");
        return 0;
    }
    delete_file_if_exists(marker);
    ctx = create_context_for_workdir(root, workDir);
    if ( ctx == NULL ) {
        fprintf(stderr, "cannot create cwd success context\n");
        return 0;
    }
    if ( !xlRunScriptString(ctx, script) ) {
        fprintf(stderr, "cwd success script failed: %s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 0;
    }
    xlDestroyContext(ctx);
    if ( !file_exists(marker) ) {
        fprintf(stderr, "cwd success script did not write marker in workDir\n");
        return 0;
    }
    return expect_cwd("cwd success script", originalCwd);
}

static int run_cwd_failure_case(const char* root, const char* originalCwd)
{
    XL_Context* ctx;
    const char* workDir = "release\\tmp\\dll_cwd_b";

    if ( !make_dir("release\\tmp\\dll_cwd_b") ) {
        fprintf(stderr, "cannot create dll_cwd_b\n");
        return 0;
    }
    ctx = create_context_for_workdir(root, workDir);
    if ( ctx == NULL ) {
        fprintf(stderr, "cannot create cwd failure context\n");
        return 0;
    }
    if ( xlRunScriptString(ctx, "func broken(\n") ) {
        fprintf(stderr, "cwd failure script unexpectedly succeeded\n");
        xlDestroyContext(ctx);
        return 0;
    }
    if ( strstr(xlGetLastError(ctx), "[parse]") == NULL ) {
        fprintf(stderr, "cwd failure script missing parse diagnostic: %s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 0;
    }
    xlDestroyContext(ctx);
    return expect_cwd("cwd failure script", originalCwd);
}

int main(void)
{
    XL_ContextOptions options;
    XL_Context* ctx;
    char originalCwd[1024];

    xlDestroyContext(NULL);
    xlClearLastError(NULL);
    if ( !expect_empty_error("xlGetLastError(NULL)") ) {
        return 1;
    }

    if ( !expect_false("xlSetWorkDir(NULL)", xlSetWorkDir(NULL, ".")) ||
         !expect_false("xlSetTempDir(NULL)", xlSetTempDir(NULL, "release\\tmp")) ||
         !expect_false("xlSetSaveTemps(NULL)", xlSetSaveTemps(NULL, 1)) ||
         !expect_false("xlAddIncludePath(NULL)", xlAddIncludePath(NULL, ".")) ||
         !expect_false("xlAddLibraryPath(NULL)", xlAddLibraryPath(NULL, "release\\tmp")) ||
         !expect_false("xlAddLinkLibrary(NULL)", xlAddLinkLibrary(NULL, "x")) ||
         !expect_false("xlLoadDependencyLibrary(NULL)", xlLoadDependencyLibrary(NULL, "x.dll")) ||
         !expect_false("xlRegisterCFunction(NULL)", xlRegisterCFunction(NULL, "dummy", (void*)dummy_host_function)) ||
         !expect_false("xlRunScriptString(NULL)", xlRunScriptString(NULL, "int a = 1")) ||
         !expect_false("xlRunScriptFile(NULL)", xlRunScriptFile(NULL, "missing.xl")) ||
         !expect_false("xlCompileScriptStringToExe(NULL)", xlCompileScriptStringToExe(NULL, "int a = 1", "release\\tmp\\null.exe")) ||
         !expect_false("xlCompileScriptFileToExe(NULL)", xlCompileScriptFileToExe(NULL, "missing.xl", "release\\tmp\\null.exe")) ) {
        return 1;
    }

    if ( !expect_empty_error("NULL-context API calls") ) {
        return 1;
    }

    options = xlDefaultContextOptions();
    options.size = 1;
    if ( xlCreateContext(&options) != NULL ) {
        fprintf(stderr, "xlCreateContext accepted undersized options\n");
        return 1;
    }

    options = xlDefaultContextOptions();
    options.version = XL_API_VERSION + 1;
    if ( xlCreateContext(&options) != NULL ) {
        fprintf(stderr, "xlCreateContext accepted unsupported API version\n");
        return 1;
    }

    options = xlDefaultContextOptions();
    options.workDir = ".";
    options.tempDir = "release\\tmp";
    options.tccDllPath = "release\\libtcc.dll";
    ctx = xlCreateContext(&options);
    if ( ctx == NULL ) {
        fprintf(stderr, "xlCreateContext default options failed\n");
        return 1;
    }
    if ( !expect_api_failure(ctx, "xlRunScriptString NULL source", xlRunScriptString(ctx, NULL), "script source is empty") ||
         !expect_api_failure(ctx, "xlRunScriptFile empty path", xlRunScriptFile(ctx, ""), "script file path is empty") ||
         !expect_api_failure(ctx, "xlCompileScriptStringToExe NULL source", xlCompileScriptStringToExe(ctx, NULL, "release\\tmp\\null_source.exe"), "script source is empty") ||
         !expect_api_failure(ctx, "xlCompileScriptFileToExe empty path", xlCompileScriptFileToExe(ctx, "", "release\\tmp\\empty_file.exe"), "script file path is empty") ||
         !expect_api_failure(ctx, "xlCompileScriptStringToExe empty output", xlCompileScriptStringToExe(ctx, "func ok() -> int\n    return 1\nend\n", ""), "output exe path is empty") ) {
        xlDestroyContext(ctx);
        return 1;
    }
    xlDestroyContext(ctx);

    if ( !get_current_dir(originalCwd, sizeof(originalCwd)) ) {
        fprintf(stderr, "cannot query initial current directory\n");
        return 1;
    }
    if ( !run_cwd_success_case(originalCwd, originalCwd) ||
         !run_cwd_failure_case(originalCwd, originalCwd) ) {
        return 1;
    }

    printf("xl.dll NULL context and cwd boundary ok\n");
    return 0;
}
