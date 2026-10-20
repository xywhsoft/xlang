#define main resolver_catalog_old_main
#include "executor_catalog_plan.c"
#undef main
static const char* names[]={"list","config","stats","resolver","operation","all"};
static const unsigned ids[]={XL6_ARTIFACT_NET_ADDRESS_LIST,XL6_ARTIFACT_NET_RESOLVER_CONFIG,
    XL6_ARTIFACT_NET_RESOLVER_STATS,XL6_ARTIFACT_NET_RESOLVER_LANGUAGE,XL6_ARTIFACT_NET_RESOLVE_OPERATION};
static XL6_ArtifactOwnershipPlan build(unsigned which,bool setup)
{
    XL6_ArtifactRuntimeSet roots={0};
    if(which==5){for(unsigned i=0;i<5;++i)assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,ids[i]));}
    else assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,ids[which]));
    if(setup&&which==3){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_RESOLVER_CONFIG));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_RESOLVER_STATS));
    }
    XL6_ArtifactRuntimePlan plan;XL6_ArtifactOwnershipPlan ownership;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&plan)&&xl6ArtifactOwnershipPlanBuild(&plan,&ownership));return ownership;
}
int main(int argc,char** argv)
{
    if(argc==3){unsigned selected=6;for(unsigned i=0;i<6;++i)if(!strcmp(argv[2],names[i]))selected=i;assert(selected<6);
        bool api=!strcmp(argv[1],"--api-sources");XL6_ArtifactOwnershipPlan plan=build(selected,!api);
        if(api||!strcmp(argv[1],"--sources")){for(size_t i=0;i<plan.runtime.sourceCount;++i)puts(plan.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<plan.runtime.nativeModuleCount;++i)printf("#define %s\n",plan.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<plan.domainCount;++i)printf("%s(),\n",plan.domains[i]);return 0;}return 2;
    }
    assert(argc==1);
    static const char* globalDomains[]={
#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer,Component,Getter) #Getter,
#include "src/runtime/xl6_ownership_domains.inc"
#undef XL6_OWNERSHIP_DOMAIN_ENTRY
    };
    for(size_t i=0;i<sizeof(globalDomains)/sizeof(globalDomains[0]);++i)
        for(size_t j=0;j<i;++j)assert(strcmp(globalDomains[i],globalDomains[j]));
    for(unsigned i=0;i<6;++i){XL6_ArtifactOwnershipPlan plan=build(i,false);
        assert(!has_module(&plan.runtime,"XRT_MODULE_NET_ENGINE")&&!has_module(&plan.runtime,"XRT_MODULE_TLS"));
        assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_TASK)&&
            !xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_FUTURE_CALLBACK_QUEUE)&&
            !xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_EXECUTOR));
        assert(xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE)==(i==3||i==5));
        if(i==0)assert(!has_module(&plan.runtime,"XRT_MODULE_NET_RESOLVER")&&!has_module(&plan.runtime,"XRT_MODULE_FUTURE"));
        if(i==2)assert(!has_module(&plan.runtime,"XRT_MODULE_NET_DNS")&&!has_module(&plan.runtime,"XRT_MODULE_NET_RESOLVER"));
    }
    puts("Resolver plan: six independent closures, unique global domains, no Engine/TLS or per-wait executor provider");return 0;
}
