#include "xlir_foreign_resource_module.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
typedef HMODULE Library;
static Library load(const char* path){return LoadLibraryA(path);}
static void* symbol(Library lib,const char* name){return (void*)GetProcAddress(lib,name);}
static bool unload(Library lib){return FreeLibrary(lib)!=0;}
#else
#include <dlfcn.h>
typedef void* Library;
static Library load(const char* path){return dlopen(path,RTLD_NOW|RTLD_LOCAL);}
static void* symbol(Library lib,const char* name){return dlsym(lib,name);}
static bool unload(Library lib){return dlclose(lib)==0;}
#endif
typedef struct Owner {
    Library library;unsigned pins;bool closed;ForeignModuleHooks* hooks;
} Owner;
static unsigned drops;
static int acquire(void* data)
{
    Owner* owner=data;if(owner->closed)return 0;++owner->pins;return 1;
}
static void release(void* data)
{
    Owner* owner=data;assert(owner->pins);
    if(!--owner->pins && owner->closed){
        assert(owner->hooks->stages==2 && unload(owner->library));owner->library=NULL;
    }
}
static void dispose(const xrtcallframe* frame)
{
    void* raw=NULL;assert(frame->ArgumentCount==1 && xrtValueGetPointer(frame->Arguments[0],&raw));
    assert(*(int*)raw==42);free(raw);++drops;
}
static void unused(void){abort();}
int main(int argc,char** argv)
{
    assert(argc==3);ForeignModuleHooks hooks={.dispose=dispose};
    Owner a={.library=load(argv[1]),.hooks=&hooks},b={.library=load(argv[2]),.hooks=&hooks};
    assert(a.library && b.library);
    hooks.producer=(ForeignModuleProducer)symbol(a.library,"foreign_module_producer");
    XL6_ClosureBoxEntry entry=(XL6_ClosureBoxEntry)symbol(b.library,"foreign_module_adapter");
    assert(hooks.producer && entry);
    const XL6_CodeOwner producer={sizeof(producer),&a,acquire,release};
    const XL6_CodeOwner adapter={sizeof(adapter),&b,acquire,release};
    ForeignModuleHooks** environment=xrtMalloc(sizeof(*environment));assert(environment);*environment=&hooks;
    const XL6_ClosureCaptureSlot slot={XL6_CLOSURE_CAPTURE_TRIVIAL,0,sizeof(*environment)};
    const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(*environment),1,&slot};
    XL6_Closure* closure=xl6ClosureRuntimeV2Get()->Create(&producer,unused,NULL,environment,&layout,NULL,NULL,1);assert(closure);
    xvalue* callback=xl6ClosureBoxPointActionV1(&adapter,closure,entry,UINT64_C(42));assert(callback);
    int* raw=malloc(sizeof(*raw));assert(raw);*raw=42;
    xvalue* token=xl6ForeignResourceCreateV1(raw,callback);assert(token);
    xvalue* copy=xrtValueDeepClone(token);assert(copy);
    a.closed=b.closed=true;
    xl6ClosureRuntimeV1Get()->Release(closure);xrtValueRelease(callback);xrtValueRelease(token);
    assert(a.pins==1 && b.pins==1 && !drops && xl6ForeignResourcePointerV1(copy)==raw);
    xrtValueRelease(copy);
    assert(drops==1 && hooks.stages==2 && !a.pins && !b.pins && !a.library && !b.library && !xrtGetError());
    puts("foreign resource producer/adapter unloaded after release: PASS");return 0;
}
