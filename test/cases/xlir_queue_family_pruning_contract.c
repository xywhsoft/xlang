/* Source-derived independent-family artifact contract, not text searching or
 * a hand-written synthetic type pool. This is component selection evidence;
 * standalone object/link-map pruning is independently required. */
#include "src/cgen/xl6_ir_cgen.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char** argv)
{
    assert(argc==4);
    const unsigned expectedAdmission=(unsigned)strtoul(argv[2],NULL,10);
    const unsigned expectedCore=(unsigned)strtoul(argv[3],NULL,10);
    XL6_ParseResult parse; XL6_IrModule ir; xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    int ok=xl6ParseFile(argv[1],&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
        xl6SemCheckModule(parse.root,&parse.diags,"queue_family") &&
        xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) && xl6IrLowerModule(&ir,&parse.diags);
    if (!ok) {
        for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%s\n",parse.diags.items[i].message);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
    }
    assert(xl6IrVerifyModule(&ir,NULL));
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    const size_t core[]={XL6_ARTIFACT_TYPED_QUEUE_SPSC,XL6_ARTIFACT_TYPED_QUEUE_MPSC,XL6_ARTIFACT_TYPED_QUEUE_MPMC};
    const size_t admission[]={XL6_ARTIFACT_QUEUE_SPSC_ADMISSION,XL6_ARTIFACT_QUEUE_MPSC_ADMISSION,XL6_ARTIFACT_QUEUE_MPMC_ADMISSION};
    const char* const source[]={"src/runtime/xl6_queue_spsc.c","src/runtime/xl6_queue_mpsc.c","src/runtime/xl6_queue_mpmc.c"};
    const char* const schema[]={"src/runtime/xl6_queue_spsc_admission.c","src/runtime/xl6_queue_mpsc_admission.c","src/runtime/xl6_queue_mpmc_admission.c"};
    const char* const native[]={"XRUNTIME_MODULE_TYPED_QUEUE_SPSC","XRUNTIME_MODULE_TYPED_QUEUE_MPSC","XRUNTIME_MODULE_TYPED_QUEUE_MPMC"};
    unsigned coreMask=0,admissionMask=0,sourceMask=0,schemaMask=0,nativeMask=0;
    for (unsigned f=0;f<3;++f) {
        if (xl6ArtifactRuntimeSetHas(&plan.components,core[f])) coreMask|=1u<<f;
        if (xl6ArtifactRuntimeSetHas(&plan.components,admission[f])) admissionMask|=1u<<f;
        for (size_t i=0;i<plan.sourceCount;++i) {
            if (!strcmp(plan.sources[i],source[f])) sourceMask|=1u<<f;
            if (!strcmp(plan.sources[i],schema[f])) schemaMask|=1u<<f;
        }
        for (size_t i=0;i<plan.nativeModuleCount;++i) if (!strcmp(plan.nativeModules[i],native[f])) nativeMask|=1u<<f;
    }
    if (coreMask!=expectedCore || sourceMask!=expectedCore || nativeMask!=expectedCore)
        fprintf(stderr,"expected core=%u; actual components=%u source=%u native=%u\n",expectedCore,coreMask,sourceMask,nativeMask);
    assert(coreMask==expectedCore && sourceMask==expectedCore && nativeMask==expectedCore);
    assert(admissionMask==expectedAdmission && schemaMask==expectedAdmission);
    printf("Queue family contract: core/source/native mask=%u, admission/source mask=%u; independent components selected\n",coreMask,admissionMask);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
