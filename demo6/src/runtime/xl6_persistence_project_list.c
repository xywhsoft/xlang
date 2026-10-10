#include "xl6_persistence_project.h"
#include "xl6_list_graph.h"

static int compact(const xvaluehandleops* ops,ptr raw,ptr user,uint64_t identity,
    xvaluegraphnextv1* next,xvalue** output,bool data,bool sequence)
{
    (void)sequence;
    if (!data) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    /* Private List ops authenticate the family before payload access. All
     * language Lists share the compact persistence rule; no spelling or
     * nominal-identity dispatch is used to recognize physical storage. */
    return xl6ListGraphProviderV1()->Copy(ops,raw,user,identity,next,output,true,true);
}
xvalue* xl6PersistenceProjectListV1(const xvalue* source,const XL6_PersistencePlanV1* plan)
{
    static const XL6_ValueGraphProviderV1 provider={compact};
    const XL6_ValueGraphProviderV1* providers[]={&provider};
    const XL6_ValueGraphPlanV1 graph={sizeof(graph),providers,1,NULL,0};
    return xl6PersistenceProjectNativeV1(source,plan,&graph);
}
