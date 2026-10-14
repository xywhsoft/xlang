/* Instantiated independently per queue family. Plain Queue<T> does not pull
 * this recursive-admission component (or other queue families) into artifacts.
 * Schema construction copies only owned immutable metadata, never a queue,
 * capacity, element storage, source callback or module-owned identity span. */
#include "xl6_concurrent_queue.h"
#include "xl6_resident_reference_schema.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>
#define XL6_QA_JOIN_(A,B) A##B
#define XL6_QA_JOIN(A,B) XL6_QA_JOIN_(A,B)
#define XL6_QA_PUBLIC(Suffix) XL6_QA_JOIN(XL6_QA_PREFIX,Suffix)
#ifndef XL6_QA_ADMITS
#define XL6_QA_ADMITS XL6_QA_PUBLIC(AdmitsSchemaV1)
#endif

typedef struct QueueSchema {
    uint64_t identity;
    XL6_SequenceLayoutV1 layout;
    XL6_SequenceAdmissionV3 policy;
} QueueSchema;
static void drop(const void* opaque)
{
    QueueSchema* schema=(QueueSchema*)opaque;
    xl6SequenceAdmissionDropV3(&schema->policy); xrtFree(schema);
}
static bool admits(const void* opaque,const xvalue* value)
{
    const QueueSchema* schema=opaque;
    return XL6_QA_ADMITS(value,schema->identity,&schema->layout,&schema->policy);
}
static bool equivalent(const void* left,const void* right)
{
    const QueueSchema* a=left; const QueueSchema* b=right;
    const XL6_SequenceAdmissionV3* p=&a->policy; const XL6_SequenceAdmissionV3* q=&b->policy;
    return a->identity==b->identity && a->layout.identity==b->layout.identity &&
        a->layout.elementIdentity==b->layout.elementIdentity && a->layout.transfer==b->layout.transfer &&
        a->layout.size==b->layout.size && a->layout.type==b->layout.type && p->ops==q->ops &&
        p->elementIdentity==q->elementIdentity && p->transfer==q->transfer && p->size==q->size &&
        p->type==q->type && p->boxedIdentity==q->boxedIdentity &&
        (p->data==q->data || p->ops->Equivalent(p->data,q->data));
}
static const XL6_ResidentReferenceSchemaOpsV1 schemaOps={drop,admits,equivalent};
xvalue* XL6_QA_FACTORY(unsigned int role,uint64_t identity,uint64_t element,
    uint64_t queueIdentity,xvalue* seed,bool nullable)
{
    xl6_error_scope scope={0};
    if (!(seed ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) abort();
    XL6_SequenceRoleV1 childRole=0; XL6_SequenceLayoutV1 layout={0};
    XL6_SequenceAdmissionV3 policy={0}; xvalue* result=NULL;
    if (!queueIdentity) xrtSetErrorKind(XERR_ARGUMENT);
    else if (xl6SequenceSchemaCopyV1(seed,&childRole,&layout,&policy)) {
        if (childRole!=XL6_SEQUENCE_ARRAY_V1) xrtSetErrorKind(XERR_TYPE);
        else {
            XL6_Sequence* pin=xl6SequenceBeginRoleV1(seed,childRole,layout.identity,
                layout.elementIdentity,layout.transfer,layout.size,layout.type);
            bool empty=pin && xrtTypedArrayCount(xl6SequenceStorageV1(pin))==0;
            if (pin) xl6SequenceEndV1(pin);
            unsigned int depth=xl6ResidentAdmissionDepthV1(&policy)+1;
            if (!empty) { if (!xrtGetError()) xrtSetErrorKind(XERR_ARGUMENT); }
            else if (depth>64) xrtSetErrorKind(XERR_RANGE);
            else {
                QueueSchema* data=xrtMalloc(sizeof(*data));
                if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); }
                else {
                    *data=(QueueSchema){queueIdentity,layout,policy};
                    policy=(XL6_SequenceAdmissionV3){0};
                    XL6_ResidentReferenceSchemaV1 schema={&schemaOps,data,queueIdentity,depth};
                    result=xl6SequenceResidentReferenceCreateTakeV1(role,identity,element,&schema,nullable);
                }
            }
        }
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&policy); xrtValueRelease(seed);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); xrtValueRelease(result); return NULL; }
    return result;
}
