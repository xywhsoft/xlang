#include "src/runtime/xl6_set_schema.h"
#include "src/runtime/xl6_resident_reference_schema.h"
#include "src/api/xl_foreign_error.h"

typedef struct SetSchema {
    XL6_SetLayoutV1 layout;
    XL6_SequenceAdmissionV3 policy;
} SetSchema;
static void drop(const void* opaque)
{
    SetSchema* schema=(SetSchema*)opaque;
    xl6SequenceAdmissionDropV3(&schema->policy); xrtFree(schema);
}
static bool admits(const void* opaque,const xvalue* value)
{
    const SetSchema* schema=opaque;
    return xl6SetAdmitsSchemaV1(value,&schema->layout,&schema->policy);
}
static bool equivalent(const void* left,const void* right)
{
    const SetSchema* a=left; const SetSchema* b=right;
    const XL6_SequenceLayoutV1* x=&a->layout.value; const XL6_SequenceLayoutV1* y=&b->layout.value;
    const XL6_SequenceAdmissionV3* p=&a->policy; const XL6_SequenceAdmissionV3* q=&b->policy;
    return a->layout.identity==b->layout.identity && x->identity==y->identity &&
        x->elementIdentity==y->elementIdentity && x->transfer==y->transfer && x->size==y->size &&
        x->type==y->type && p->ops==q->ops && p->elementIdentity==q->elementIdentity &&
        p->transfer==q->transfer && p->size==q->size && p->type==q->type &&
        p->boxedIdentity==q->boxedIdentity &&
        (p->data==q->data || p->ops->Equivalent(p->data,q->data));
}
static const XL6_ResidentReferenceSchemaOpsV1 schemaOps={drop,admits,equivalent};

/* Consumed inputs must retire even when installing an error scope fails.
 * Preserve the first outcome, including refusal without a diagnostic. */
static void discard(xvalue* value)
{
    if (!value) return;
    xerror* incoming=xrtErrorRef(xrtGetError()); xl6_error_scope cleanup={0};
    if (xl6ErrorScopeBeginCleanup(&cleanup,true)) {
        xrtErrorFree(incoming); xrtValueRelease(value);
        xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
        if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    } else {
        xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(incoming)});
        xrtClearError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(incoming);
    }
}
xvalue* xl6SequenceSetCreateTakeV1(unsigned int role,uint64_t identity,uint64_t element,
    xvalue* child,bool nullable)
{
    xl6_error_scope scope={0};
    if (!(child ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) {
        discard(child); return NULL;
    }
    XL6_SetLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0}; xvalue* result=NULL;
    if (!identity || !element || (role!=XL6_SEQUENCE_ARRAY_V1 && role!=XL6_SEQUENCE_STACK_V1))
        xrtSetErrorKind(XERR_ARGUMENT);
    else if (xl6SetSchemaCopyV1(child,&layout,&policy)) {
        if (nullable ? element==layout.identity : element!=layout.identity) xrtSetErrorKind(XERR_ARGUMENT);
        else {
            unsigned int depth=xl6ResidentAdmissionDepthV1(&policy)+1;
            if (depth>64) xrtSetErrorKind(XERR_RANGE);
            else {
                SetSchema* data=xrtMalloc(sizeof(*data));
                if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); }
                else {
                    *data=(SetSchema){layout,policy}; policy=(XL6_SequenceAdmissionV3){0};
                    XL6_ResidentReferenceSchemaV1 schema={&schemaOps,data,layout.identity,depth};
                    result=xl6SequenceResidentReferenceCreateTakeV1(role,identity,element,&schema,nullable);
                }
            }
        }
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&policy); xrtValueRelease(child);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); discard(result); return NULL; }
    return result;
}
