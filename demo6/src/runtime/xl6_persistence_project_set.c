#include "xl6_persistence_project.h"
#include "xl6_set_graph.h"
xvalue* xl6PersistenceProjectSetV1(const xvalue* source,const XL6_PersistencePlanV1* plan)
{
    const XL6_ValueGraphProviderV1* providers[]={xl6SetGraphProviderV1()};
    const XL6_ValueGraphPlanV1 graph={sizeof(graph),providers,1,NULL,0};
    return xl6PersistenceProjectNativeV1(source,plan,&graph);
}
