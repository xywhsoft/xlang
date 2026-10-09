/* Standalone product only; no injected providers or xlang import. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
typedef int (*Init)(void*);
typedef int (*Unit)(void);
typedef int64_t (*Probe)(void);
int main(int argc,char** argv)
{
    assert(argc==2);
#ifdef _WIN32
    HMODULE image=LoadLibraryA(argv[1]); assert(image);
    Init init=(Init)(void*)GetProcAddress(image,"xl_script_module_init");
    Unit unit=(Unit)(void*)GetProcAddress(image,"xl_script_module_unit_checked");
    Probe probe=(Probe)(void*)GetProcAddress(image,"xl6_export_stackBatchProbe");
#else
    void* image=dlopen(argv[1],RTLD_NOW | RTLD_LOCAL);
    if (!image) fprintf(stderr,"dlopen: %s\n",dlerror());
    assert(image);
    Init init=(Init)dlsym(image,"xl_script_module_init");
    Unit unit=(Unit)dlsym(image,"xl_script_module_unit_checked");
    Probe probe=(Probe)dlsym(image,"xl6_export_stackBatchProbe");
#endif
    assert(init && unit && probe && init(NULL) && probe()==42 && unit());
    puts("independent Stack artifact and checked cleanup passed");
    /* Keep the runtime image resident until process exit. This gate does not
     * establish a general native-runtime unloading contract. */
    return 0;
}
