#include "xl6_value_copy.h"
#include "xl6_set_graph.h"
xvalue* xl6ValueCloneSetGraphV1(const xvalue* source)
{
    const XL6_ValueGraphProviderV1* providers[]={xl6SetGraphProviderV1()};
    const XL6_ValueGraphPlanV1 plan={sizeof(plan),providers,1,NULL,0};
    return xl6ValueDeepCloneNativeV1(source,&plan);
}
