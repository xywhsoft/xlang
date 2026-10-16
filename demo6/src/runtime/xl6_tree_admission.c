#include "src/runtime/xl6_tree.h"
#include "src/runtime/xl6_resident_reference_schema.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

typedef struct TreeSchema {
    XL6_TreeLayoutV2 layout;
    XL6_SequenceAdmissionV3 key,value;
} TreeSchema;
static void drop(const void* opaque)
{
    TreeSchema* schema=(TreeSchema*)opaque;
    xl6SequenceAdmissionDropV3(&schema->value);
    xl6SequenceAdmissionDropV3(&schema->key); xrtFree(schema);
}
static bool admits(const void* opaque,const xvalue* value)
{
    const TreeSchema* schema=opaque;
    return xl6TreeAdmitsSchemaV1(value,&schema->layout,&schema->key,&schema->value);
}
static bool sameLayout(const XL6_SequenceLayoutV1* a,const XL6_SequenceLayoutV1* b)
{
    return a->identity==b->identity && a->elementIdentity==b->elementIdentity &&
        a->transfer==b->transfer && a->size==b->size && a->type==b->type;
}
static bool samePolicy(const XL6_SequenceAdmissionV3* a,const XL6_SequenceAdmissionV3* b)
{
    return a->ops==b->ops && a->elementIdentity==b->elementIdentity && a->transfer==b->transfer &&
        a->size==b->size && a->type==b->type && a->boxedIdentity==b->boxedIdentity &&
        (a->data==b->data || a->ops->Equivalent(a->data,b->data));
}
static bool equivalent(const void* left,const void* right)
{
    const TreeSchema* a=left; const TreeSchema* b=right;
    return a->layout.identity==b->layout.identity && a->layout.keyOrder==b->layout.keyOrder &&
        a->layout.keyCodec==b->layout.keyCodec &&
        sameLayout(&a->layout.key,&b->layout.key) && sameLayout(&a->layout.value,&b->layout.value) &&
        samePolicy(&a->key,&b->key) && samePolicy(&a->value,&b->value);
}
static const XL6_ResidentReferenceSchemaOpsV1 schemaOps={drop,admits,equivalent};
xvalue* xl6SequenceTreeCreateTakeV1(unsigned int role,uint64_t identity,uint64_t element,
    const XL6_TreeLayoutV2* layout,xvalue* keys,xvalue* values,bool nullable)
{
    xl6_error_scope scope={0};
    if (!(keys && values ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) abort();
    XL6_SequenceAdmissionV3 key={0},value={0}; xvalue* result=NULL;
    if (xl6TreeDomainsCopyV1(layout,keys,values,&key,&value)) {
        unsigned int keyDepth=xl6ResidentAdmissionDepthV1(&key),valueDepth=xl6ResidentAdmissionDepthV1(&value);
        unsigned int depth=(keyDepth>valueDepth ? keyDepth : valueDepth)+1;
        if (depth>64) xrtSetErrorKind(XERR_RANGE);
        else {
            TreeSchema* data=xrtMalloc(sizeof(*data));
            if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); }
            else {
                *data=(TreeSchema){*layout,key,value};
                key=(XL6_SequenceAdmissionV3){0}; value=(XL6_SequenceAdmissionV3){0};
                XL6_ResidentReferenceSchemaV1 schema={&schemaOps,data,layout->identity,depth};
                result=xl6SequenceResidentReferenceCreateTakeV1(role,identity,element,&schema,nullable);
            }
        }
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&value); xl6SequenceAdmissionDropV3(&key);
    xrtValueRelease(values); xrtValueRelease(keys);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); xrtValueRelease(result); return NULL; }
    return result;
}
