#include "xl6_value_copy.h"
#include "xl6_list_graph.h"
#include "xl6_set_graph.h"
#include "xl6_dict_graph.h"

xvalue* xl6ValueCloneCollectionsV1(const xvalue* source)
{
    const XL6_ValueGraphProviderV1* providers[]={xl6ListGraphProviderV1(),xl6DictGraphProviderV1(),xl6SetGraphProviderV1()};
    const XL6_ValueGraphPlanV1 plan={sizeof(plan),providers,3,NULL,0};
    return xl6ValueDeepCloneNativeV1(source,&plan);
}
