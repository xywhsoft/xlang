#include "xl6_template_internal.h"
#include "xl6_dict_graph.h"
#include "xl6_list_graph.h"
#include "xl6_set_graph.h"
static int reject(const xvaluehandleops* ops,ptr raw,ptr user,uint64_t identity,
    xvaluegraphnextv1* next,xvalue** output,bool data,bool sequence)
{
    (void)ops;(void)raw;(void)user;(void)identity;(void)next;(void)output;(void)data;(void)sequence;
    xrtSetErrorKind(XERR_UNSUPPORTED);return -1;
}
/* Language entry points first apply the verified PUBLIC projection plan.
 * This native boundary still rejects any unprojected class backing object:
 * it cannot infer visibility, execute getters or follow private resources. */
static int object(uint64_t identity,xvaluegraphnextv1* next,xvalue** output,ptr user)
{
    (void)identity;(void)output;(void)user;
    const xvalue* marker=xrtValueGraphObjectGetV1(next,XRT_STR_LITERAL("$xlir.class.runtime-type"));
    if (marker || xrtGetError()) {if (!xrtGetError()) xrtSetErrorKind(XERR_UNSUPPORTED);return -1;}
    return 0;
}
xvalue* xl6TemplateDataV1(const xvalue* source)
{
    static const XL6_ValueGraphProviderV1 terminal={reject};
    const XL6_ValueGraphProviderV1* providers[]={xl6DictGraphProviderV1(),xl6ListGraphProviderV1(),xl6SetGraphProviderV1(),&terminal};
    const XL6_ValueGraphPlanV1 plan={sizeof(plan),providers,4,NULL,0};
    return xl6ValueDataProjectNativeV1(source?source:xrtValueNull(),object,NULL,NULL,0,&plan);
}
