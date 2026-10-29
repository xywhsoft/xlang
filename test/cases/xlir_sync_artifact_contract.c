/* Compile the actual verified plan with genuine TCC, retaining a native
 * object symbol table. DLL exports are not a proxy for private symbol absence. */
#include "src/cgen/xl6_ir_cgen.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/compile/xl6_xrt_trim.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include "libtcc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void error(void* context,const char* message)
{ (void)context; fprintf(stderr,"actual TCC: %s\n",message); }
static void include(TCCState* state,const char* root,const char* suffix)
{
    char path[8192]; int length=snprintf(path,sizeof(path),"%s/%s",root,suffix);
    assert(length>0 && (size_t)length<sizeof(path)); assert(tcc_add_include_path(state,path)==0);
}
int main(int argc,char** argv)
{
    assert(argc==6 || (argc==7 && (!strcmp(argv[6],"selected") || !strcmp(argv[6],"full"))));
    XL6_ParseResult parse; XL6_IrModule ir; xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(argv[1],&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
        xl6SemCheckModule(parse.root,&parse.diags,"sync_artifact") &&
        xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) && xl6IrLowerModule(&ir,&parse.diags));
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&parse.diags));
    TCCState* state=tcc_new(); assert(state); tcc_set_error_func(state,NULL,error);
    tcc_set_lib_path(state,argv[3]);
    assert(tcc_set_options(state,"-std=c11 -DXL_STATIC_EMBED")==0);
    assert(tcc_set_output_type(state,TCC_OUTPUT_OBJ)==0);
    include(state,argv[2],""); include(state,argv[2],"src");
    include(state,argv[2],"release"); include(state,argv[2],"release/include");
    include(state,argv[2],"lib/xruntime");
    assert(tcc_add_include_path(state,argv[4])==0);
    if(argc==7) {
        /* Same implementation generator as actual standalone output. The full
         * control uses its real undeclared-C-source fallback, not invented
         * symbols or forced family macros. Keep an inspectable object table. */
        char* implementation=NULL;
        assert(xl6XrtTrimBuildImplementationSourceFromArtifact(NULL,
            !strcmp(argv[6],"full") ? 1u : 0u,
            plan.nativeModules,plan.nativeModuleCount,&implementation));
        for(size_t i=0;i<plan.nativeModuleCount;++i)
            printf("actual selected kernel root: %s\n",plan.nativeModules[i]);
        assert(tcc_compile_string(state,implementation)==0);
        xrtFree(implementation);
    }
    for(size_t i=0;argc==6 && i<plan.sourceCount;++i) {
        char path[8192]; int length=snprintf(path,sizeof(path),"%s/%s",argv[2],plan.sources[i]);
        assert(length>0 && (size_t)length<sizeof(path));
        printf("actual selected provider: %s\n",plan.sources[i]);
        assert(tcc_add_file(state,path)==0);
    }
    assert(tcc_output_file(state,argv[5])==0); tcc_delete(state);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    puts(argc==6 ? "actual verified plan compiled to TCC native object" :
        "actual standalone implementation compiled to TCC kernel object");
    return 0;
}
