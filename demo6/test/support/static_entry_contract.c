#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned rejected;
static void reject(XL6_IrModule* module,const XL6_IrCgenEntryContractV1* entry)
{
    XL6_IrCgenOptions options={0};options.emitScriptEntry=1;options.emitMainWrapper=1;options.entryContract=entry;
    XL6_DiagList diagnostics={0};XL6_Output output;size_t length=0;
    assert(xl6OutputOpenMemory(&output));assert(!xl6IrCgenEmitCEx(&output,module,&diagnostics,&options));
    char* text=xl6OutputTakeMemoryText(&output,&length);assert(!length && diagnostics.count);
    xrtFree(text);xl6DiagFree(&diagnostics);++rejected;
}
int main(int argc,char** argv)
{
    assert(argc==3);bool managed=!strcmp(argv[2],"graph");assert(managed || !strcmp(argv[2],"direct"));
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_ArtifactRuntimePlan plan={0};XL6_ArtifactOwnershipPlan ownership={0};
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
    assert(plan.entryOwnershipAbi==(managed ? XL6_ARTIFACT_ENTRY_OWNERSHIP_GRAPH : XL6_ARTIFACT_ENTRY_OWNERSHIP_DIRECT));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_STATIC_MODULE)==managed);
    XL6_IrCgenEntryContractV1 entry={sizeof(entry),managed,NULL,0,NULL};
    if(managed){
        assert(xl6ArtifactOwnershipPlanBuild(&plan,&ownership));
        entry.domains=ownership.domains;entry.domainCount=ownership.domainCount;assert(entry.domainCount);
        if(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_CODE_FINALIZER))
            entry.discoverSymbol="xl6ClassOwnershipDiscoverV1";
    }
    XL6_IrCgenOptions options={0};options.emitScriptEntry=1;options.emitMainWrapper=1;options.entryContract=&entry;
    XL6_Output output;size_t length;assert(xl6OutputOpenMemory(&output));
    assert(xl6IrCgenEmitCEx(&output,&unit.ir,&unit.parse.diags,&options));
    char* text=xl6OutputTakeMemoryText(&output,&length);assert(length && strstr(text,"int main("));
    assert((strstr(text,"xl6StaticModuleDrainV1(xlir_entry_module)")!=NULL)==managed);xrtFree(text);
    reject(&unit.ir,NULL);
    XL6_IrCgenEntryContractV1 invalid=entry;invalid.size=0;reject(&unit.ir,&invalid);
    const char* bad[]={"not a C identifier"};invalid=entry;invalid.domains=bad;invalid.domainCount=1;
    invalid.managed=true;reject(&unit.ir,&invalid);
    const char* duplicate[]={"xl6OwnershipDomainClassV1","xl6OwnershipDomainClassV1"};
    invalid=entry;invalid.managed=true;invalid.domains=duplicate;invalid.domainCount=2;reject(&unit.ir,&invalid);
    invalid=entry;invalid.managed=true;invalid.domains=NULL;invalid.domainCount=1;reject(&unit.ir,&invalid);
    invalid=entry;invalid.managed=true;invalid.discoverSymbol="not a C identifier";reject(&unit.ir,&invalid);
    invalid=entry;invalid.managed=false;invalid.domains=duplicate;invalid.domainCount=1;reject(&unit.ir,&invalid);
    if(managed){invalid=(XL6_IrCgenEntryContractV1){sizeof(invalid),false,NULL,0,NULL};reject(&unit.ir,&invalid);}
    printf("static entry %s domains=%zu rejected=%u before output\n",argv[2],entry.domainCount,rejected);
    xl6CompileUnitFree(&unit);return 0;
}
