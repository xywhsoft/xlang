/* Consume a genuine independently generated DLL, without linking xl.dll or
 * injecting providers. Private providers are deliberately not DLL exports;
 * their absence cannot be inferred from GetProcAddress. */
#include "xl_sync_retirement.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>
typedef int (*Init)(xvalue*);
typedef int (*Unit)(void);
typedef int64_t (*Probe)(void);
int main(int argc,char** argv)
{
    assert(argc==2);
    HMODULE image=LoadLibraryA(argv[1]); assert(image);
    Init init=(Init)(void*)GetProcAddress(image,"xl_script_module_init");
    Unit unit=(Unit)(void*)GetProcAddress(image,"xl_script_module_unit_checked");
    Unit retire=(Unit)(void*)GetProcAddress(image,"xl_script_runtime_retire_checked");
    Probe probe=(Probe)(void*)GetProcAddress(image,"xl6_export_syncProbe");
    assert(init && unit && retire && probe);
    assert(init(NULL) && probe()==42 && unit());
    /* module Unit is resettable; DLL unload also needs terminal runtime/TLS
     * retirement. Skipping it leaves Windows FLS callbacks in the old image. */
    assert(retire() && retire());
    assert(FreeLibrary(image));
    puts("standalone DLL: actual exported program and checked cleanup passed");
    return 0;
}
