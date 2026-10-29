#include "src/runtime/xl6_value_copy.h"
#include "src/runtime/xl6_list_graph.h"

xvalue* xl6ValueCloneGraphV1(const xvalue* source)
{
    /* Stack-borrowed immutable plan, one XRT graph memo for every family and
     * every descendant. In particular repeated List edges are not copied by
     * independent nested graph passes. No persistence sequence rule here. */
    const XL6_ValueGraphProviderV1* providers[]={xl6ListGraphProviderV1()};
    const XL6_ValueGraphPlanV1 plan={sizeof(plan),providers,1,NULL,0};
    return xl6ValueDeepCloneNativeV1(source,&plan);
}
