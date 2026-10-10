#include "xl6_persistence_project.h"
#include "xl6_list_graph.h"
#include "xl6_set_graph.h"

static int compactList(const xvaluehandleops* ops,ptr raw,ptr user,uint64_t identity,
    xvaluegraphnextv1* next,xvalue** output,bool data,bool sequence)
{
    (void)sequence;
    if (!data) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    return xl6ListGraphProviderV1()->Copy(ops,raw,user,identity,next,output,true,true);
}
xvalue* xl6PersistenceProjectCollectionsV1(const xvalue* source,const XL6_PersistencePlanV1* plan)
{
    static const XL6_ValueGraphProviderV1 list={compactList};
    const XL6_ValueGraphProviderV1* providers[]={&list,xl6SetGraphProviderV1()};
    const XL6_ValueGraphPlanV1 graph={sizeof(graph),providers,2,NULL,0};
    return xl6PersistenceProjectNativeV1(source,plan,&graph);
}
