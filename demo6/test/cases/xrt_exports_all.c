#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static const char* const names[] = {
#define XL_XRT_SYMBOL(name) #name,
#include "src/api/xl_xrt_symbols.inc"
#undef XL_XRT_SYMBOL
};

int main(int argc, char** argv)
{
    if (argc != 2 || sizeof(names)/sizeof(*names) < 1000) return 2;
#ifdef _WIN32
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[1], -1, NULL, 0);
    wchar_t* path = size > 0 ? malloc((size_t)size * sizeof(*path)) : NULL;
    if (!path) return 2;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[1], -1, path, size)) { free(path); return 2; }
    HMODULE library = LoadLibraryW(path); free(path);
    if (!library) return 3;
#define FIND(name) GetProcAddress(library, name)
#else
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { fprintf(stderr, "%s\n", dlerror()); return 3; }
#define FIND(name) dlsym(library, name)
#endif
    size_t missing = 0;
    for (size_t i=0; i<sizeof(names)/sizeof(*names); ++i) {
        if (!FIND(names[i])) { fprintf(stderr, "missing export: %s\n", names[i]); ++missing; }
    }
    if (FIND("__xlang_missing_xrt_export_negative_control")) return 4;
#ifdef _WIN32
    FreeLibrary(library);
#else
    dlclose(library);
#endif
    printf("XRT exports: %zu declared, %zu missing; missing-name control rejected\n",
        sizeof(names)/sizeof(*names), missing);
    return missing ? 1 : 0;
}
