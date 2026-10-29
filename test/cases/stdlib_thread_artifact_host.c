/* Independent consumer: no xlang dependency or injected providers. Retain
 * the image until process exit on BOTH platforms; logical wait is not join. */
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
    Probe probe=(Probe)(void*)GetProcAddress(image,"xl6_export_threadProbe");
#else
    void* image=dlopen(argv[1],RTLD_NOW | RTLD_LOCAL);
    if (!image) fprintf(stderr,"dlopen: %s\n",dlerror());
    assert(image);
    Init init=(Init)dlsym(image,"xl_script_module_init");
    Unit unit=(Unit)dlsym(image,"xl_script_module_unit_checked");
    Probe probe=(Probe)dlsym(image,"xl6_export_threadProbe");
#endif
    assert(init && unit && probe && init(NULL) && probe()==42 && unit());
    puts("independent Thread artifact: exported program and checked cleanup passed; image retained until process exit");
    return 0;
}
