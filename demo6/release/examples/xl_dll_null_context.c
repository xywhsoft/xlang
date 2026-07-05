#ifndef XL6_EXAMPLE_BOUNDARY_TEST
#include "include/xl.h"

#include <stdio.h>

int main(void)
{
    XL_ContextOptions options = xlDefaultContextOptions();
    XL_Context* ctx;

    if ( xlGetLastError(NULL)[0] != '\0' ) {
        fprintf(stderr, "NULL context error should be empty\n");
        return 1;
    }

    ctx = xlCreateContext(&options);
    if ( ctx == NULL ) {
        fprintf(stderr, "cannot create xl context\n");
        return 2;
    }
    if ( !xlRunScriptString(ctx, "assert 40 + 2 == 42, \"null context example failed\"\nprintln(\"xl dll null context ok\")\n") ) {
        fprintf(stderr, "%s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 3;
    }
    xlDestroyContext(ctx);
    return 0;
}
#else
#include "include/xl.h"

#include <stdio.h>
#include <stdlib.h>
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

static int is_absolute_path(const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
#ifdef _WIN32
    return (path[0] != '\0' && path[1] == ':') || (path[0] == '\\' && path[1] == '\\');
#else
    return path[0] == '/';
#endif
}

static int make_toolchain_path(const char* root, const char* envName, char* out, size_t outSize)
{
    const char* value = getenv(envName);

    if ( value == NULL || value[0] == '\0' ) {
        fprintf(stderr, "missing toolchain environment variable: %s\n", envName);
        return 0;
    }
    if ( is_absolute_path(value) ) {
#ifdef _WIN32
        DWORD got = GetFullPathNameA(value, (DWORD)outSize, out, NULL);
        return got > 0 && got < outSize;
#else
        return realpath(value, out) != NULL;
#endif
    }
    return make_full_path(root, value, out, outSize);
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
    if ( !make_toolchain_path(root, "XL6_TCC_INCLUDE", path, sizeof(path)) || !xlAddIncludePath(ctx, path) ) {
        return 0;
    }
    if ( !make_toolchain_path(root, "XL6_TCC_WIN_INCLUDE", path, sizeof(path)) || !xlAddIncludePath(ctx, path) ) {
        return 0;
    }
    if ( !make_toolchain_path(root, "XL6_TCC_WINAPI_INCLUDE", path, sizeof(path)) || !xlAddIncludePath(ctx, path) ) {
        return 0;
    }
    if ( !make_toolchain_path(root, "XL6_TCC_LIB", path, sizeof(path)) || !xlAddLibraryPath(ctx, path) ) {
        return 0;
    }
    return 1;
}

static XL_Context* create_context_for_workdir(const char* root, const char* workDir)
{
    XL_ContextOptions options;
    XL_Context* ctx;

    options = xlDefaultContextOptions();
    options.workDir = workDir;
    options.tempDir = "build\\test";
    {
        char tccLibPath[1024];
        if ( !make_toolchain_path(root, "XL6_TCC_ROOT", tccLibPath, sizeof(tccLibPath)) ) {
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
    const char* workDir = "build\\test\\dll_cwd_a";
    const char* marker = "build\\test\\dll_cwd_a\\cwd_probe.txt";
    const char* script =
        "file.writeText(\"cwd_probe.txt\", \"ok\")\n"
        "assert file.exists(\"cwd_probe.txt\"), \"cwd probe missing\"\n";

    if ( !make_dir("build\\test\\dll_cwd_a") ) {
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
    const char* workDir = "build\\test\\dll_cwd_b";

    if ( !make_dir("build\\test\\dll_cwd_b") ) {
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

static int run_default_toolchain_snapshot_case(const char* originalCwd)
{
    XL_ContextOptions options;
    XL_Context* ctx;
    const char* workDir = "build\\test\\dll_cwd_default_toolchain";
    const char* marker = "build\\test\\dll_cwd_default_toolchain\\default_toolchain_marker.txt";
    const char* script =
        "assert string.trim(\" ok \") == \"ok\", \"default toolchain string failed\"\n"
        "file.writeText(\"default_toolchain_marker.txt\", \"ok\")\n"
        "assert file.exists(\"default_toolchain_marker.txt\"), \"default toolchain marker missing\"\n";

    if ( !make_dir("build\\test\\dll_cwd_default_toolchain") ) {
        fprintf(stderr, "cannot create dll_cwd_default_toolchain\n");
        return 0;
    }
    delete_file_if_exists(marker);

    options = xlDefaultContextOptions();
    options.workDir = workDir;
    options.tempDir = "build\\test";

    ctx = xlCreateContext(&options);
    if ( ctx == NULL ) {
        fprintf(stderr, "cannot create default toolchain context\n");
        return 0;
    }
    if ( !xlRunScriptString(ctx, script) ) {
        fprintf(stderr, "default toolchain snapshot script failed: %s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 0;
    }
    xlDestroyContext(ctx);

    if ( !file_exists(marker) ) {
        fprintf(stderr, "default toolchain snapshot script did not write marker\n");
        return 0;
    }
    return expect_cwd("default toolchain snapshot script", originalCwd);
}

static int run_dependency_path_snapshot_case(const char* originalCwd)
{
    XL_ContextOptions options;
    XL_Context* ctx;
    const char* workDir = "build\\test\\dll_dependency_snapshot";
    const char* marker = "build\\test\\dll_dependency_snapshot\\dependency_snapshot_marker.txt";
    const char* script =
        "extern \"xl6_dll_link_value\" func linkedValue() as int\n"
        "extern \"xl6_dll_loadlib_value\" func loadedValue() as int\n"
        "assert linkedValue() == 42, \"snapshot link library failed\"\n"
        "assert loadedValue() == 42, \"snapshot loadlib failed\"\n"
        "file.writeText(\"dependency_snapshot_marker.txt\", \"ok\")\n"
        "assert file.exists(\"dependency_snapshot_marker.txt\"), \"snapshot marker missing\"\n";

    if ( !make_dir("build\\test\\dll_dependency_snapshot") ) {
        fprintf(stderr, "cannot create dll_dependency_snapshot\n");
        return 0;
    }
    delete_file_if_exists(marker);

    options = xlDefaultContextOptions();
    options.workDir = workDir;
    options.tempDir = "build\\test";

    ctx = xlCreateContext(&options);
    if ( ctx == NULL ) {
        fprintf(stderr, "cannot create dependency snapshot context\n");
        return 0;
    }
    if ( !xlAddLibraryPath(ctx, "build\\tmp") ||
         !xlAddLinkLibrary(ctx, "xl6dlllink") ||
          !xlLoadDependencyLibrary(ctx, "build\\tmp\\xl_dll_loadlib_host.dll") ||
          !xlAddLinkLibrary(ctx, "build\\tmp\\xl_dll_loadlib_host.def") ) {
        fprintf(stderr, "cannot add dependency snapshot paths: %s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 0;
    }
    if ( !xlRunScriptString(ctx, script) ) {
        fprintf(stderr, "dependency path snapshot script failed: %s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 0;
    }
    xlDestroyContext(ctx);

    if ( !file_exists(marker) ) {
        fprintf(stderr, "dependency path snapshot script did not write marker\n");
        return 0;
    }
    return expect_cwd("dependency path snapshot script", originalCwd);
}

int main(void)
{
    XL_ContextOptions options;
    XL_Context* ctx;
    const char* nullCPaths[1];
    char originalCwd[1024];

    if ( !xlRuntimeInit() ) {
        fprintf(stderr, "xlRuntimeInit failed\n");
        return 1;
    }
    if ( !xlRuntimeInit() ) {
        fprintf(stderr, "second xlRuntimeInit failed\n");
        xlRuntimeUnit();
        return 1;
    }
    xlRuntimeUnit();
    xlRuntimeUnit();

    xlDestroyContext(NULL);
    xlClearLastError(NULL);
    if ( !expect_empty_error("xlGetLastError(NULL)") ) {
        return 1;
    }

    if ( !expect_false("xlSetWorkDir(NULL)", xlSetWorkDir(NULL, ".")) ||
         !expect_false("xlSetTempDir(NULL)", xlSetTempDir(NULL, "build\\test")) ||
         !expect_false("xlSetSaveTemps(NULL)", xlSetSaveTemps(NULL, 1)) ||
         !expect_false("xlAddIncludePath(NULL)", xlAddIncludePath(NULL, ".")) ||
         !expect_false("xlAddLibraryPath(NULL)", xlAddLibraryPath(NULL, "build\\test")) ||
         !expect_false("xlAddLinkLibrary(NULL)", xlAddLinkLibrary(NULL, "x")) ||
         !expect_false("xlLoadDependencyLibrary(NULL)", xlLoadDependencyLibrary(NULL, "x.dll")) ||
         !expect_false("xlRegisterCFunction(NULL)", xlRegisterCFunction(NULL, "dummy", (void*)dummy_host_function)) ||
         !expect_false("xlRunScriptString(NULL)", xlRunScriptString(NULL, "int a = 1")) ||
         !expect_false("xlRunScriptFile(NULL)", xlRunScriptFile(NULL, "missing.xl")) ||
         !expect_false("xlRunScriptStringArg(NULL)", xlRunScriptStringArg(NULL, "int a = 1", NULL)) ||
         !expect_false("xlRunScriptFileArg(NULL)", xlRunScriptFileArg(NULL, "missing.xl", NULL)) ) {
        return 1;
    }
#if !defined(XL_RUNTIME_ONLY_API)
    if ( !expect_false("xlCompileScriptStringToExe(NULL)", xlCompileScriptStringToExe(NULL, "int a = 1", "build\\test\\null.exe")) ||
         !expect_false("xlCompileScriptFileToExe(NULL)", xlCompileScriptFileToExe(NULL, "missing.xl", "build\\test\\null.exe")) ) {
        return 1;
    }
#endif
    nullCPaths[0] = "missing.c";
    if ( !expect_false("xlCompileCSourceFilesToModule(NULL)", xlCompileCSourceFilesToModule(NULL, nullCPaths, 1) != NULL) ||
         !expect_false("xlRunCSourceFiles(NULL)", xlRunCSourceFiles(NULL, nullCPaths, 1, 0, NULL)) ||
         !expect_false("xlTccRegisterXrtEnvironment(NULL)", xlTccRegisterXrtEnvironment(NULL)) ||
         !expect_false("xlTccRegisterContextEnvironment(NULL)", xlTccRegisterContextEnvironment(NULL, NULL)) ||
         !expect_false("xlTccImpdef(NULL)", xlTccImpdef(NULL, "build\\test\\null.def")) ) {
        return 1;
    }
#if !defined(XL_RUNTIME_ONLY_API)
    if ( !expect_false("xlCompileCSourceFileToExe(NULL)", xlCompileCSourceFileToExe(NULL, "missing.c", "build\\test\\null_c.exe")) ||
         !expect_false("xlCompileCSourceFilesToExe(NULL)", xlCompileCSourceFilesToExe(NULL, nullCPaths, 1, "build\\test\\null_c.exe")) ||
         !expect_false("xlCompileCSourceFileToSharedLibrary(NULL)", xlCompileCSourceFileToSharedLibrary(NULL, "missing.c", "build\\test\\null_c.dll")) ||
         !expect_false("xlCompileCSourceFilesToSharedLibrary(NULL)", xlCompileCSourceFilesToSharedLibrary(NULL, nullCPaths, 1, "build\\test\\null_c.dll")) ) {
        return 1;
    }
#endif

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
    options.tempDir = "build\\test";
    ctx = xlCreateContext(&options);
    if ( ctx == NULL ) {
        fprintf(stderr, "xlCreateContext default options failed\n");
        return 1;
    }
    xlRuntimeUnit();
    if ( !xlRunScriptString(ctx, "int value = 21 * 2\nassert value == 42, \"context runtime ref failed\"\n") ) {
        fprintf(stderr, "context did not survive unmatched xlRuntimeUnit: %s\n", xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }
    if ( !expect_api_failure(ctx, "xlRunScriptString NULL source", xlRunScriptString(ctx, NULL), "script source is empty") ||
         !expect_api_failure(ctx, "xlRunScriptFile empty path", xlRunScriptFile(ctx, ""), "script file path is empty") ) {
        xlDestroyContext(ctx);
        return 1;
    }
#if !defined(XL_RUNTIME_ONLY_API)
    if ( !expect_api_failure(ctx, "xlCompileScriptStringToExe NULL source", xlCompileScriptStringToExe(ctx, NULL, "build\\test\\null_source.exe"), "script source is empty") ||
         !expect_api_failure(ctx, "xlCompileScriptFileToExe empty path", xlCompileScriptFileToExe(ctx, "", "build\\test\\empty_file.exe"), "script file path is empty") ||
         !expect_api_failure(ctx, "xlCompileScriptStringToExe empty output", xlCompileScriptStringToExe(ctx, "func ok() as int\n    return 1\nend\n", ""), "output exe path is empty") ) {
        xlDestroyContext(ctx);
        return 1;
    }
#endif
    xlDestroyContext(ctx);

    if ( !get_current_dir(originalCwd, sizeof(originalCwd)) ) {
        fprintf(stderr, "cannot query initial current directory\n");
        return 1;
    }
    if ( !run_cwd_success_case(originalCwd, originalCwd) ||
         !run_cwd_failure_case(originalCwd, originalCwd) ||
         !run_default_toolchain_snapshot_case(originalCwd) ||
         !run_dependency_path_snapshot_case(originalCwd) ) {
        return 1;
    }

    printf("xl.dll NULL context and cwd boundary ok\n");
    return 0;
}
#endif
