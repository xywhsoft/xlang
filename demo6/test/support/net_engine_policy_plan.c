#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool has_module(const XL6_ArtifactRuntimePlan* p,const char* name)
{for(size_t i=0;i<p->nativeModuleCount;++i)if(!strcmp(p->nativeModules[i],name))return true;return false;}
static XL6_ArtifactOwnershipPlan plan(const char* choice)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(!strcmp(choice,"config")?XL6_ARTIFACT_NET_ENGINE_CONFIG:
        !strcmp(choice,"stats")?XL6_ARTIFACT_NET_ENGINE_STATS:XL6_ARTIFACT_NET_ENGINE_LANGUAGE);
    if(!strcmp(choice,"all")){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_ENGINE_CONFIG));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_ENGINE_STATS));
    }
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){
        XL6_ArtifactOwnershipPlan p=plan(argv[2]);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<p.runtime.sourceCount;++i)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;++i)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<p.domainCount;++i)printf("%s(),\n",p.domains[i]);return 0;}return 2;
    }
    assert(argc==1);
    {
        const char* names[]={"xl6OwnershipCatalogAddNetTaskV1","xl6OwnershipCatalogAddNetTimerV1","xl6OwnershipCatalogAddNetWorkerExitV1"};
        for(size_t i=0;i<sizeof(names)/sizeof(*names);++i)
            assert(xl6ArtifactRuntimeSymbolOwner(names[i])==XL6_ARTIFACT_OWNERSHIP_CATALOG);
        XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan closed;
        XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_OWNERSHIP_CATALOG);
        assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&closed));
        assert(!has_module(&closed.runtime,"XRT_MODULE_NET_ENGINE")&&!has_module(&closed.runtime,"XRT_MODULE_TASK_NET"));
        assert(!xl6ArtifactRuntimeSetHas(&closed.runtime.components,XL6_ARTIFACT_EXECUTION_NET_ENGINE));
        const char* kinds[]={"all","engine","config","stats"};
        for(size_t i=0;i<sizeof(kinds)/sizeof(*kinds);++i){XL6_ArtifactOwnershipPlan p=plan(kinds[i]);
            assert(!has_module(&p.runtime,"XRT_MODULE_TASK_NET")&&!has_module(&p.runtime,"XRT_MODULE_TASK_POOL")&&
                !has_module(&p.runtime,"XRT_MODULE_NET_RESOLVER")&&!has_module(&p.runtime,"XRT_MODULE_NET_TCP"));
            assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_NETWORK_SERVICE));
            if(i<2)assert(xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_ENGINE_NATIVE));
            else assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
            if(i==3)assert(!has_module(&p.runtime,"XRT_MODULE_NET_ENGINE"));
        }
        puts("Engine policy plan: exact symbol owners, catalog-only does not import network execution");
    }
    puts("Engine plan: typed source/native/work domains, independent config/stats, no task/network over-selection");return 0;
}
