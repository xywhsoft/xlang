#include "src/runtime/xl6_list_import_chain.h"
#include "src/runtime/xl6_list_graph_access.h"
#include <stdlib.h>

typedef struct ListTemplate {
    XL6_ListGraphViewV1 view;
    XL6_SequenceAdmissionV3 policy;
} ListTemplate;
static bool begin(const xvalue* source,ptr* output)
{
    ListTemplate* value=xrtMalloc(sizeof(*value));
    if (!value) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return false; }
    *value=(ListTemplate){0}; *output=value;
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    if (!source || xrtValueType(source)!=XVALUE_HANDLE ||
        !xrtValueGetHandle(source,&raw,&ops,&user)) { xrtSetErrorKind(XERR_TYPE); return false; }
    int status=xl6ListGraphBeginV1(ops,raw,user,xrtValueTypeId(source),&value->view);
    if (!status) xrtSetErrorKind(XERR_TYPE);
    if (status!=1) return false;
    XL6_ListLayoutV1 layout={0};
    if (!xl6ListSchemaCopyV1(source,&layout,&value->policy)) return false;
    if (xrtTypedListCount(value->view.storage)) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void end(ptr opaque)
{
    ListTemplate* value=opaque; if (!value) return;
    if (value->view.owner) xl6ListGraphEndV1(&value->view);
    xl6SequenceAdmissionDropV3(&value->policy); xrtFree(value);
}
static xvalue* empty(ptr opaque)
{ return xl6ListGraphEmptyV1(&((ListTemplate*)opaque)->view); }
static bool admits(ptr opaque,const xvalue* value)
{
    const ListTemplate* source=opaque;
    return source->policy.ops->Value(source->policy.data,value);
}
static bool append(xvalue* target,int64_t key,const xvalue* value)
{ return xl6ListErasedSetV1(target,key,value); }
const XL6_CollectionImportProviderV1* xl6ListImportProviderV1(void)
{ static const XL6_CollectionImportProviderV1 provider={begin,end,empty,admits,append}; return &provider; }
