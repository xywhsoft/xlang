/* Real public source-JIT module lifetime, including deferred collection.
 * A successful run alone does not prove that cyclic globals were retired. */
#include "xlc.h"
#include "release/include/xruntime_decl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
static bool imageReleased(const void* address)
{
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION info;assert(VirtualQuery(address,&info,sizeof(info))==sizeof(info));
    return info.State==MEM_FREE;
#else
    /* Native TCC uses malloc + mprotect on this Linux channel, not mmap.
     * Free removes executable permission before returning the allocation;
     * a still-mapped heap page is not evidence of a live code image. */
    FILE* maps=fopen("/proc/self/maps","r");assert(maps);char line[1024],permissions[5];
    unsigned long long first,last,target=(unsigned long long)(uintptr_t)address;
    bool released=true;
    while(fgets(line,sizeof(line),maps)){
        if(sscanf(line,"%llx-%llx %4s",&first,&last,permissions)==3 && target>=first && target<last){
            released=permissions[2]!='x';break;
        }
    }
    assert(!ferror(maps) && !fclose(maps));return released;
#endif
}
static void transaction(char** args)
{
    assert(xlRuntimeInit());XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=".";options.tempDir=args[5];options.tccDllPath=args[1];options.tccLibPath=args[2];
    XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
    assert(xlcAddIncludePath(compiler,args[3]) && xlcAddSourceFile(compiler,args[4]));
    if(!xlcCompile(compiler)){fprintf(stderr,"%s\n",xlcGetLastError(compiler));assert(0);}
    XL_Module* module=xlcRelocateModule(compiler);assert(module);
    assert(xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL));
    void* image=xlModuleGetSymbol(module,"xl_script_module_ownership_trace");assert(image && !imageReleased(image));
    xlDestroyModule(module);xlcDestroyContext(compiler);
    /* Each balanced manual acquisition requests another bounded collection
     * turn. No guessed reference subtraction or semantic force-finalization. */
    double deadline=xrtTimer()+5;
    while(!imageReleased(image) && !xrtGetError()){
        assert(xrtTimer()<deadline);assert(xlRuntimeInit());xlRuntimeUnit();xrtThreadYield();
    }
    xlRuntimeUnit();
    if(xrtGetError())fprintf(stderr,"lifetime error kind=%u code=%d message=%s at %s:%d\n",(unsigned)xrtErrorKind(xrtGetError()),
        (int)xrtErrorCode(xrtGetError()),xrtErrorMessage(xrtGetError()),xrtErrorFile(xrtGetError()),(int)xrtErrorLine(xrtGetError()));
    assert(!xrtGetError());
    assert(imageReleased(image));
}
int main(int argc,char** argv)
{
    assert(argc==6 && xrtMemDebugEnable(true));
    transaction(argv);transaction(argv);
    for(unsigned round=0;round<3;++round){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);transaction(argv);xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
            before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
            before.UseAfterFreeCount==after.UseAfterFreeCount);
    }
#ifdef _WIN32
    puts("source Process owner: 5 real module lifetimes, code images MEM_FREE, all owning fields readable, complete XRT release ledger balanced");
#else
    puts("source Process owner: 5 real module lifetimes, code pages non-executable, all owning fields readable, complete XRT release ledger balanced");
#endif
    return 0;
}
