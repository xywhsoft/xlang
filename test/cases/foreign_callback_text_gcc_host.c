/* GCC native publishers call TCC-generated language callbacks. The module
 * case deliberately outlives its compiler context. No desktop or network. */
#include "include/xlc.h"
#include "foreign_callback_span.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    if(argc!=9)return 2;
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=argv[2];options.tempDir=argv[7];options.tccDllPath=argv[5];options.tccLibPath=argv[6];
    XLC_Context* compiler=xlcCreateContext(&options);XL_Context* runner=NULL;XL_Module* module=NULL;int ok=0;
    if(!compiler)return 3;
    if(!xlcAddIncludePath(compiler,argv[2]) || !xlcAddIncludePath(compiler,argv[3]) || !xlcAddIncludePath(compiler,argv[4]))goto done;
    char include[4096];int n=snprintf(include,sizeof(include),"%s/include",argv[4]);
    if(n<0 || (size_t)n>=sizeof(include) || !xlcAddIncludePath(compiler,include))goto done;
#define BIND(Name) if(!xlcRegisterCFunction(compiler,#Name,(void*)Name))goto done
    BIND(span_policy_get);
    BIND(span_create);BIND(span_destroy);BIND(span_set_text);BIND(span_set_optional_text);
    BIND(span_fire_text);BIND(span_fail_end_once);BIND(span_text_fail_after);BIND(span_text_reset);
    BIND(span_text_heap_begin);BIND(span_text_heap_end);BIND(span_text_counts_begin);BIND(span_text_counts_end);BIND(span_live);
#undef BIND
    if(!xlcAddSourceFile(compiler,argv[1]) || !xlcCompile(compiler))goto done;
    if(!strcmp(argv[8],"run"))ok=xlcRun(compiler,0,NULL) && !xlcGetLastExitCode(compiler);
    else if(!strcmp(argv[8],"module")){
        module=xlcRelocateModule(compiler);if(!module)goto done;
        xlcDestroyContext(compiler);compiler=NULL;
        runner=xlCreateContext(&options);if(!runner)goto done;
        ok=xlRunModuleMain(runner,module,NULL) && !xlGetLastExitCode(runner);
    }
done:
    if(!ok)fprintf(stderr,"GCC native/TCC callback failed: %s\n",compiler ? xlcGetLastError(compiler) : runner ? xlGetLastError(runner) : "context creation failed");
    if(module)xlDestroyModule(module);
    if(runner)xlDestroyContext(runner);
    if(compiler)xlcDestroyContext(compiler);
    XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(-1);
    if(!drain.ready || !drain.ok || span_live()!=0)ok=0;
    if(ok)printf("GCC native / TCC owned-text callback %s; all native roots released\n",argv[8]);
    return ok ? 0:1;
}
