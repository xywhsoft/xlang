/* Consume a genuine independently generated DLL/SO, without linking xlang
 * shared libraries or injecting providers. Private provider absence cannot
 * be inferred from the dynamic export table. */
#include "xl_sync_retirement.h"
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
typedef int (*Init)(xvalue*);
typedef int (*Unit)(void);
typedef int64_t (*Probe)(void);
int main(int argc,char** argv)
{
    assert(argc==2);
#ifdef _WIN32
    HMODULE image=LoadLibraryA(argv[1]); assert(image);
    Init init=(Init)(void*)GetProcAddress(image,"xl_script_module_init");
    Unit unit=(Unit)(void*)GetProcAddress(image,"xl_script_module_unit_checked");
    Unit retire=(Unit)(void*)GetProcAddress(image,"xl_script_runtime_retire_checked");
    Probe probe=(Probe)(void*)GetProcAddress(image,"xl6_export_syncProbe");
#else
    void* image=dlopen(argv[1],RTLD_NOW | RTLD_LOCAL);
    if (!image) fprintf(stderr,"dlopen: %s\n",dlerror());
    assert(image);
    Init init=(Init)dlsym(image,"xl_script_module_init");
    Unit unit=(Unit)dlsym(image,"xl_script_module_unit_checked");
    Probe probe=(Probe)dlsym(image,"xl6_export_syncProbe");
#endif
    assert(init && unit && probe);
    assert(init(NULL) && probe()==42 && unit());
#ifdef _WIN32
    assert(retire);
    /* module Unit is resettable; DLL unload also needs terminal runtime/TLS
     * retirement. Skipping it leaves Windows FLS callbacks in the old image. */
    assert(retire() && retire());
    assert(FreeLibrary(image));
    puts("standalone DLL: actual exported program and checked cleanup passed");
#else
    /* Terminal internal TLS retirement is not a supported POSIX contract.
     * Keep the image loaded until process exit: this tests ELF execution and
     * module Unit, not safe dlclose or reclaiming pthread-key callbacks. */
    puts("standalone ELF: actual exported program and checked module cleanup passed; image retained until process exit");
#endif
    return 0;
}
