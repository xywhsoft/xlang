/* The DLL owns its embedded runtime. Never use a second runtime to interpret
 * its values: this test crosses only an int64_t export and checked cleanup. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
int main(int argc, char** argv)
{
    typedef int64_t (*Check)(void);
    typedef int (*Cleanup)(void);
    typedef void (*PrintError)(void);
    HMODULE library;
    FARPROC symbol;
    Check check; Cleanup cleanup, retire; PrintError printError;
    int64_t* counter = NULL;
    const int64_t* offset = NULL;
    int ok;
    if (argc != 2 && argc != 3) return 2;
    library = LoadLibraryA(argv[1]);
    if (!library) { fprintf(stderr, "LoadLibrary failed: %lu\n", (unsigned long)GetLastError()); return 3; }
    symbol = GetProcAddress(library, "xlir_artifact_check"); memcpy(&check, &symbol, sizeof(check));
    symbol = GetProcAddress(library, "xl_script_module_unit_checked"); memcpy(&cleanup, &symbol, sizeof(cleanup));
    symbol = GetProcAddress(library, "xl_script_runtime_retire_checked"); memcpy(&retire, &symbol, sizeof(retire));
    symbol = GetProcAddress(library, "xl_script_print_uncaught"); memcpy(&printError, &symbol, sizeof(printError));
    if (!check || !cleanup || !retire || !printError) { fprintf(stderr, "DLL contract symbols missing\n"); return 4; }
    if (argc == 3) {
        counter = (int64_t*)(void*)GetProcAddress(library, "xlir_artifact_counter");
        offset = (const int64_t*)(void*)GetProcAddress(library, "xlir_artifact_offset");
        if (!counter || !offset || *counter != 41 || *offset != 1) return 8;
    }
    ok = check() == 42;
    if (counter && *counter != 42) ok = 0;
    if (!ok) printError();
    /* Do not unload code if quiescence/finalization failed. Process exit is the
     * isolation boundary for a failing probe, not a claimed safe unload. */
    if (!cleanup()) { printError(); return 5; }
    /* Terminal runtime retirement is distinct from reusable module cleanup.
     * On failure no XRT-based diagnostic call or unload is allowed. */
    if (!retire() || !retire()) return 9;
    if (!FreeLibrary(library)) return 6;
    if (!ok) return 7;
    puts("artifact DLL check and cleanup complete");
    return 0;
}
