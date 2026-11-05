#include "src/compile/xl6_artifact_runtime.h"
#include "src/compile/xl6_compile_unit.h"
#include "src/runtime/xl6_native_com.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef int (*SymbolVisit)(void*,const char*,const void*);
extern int xlApiVisitDefaultRuntimeSymbols(SymbolVisit,void*);
static const char* const symbols[]={"xl6TransferServicesFromLeaseV1","xl6TransferServicesProcessResidentV1"};
static const struct {const char* name;unsigned owner;} declarations[]={
#define XL6_ARTIFACT_SYMBOL(Name,Owner) {Name,XL6_ARTIFACT_##Owner},
#include "src/compile/xl6_artifact_runtime_symbols.inc"
#undef XL6_ARTIFACT_SYMBOL
};
typedef struct Seen {unsigned counts[2];} Seen;
static int visit(void* raw,const char* name,const void* address)
{
    Seen* seen=raw;
    for(size_t i=0;i<2;++i)if(!strcmp(name,symbols[i])){
        const void* expected=i ? (const void*)xl6TransferServicesProcessResidentV1:(const void*)xl6TransferServicesFromLeaseV1;
        assert(address && address==expected);++seen->counts[i];
    }
    return 1;
}
static bool hasSource(const XL6_ArtifactRuntimePlan* plan,const char* path)
{
    unsigned count=0;for(size_t i=0;i<plan->sourceCount;++i)count+=!strcmp(plan->sources[i],path);
    assert(count<=1);return count!=0;
}
int main(int argc,char** argv)
{
    _Static_assert(XL6_ARTIFACT_NATIVE_COM==1374,"append-only component ID");
    _Static_assert(XL6_ARTIFACT_RUNTIME_COUNT==1375,"complete component count");
    _Static_assert(XL6_ARTIFACT_RUNTIME_WORD_COUNT==22,"bounded component bitset");
    assert(argc==2);
    Seen seen={{0}};assert(xlApiVisitDefaultRuntimeSymbols(visit,&seen));
    for(size_t i=0;i<2;++i){
        unsigned count=0;for(size_t j=0;j<sizeof(declarations)/sizeof(*declarations);++j)
            if(!strcmp(declarations[j].name,symbols[i])){assert(declarations[j].owner==XL6_ARTIFACT_NATIVE_COM);++count;}
        assert(count==1 && seen.counts[i]==1 && xl6ArtifactRuntimeSymbolOwner(symbols[i])==XL6_ARTIFACT_NATIVE_COM);
    }
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_NATIVE_COM);
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
    const unsigned dependencies[]={XL6_ARTIFACT_FOREIGN_VIEW,XL6_ARTIFACT_CODE_LEASE,XL6_ARTIFACT_FOREIGN_ERROR,XL6_ARTIFACT_CALLBACK_FAILURE};
    for(size_t i=0;i<sizeof(dependencies)/sizeof(*dependencies);++i)assert(xl6ArtifactRuntimeSetHas(&plan.components,dependencies[i]));
    assert(hasSource(&plan,"src/runtime/xl6_native_com.c"));
    for(unsigned i=0;i<XL6_ARTIFACT_RUNTIME_COUNT;++i)if(xl6ArtifactRuntimeSetHas(&plan.components,i)){
        const XL6_ArtifactRuntimeComponent* component=xl6ArtifactRuntimeComponentGet(i);assert(component);
        printf("component\t%u\t%s\n",i,component->source);
        for(size_t j=0;j<component->dependencyCount;++j)assert(xl6ArtifactRuntimeSetHas(&plan.components,component->dependencies[j]));
    }
    const XL6_ArtifactRuntimeComponent* capsule=xl6ArtifactRuntimeComponentGet(XL6_ARTIFACT_NATIVE_COM);
    assert(capsule->dependencyCount==4 && capsule->placement==XL6_ARTIFACT_RUNTIME_RESIDENT_SOURCE);
    const char* const modules[]={"XRT_MODULE_MEMORY","XRT_MODULE_ATOMIC","XRT_MODULE_THREAD"};
    for(size_t i=0;i<3;++i){bool found=false;for(size_t j=0;j<plan.nativeModuleCount;++j)found|=!strcmp(modules[i],plan.nativeModules[j]);assert(found);}
    const char* const mixed[]={symbols[0],"xl6TransferServicesNotARealSymbol"};
    XL6_ArtifactRuntimePlan before=plan;assert(!xl6ArtifactRuntimeRequireSymbols(&plan,mixed,2)&&!memcmp(&before,&plan,sizeof(plan)));
    assert(xl6ArtifactRuntimeSymbolOwner(mixed[1])==XL6_ARTIFACT_RUNTIME_COUNT);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    if(!xl6CompileUnitLoadFile(&unit,argv[1],1) || !xl6IrLowerModule(&unit.ir,&unit.parse.diags) ||
       !xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags)){
        xl6DiagPrintText(&unit.parse.diags,argv[1]);return 2;
    }
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NATIVE_COM)&&!hasSource(&plan,"src/runtime/xl6_native_com.c"));
    assert(xl6ArtifactRuntimeRequireSymbols(&plan,symbols,2));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NATIVE_COM)&&hasSource(&plan,"src/runtime/xl6_native_com.c"));
    xl6CompileUnitFree(&unit);
    xn_transfer_services_v1 services={0};assert(xl6TransferServicesProcessResidentV1(NULL,&services)&&services.wrapDataObject&&services.wrapDropSource&&services.wrapDropTarget);
    puts("COM component wiring: unique compiled and API owners, real plan closure, opt-in source, atomic unknown refusal passed");return 0;
}
