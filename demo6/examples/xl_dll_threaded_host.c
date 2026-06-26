#include "include/xl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#error xl_dll_threaded_host currently targets the Windows xl.dll test path.
#endif

#define XL_THREAD_COUNT 4

typedef struct ThreadArg {
    int index;
    int ok;
    char error[1024];
    char root[1024];
    char workDir[1024];
    char tempDir[1024];
    char tccDllPath[1024];
    char tccLibPath[1024];
    char includeRoot[1024];
    char includeSrc[1024];
    char includeXrt[1024];
    char includeTcc[1024];
    char includeWin[1024];
    char includeWinapi[1024];
    char libPath[1024];
    char marker[1024];
} ThreadArg;

static long long host_thread_value(long long value)
{
    return value + 1000;
}

static int get_current_dir(char* out, size_t outSize)
{
    DWORD got = GetCurrentDirectoryA((DWORD)outSize, out);
    return got > 0 && got < outSize;
}

static int make_full_path(const char* base, const char* rel, char* out, size_t outSize)
{
    char joined[1024];
    int written = snprintf(joined, sizeof(joined), "%s\\%s", base, rel);
    DWORD got;

    if ( written <= 0 || (size_t)written >= sizeof(joined) ) {
        return 0;
    }
    got = GetFullPathNameA(joined, (DWORD)outSize, out, NULL);
    return got > 0 && got < outSize;
}

static int is_absolute_path(const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    return (path[0] != '\0' && path[1] == ':') || (path[0] == '\\' && path[1] == '\\');
}

static int make_toolchain_path(const char* root, const char* envName, char* out, size_t outSize)
{
    const char* value = getenv(envName);
    DWORD got;

    if ( value == NULL || value[0] == '\0' ) {
        return 0;
    }
    if ( is_absolute_path(value) ) {
        got = GetFullPathNameA(value, (DWORD)outSize, out, NULL);
        return got > 0 && got < outSize;
    }
    return make_full_path(root, value, out, outSize);
}

static int make_dir(const char* path)
{
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static void delete_file_if_exists(const char* path)
{
    DeleteFileA(path);
}

static int file_exists(const char* path)
{
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void set_error(ThreadArg* arg, const char* text)
{
    snprintf(arg->error, sizeof(arg->error), "thread %d: %s", arg->index, text != NULL ? text : "error");
}

static int add_default_paths(XL_Context* ctx, ThreadArg* arg)
{
    if ( !xlAddIncludePath(ctx, arg->includeRoot) ||
         !xlAddIncludePath(ctx, arg->includeSrc) ||
         !xlAddIncludePath(ctx, arg->includeXrt) ||
         !xlAddIncludePath(ctx, arg->includeTcc) ||
         !xlAddIncludePath(ctx, arg->includeWin) ||
         !xlAddIncludePath(ctx, arg->includeWinapi) ||
         !xlAddLibraryPath(ctx, arg->libPath) ) {
        set_error(arg, xlGetLastError(ctx));
        return 0;
    }
    return 1;
}

static DWORD WINAPI thread_proc(LPVOID param)
{
    ThreadArg* arg = (ThreadArg*)param;
    XL_ContextOptions options;
    XL_Context* ctx;
    char source[1024];
    int expected;

    arg->ok = 0;
    expected = arg->index + 1000;

    options = xlDefaultContextOptions();
    options.workDir = arg->workDir;
    options.tempDir = arg->tempDir;
    options.tccDllPath = arg->tccDllPath;
    options.tccLibPath = arg->tccLibPath;

    ctx = xlCreateContext(&options);
    if ( ctx == NULL ) {
        set_error(arg, "cannot create context");
        return 1;
    }
    if ( !add_default_paths(ctx, arg) ||
         !xlRegisterCFunction(ctx, "host_thread_value", (void*)host_thread_value) ) {
        set_error(arg, xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }

    snprintf(source,
             sizeof(source),
             "extern \"host_thread_value\" func hostThreadValue(int value) -> int\n"
             "int value = hostThreadValue(%d)\n"
             "assert value == %d, \"host thread value failed\"\n"
             "file.writeText(\"thread_marker_%d.txt\", \"ok\")\n"
             "assert file.exists(\"thread_marker_%d.txt\"), \"thread marker missing\"\n",
             arg->index,
             expected,
             arg->index,
             arg->index);

    if ( !xlRunScriptString(ctx, source) ) {
        set_error(arg, xlGetLastError(ctx));
        xlDestroyContext(ctx);
        return 1;
    }
    xlDestroyContext(ctx);
    if ( !file_exists(arg->marker) ) {
        set_error(arg, "marker was not written");
        return 1;
    }
    arg->ok = 1;
    return 0;
}

static int init_arg(ThreadArg* arg, const char* root, int index)
{
    char rel[256];

    memset(arg, 0, sizeof(*arg));
    arg->index = index;
    snprintf(arg->root, sizeof(arg->root), "%s", root);
    snprintf(arg->includeRoot, sizeof(arg->includeRoot), "%s", root);

    snprintf(rel, sizeof(rel), "release\\tmp\\dll_thread_%d", index);
    if ( !make_full_path(root, rel, arg->workDir, sizeof(arg->workDir)) ||
         !make_full_path(root, "release\\tmp", arg->tempDir, sizeof(arg->tempDir)) ||
         !make_full_path(root, "release\\libtcc.dll", arg->tccDllPath, sizeof(arg->tccDllPath)) ||
         !make_toolchain_path(root, "XL6_TCC_ROOT", arg->tccLibPath, sizeof(arg->tccLibPath)) ||
         !make_full_path(root, "src", arg->includeSrc, sizeof(arg->includeSrc)) ||
         !make_full_path(root, "lib\\xrt", arg->includeXrt, sizeof(arg->includeXrt)) ||
         !make_toolchain_path(root, "XL6_TCC_INCLUDE", arg->includeTcc, sizeof(arg->includeTcc)) ||
         !make_toolchain_path(root, "XL6_TCC_WIN_INCLUDE", arg->includeWin, sizeof(arg->includeWin)) ||
         !make_toolchain_path(root, "XL6_TCC_WINAPI_INCLUDE", arg->includeWinapi, sizeof(arg->includeWinapi)) ||
         !make_toolchain_path(root, "XL6_TCC_LIB", arg->libPath, sizeof(arg->libPath)) ) {
        set_error(arg, "cannot build absolute paths");
        return 0;
    }
    if ( !make_dir(arg->workDir) ) {
        set_error(arg, "cannot create workDir");
        return 0;
    }
    snprintf(arg->marker, sizeof(arg->marker), "%s\\thread_marker_%d.txt", arg->workDir, index);
    delete_file_if_exists(arg->marker);
    return 1;
}

int main(void)
{
    char root[1024];
    char originalCwd[1024];
    ThreadArg args[XL_THREAD_COUNT];
    HANDLE threads[XL_THREAD_COUNT];
    int i;
    int ok;

    if ( !get_current_dir(root, sizeof(root)) || !get_current_dir(originalCwd, sizeof(originalCwd)) ) {
        fprintf(stderr, "cannot query current directory\n");
        return 1;
    }

    for ( i = 0; i < XL_THREAD_COUNT; ++i ) {
        if ( !init_arg(&args[i], root, i) ) {
            fprintf(stderr, "%s\n", args[i].error);
            return 1;
        }
    }

    for ( i = 0; i < XL_THREAD_COUNT; ++i ) {
        threads[i] = CreateThread(NULL, 0, thread_proc, &args[i], 0, NULL);
        if ( threads[i] == NULL ) {
            fprintf(stderr, "cannot create thread %d\n", i);
            return 1;
        }
    }
    WaitForMultipleObjects(XL_THREAD_COUNT, threads, TRUE, INFINITE);
    for ( i = 0; i < XL_THREAD_COUNT; ++i ) {
        CloseHandle(threads[i]);
    }

    ok = 1;
    for ( i = 0; i < XL_THREAD_COUNT; ++i ) {
        if ( !args[i].ok ) {
            fprintf(stderr, "%s\n", args[i].error[0] != '\0' ? args[i].error : "thread failed");
            ok = 0;
        }
    }
    if ( !get_current_dir(root, sizeof(root)) || strcmp(root, originalCwd) != 0 ) {
        fprintf(stderr, "threaded host changed cwd: %s\n", root);
        ok = 0;
    }
    if ( !ok ) {
        return 1;
    }

    printf("xl.dll threaded host API boundary ok\n");
    return 0;
}
