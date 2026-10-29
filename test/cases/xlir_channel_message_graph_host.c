#include "include/xlc.h"
#include "include/xruntime_decl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool fail, rooted;
static unsigned drops, perKind[3];
static xvalue* alias;
static bool liveAllocation(const xmemdebugallocation* allocation,void* unused)
{
    (void)unused;
    fprintf(stderr,"live %zu bytes at %s:%u\n",allocation->Size,
        allocation->File?allocation->File:"unknown",allocation->Line);
    return true;
}
static bool message_graph_fail(void) { return fail; }
static void message_graph_drop(int64_t kind)
{
    assert(kind>=0 && kind<3 && !perKind[kind]);
    ++perKind[kind];++drops;
}
static void message_graph_capture(xvalue* value)
{
    assert(value && !alias);
    if (rooted) alias=xrtValueRetain(value);
}
int main(int argc,char** argv)
{
    assert(argc==7); // source, work root, native library, TCC root, private headers, temp root
    assert(xrtMemDebugEnable(true));
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=argv[2];options.tccDllPath=argv[3];options.tccLibPath=argv[4];options.tempDir=argv[6];
    /* Warm the public compiler/native loader without messages. Its retained
     * process-wide DLL path is not part of any individual message lifetime. */
    XLC_Context* warm=xlcCreateContext(&options);assert(warm);
    assert(xlcAddIncludePath(warm,argv[5]));
    assert(xlcAddSourceString(warm,"__message_loader_warm.xl","assert 6 * 7 == 42\n"));
    assert(xlcCompile(warm) && xlcRun(warm,0,NULL));xlcDestroyContext(warm);
    XL_RuntimeDrainResult warmed=xlRuntimeDrainRetiringChecked(-1);assert(warmed.ready && warmed.ok);
    for (unsigned mode=0;mode<4;++mode) {
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
        char temporary[4096];
        int length=snprintf(temporary,sizeof(temporary),"%s/case-%u",argv[6],mode);
        assert(length>0 && (size_t)length<sizeof(temporary));
        options.tempDir=temporary;
        XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
        assert(xlcAddIncludePath(compiler,argv[5]));
        fail=(mode&1)!=0;rooted=(mode&2)!=0;drops=0;alias=NULL;memset(perKind,0,sizeof(perKind));
        assert(xlcRegisterCFunction(compiler,"message_graph_fail",(void*)message_graph_fail));
        assert(xlcRegisterCFunction(compiler,"message_graph_drop",(void*)message_graph_drop));
        assert(xlcRegisterCFunction(compiler,"message_graph_capture",(void*)message_graph_capture));
        assert(xlcAddSourceFile(compiler,argv[1]) && xlcCompile(compiler));
        XL_Module* module=xlcRelocateModule(compiler);assert(module);
        assert(xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL));
        assert(!drops && (!rooted || alias));
        xlDestroyModule(module);xlcDestroyContext(compiler);
        if (rooted) {
            XL_RuntimeDrainResult waiting=xlRuntimeDrainRetiringChecked(0);
            assert(!waiting.ready && !drops);
            xrtValueRelease(alias);alias=NULL;
        }
        bool checkedFailure=false;
        XL_RuntimeDrainResult result={0};
        for (unsigned retry=0;retry<8 && !result.ready;++retry) {
            result=xlRuntimeDrainRetiringChecked(-1);
            checkedFailure|=!result.ok;
            assert(drops<=3);
        }
        if (!result.ready || checkedFailure!=fail || drops!=3)
            fprintf(stderr,"message graph mode=%u ready=%d checked-failure=%d drops=%u\n",mode,result.ready,checkedFailure,drops);
        assert(result.ready && checkedFailure==fail && drops==3);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        if (before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes)
        {
            fprintf(stderr,"graph ledger mode=%u before=%zu/%zu after=%zu/%zu\n",mode,
                before.LiveCount,before.LiveBytes,after.LiveCount,after.LiveBytes);
            xrtMemDebugVisitLive(liveAllocation,NULL);
        }
        assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    }
    puts("actual Channel graph host: 4 checked retirements, 12 finalizers, external roots preserved, errors reported");
    return 0;
}
