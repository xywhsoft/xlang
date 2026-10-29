#include "xlir_foreign_resource_module.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/runtime/xl6_native_handle.h"
#include "include/xl_clock.h"
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
static int expected=42;
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
    assert(((ForeignModulePayload*)raw)->value==expected);free(raw);++drops;
}
static void unused(void){abort();}
static bool capture(xrtownershipref ref,void* data){*(xrtownershipref*)data=ref;return true;}
static bool visit(xrtownershipref ref,void* data){(void)ref;(void)data;return true;}
int main(int argc,char** argv)
{
    assert(argc==3||(argc==4&&(!strcmp(argv[3],"graph")||!strcmp(argv[3],"task"))));
    bool graph=argc==4,task=graph&&!strcmp(argv[3],"task");
    ForeignModuleHooks hooks={.dispose=dispose};
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
    ForeignModulePayload* raw=malloc(sizeof(*raw));assert(raw);raw->value=42;raw->completed=NULL;
    if(task){
        atomic_int completed;atomic_init(&completed,0);raw->completed=&completed;expected=43;
        XL6_ForeignGraphDescriptorV1 descriptor={sizeof(descriptor),XL6_FOREIGN_GRAPH_V1_TAG,
            (xrtownershiptrace)symbol(a.library,"foreign_module_graph_trace"),NULL,
            (xnettaskproc)symbol(a.library,"foreign_module_graph_task")};
        assert(descriptor.Trace&&descriptor.Task);
        void* record=xl6ForeignGraphCreateV1(raw,&descriptor,callback);assert(record);
        a.closed=b.closed=true;
        xl6ClosureRuntimeV1Get()->Release(closure);xrtValueRelease(callback);
        xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;
        config.Backend=XNET_PORT_SELECT;
        xnetengine* engine=xrtNetEngineCreate(&config);assert(engine&&xrtNetEngineStart(engine));
        assert(xrtNetEnginePostOwnedV1(engine,0,record,xl6ForeignGraphTaskPolicyV1Get()));
        XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
        while(!atomic_load_explicit(&completed,memory_order_acquire)){
            assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();
        }
        /* Stop joins the actual worker and resident task Drop before checking
         * the cross-DLL owner counters; no unsynchronized polling of them. */
        assert(xrtNetEngineStop(engine)&&xrtNetEngineDestroy(engine));
        assert(drops==1&&hooks.stages==2&&!a.pins&&!b.pins&&!a.library&&!b.library&&!xrtGetError());
        puts("foreign owned task: real worker, producer/adapter unload after callback tail: PASS");return 0;
    }
    xvalue* token=NULL;
    if(graph){
        XL6_ForeignGraphDescriptorV1 descriptor={sizeof(descriptor),XL6_FOREIGN_GRAPH_V1_TAG,
            (xrtownershiptrace)symbol(a.library,"foreign_module_graph_trace"),NULL,NULL};
        assert(descriptor.Trace);
        void* record=xl6ForeignGraphCreateV1(raw,&descriptor,callback);assert(record);
        token=xl6NativeHandleTakeV1(&record,xl6ForeignGraphHandlePolicyV1Get());assert(token&&!record);
    }else token=xl6ForeignResourceCreateV1(raw,callback);
    assert(token);
    xvalue* copy=graph?xrtValueRetain(token):xrtValueDeepClone(token);assert(copy);
    a.closed=b.closed=true;
    xl6ClosureRuntimeV1Get()->Release(closure);xrtValueRelease(callback);xrtValueRelease(token);
    assert(a.pins==1 && b.pins==1 && !drops);
    if(graph){
        xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
        xrtownershipref record={0};assert(xl6ForeignGraphHandlePolicyV1Get()->Trace(copy,capture,&record));
        assert(record.Ops&&record.Ops->Trace(record.Data,visit,NULL));
        assert(xrtOwnershipScopeEnd(&freeze));
    }else assert(xl6ForeignResourcePointerV1(copy)==raw);
    xrtValueRelease(copy);
    assert(drops==1 && hooks.stages==2 && !a.pins && !b.pins && !a.library && !b.library && !xrtGetError());
    puts("foreign resource producer/adapter unloaded after release: PASS");return 0;
}
