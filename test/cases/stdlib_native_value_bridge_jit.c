/* Genuine source compiler/JIT consumer, never an old compiler DLL. Records
 * enter through public var arguments after their original maps are dead. */
#include "xlc.h"
#include "src/runtime/xl6_slot_map_key_value.h"
#include "src/runtime/xl6_array_admission.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static xvalue* arguments(void)
{
    const xrttype* type=xrtTypeInt64();
    const XL6_SlotMapLayoutV1 layout={0xDA01,{0xDA02,0xDA03,1,sizeof(int64_t),type}};
    xvalue* args=xrtValueArray(); assert(args);
    for (unsigned i=0;i<2;++i) {
        xvalue* schema=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,layout.value.identity,
            layout.value.elementIdentity,1,sizeof(int64_t),type,XL6_VALUE_ADMIT_I64,false,layout.value.elementIdentity);
        assert(schema); xvalue* map=xl6SlotMapCreateTakeV1(&layout,schema); assert(map);
        XL6_SlotMapKeyV1 key={0}; assert(xl6SlotMapInsertV1(map,&layout,&(int64_t){42},&key));
        assert(xrtValueArrayAppendNew(args,xl6SlotMapKeyBoxV1(key)));
        if (!i) assert(xrtValueArrayAppendNew(args,xl6SlotMapKeyBoxV1(key)));
        xrtValueRelease(map); xl6SlotMapKeyDropValueV1(key);
    }
    return args;
}
int main(int argc,char** argv)
{
    assert(argc==7);
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=argv[6]; options.tempDir=argv[5];
    options.tccDllPath=argv[3]; options.tccLibPath=argv[4];
    XLC_Context* compiler=xlcCreateContext(&options); assert(compiler);
    assert(xlcAddIncludePath(compiler,argv[2]) && xlcAddSourceFile(compiler,argv[1]));
    if (!xlcCompile(compiler)) { fprintf(stderr,"%s\n",xlcGetLastError(compiler)); assert(false); }
    XL_Module* module=xlcRelocateModule(compiler); assert(module);
    xvalue* args=arguments();
    for (unsigned i=0;i<100;++i) {
        assert(xlRunModuleMain(xlcRuntimeContext(compiler),module,args));
        assert(xlGetLastExitCode(xlcRuntimeContext(compiler))==0 && !xrtGetError());
    }
    xrtValueRelease(args); xlDestroyModule(module); xlcDestroyContext(compiler);
    assert(!xrtGetError()); puts("[PASS] actual public JIT owning-record foreign calls: 100 genuine-key rounds");
    return 0;
}
