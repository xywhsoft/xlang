#include "src/runtime/xl6_persistence_project.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

static void dropFailed(xvalue* value)
{
    if (!value) return;
    xerror* failure=xrtTakeError();
    xl6_error_scope cleanup={0};
    if (!xl6ErrorScopeBegin(&cleanup)) abort();
    xrtSetErrorTake(failure); xl6ErrorScopeCaptureFailure(&cleanup);
    xrtValueRelease(value);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
}

static bool viewValid(xstrview key) { return key.Data || !key.Size; }
static bool planValid(const XL6_PersistencePlanV1* plan)
{
    if (!plan || plan->size!=sizeof(*plan) || (plan->schemaCount && !plan->schemas) ||
        plan->schemaCount>SIZE_MAX/sizeof(*plan->schemas) ||
        plan->sequenceCount>SIZE_MAX/sizeof(*plan->sequences) ||
        (plan->sequenceCount && !plan->sequences)) return false;
    for(size_t i=0;i<plan->sequenceCount;++i) {
        const xvaluegraphsequencev1* sequence=&plan->sequences[i];
        if(!sequence->TypeId || (sequence->Type!=XVALUE_INT_MAP && sequence->Type!=XVALUE_SET)) return false;
        for(size_t j=0;j<i;++j) if(plan->sequences[j].TypeId==sequence->TypeId) return false;
        for(size_t j=0;j<plan->schemaCount;++j) if(plan->schemas[j].identity==sequence->TypeId) return false;
    }
    for (size_t i=0;i<plan->schemaCount;++i) {
        const XL6_PersistenceSchemaV1* schema=&plan->schemas[i];
        if (!schema->identity || (schema->flags & ~XL6_PERSIST_SCHEMA_DISABLED_V1) ||
            schema->fieldCount>SIZE_MAX/sizeof(*schema->fields) ||
            (schema->fieldCount && !schema->fields)) return false;
        for (size_t j=0;j<i;++j) if (plan->schemas[j].identity==schema->identity) return false;
        if (schema->flags && (schema->fieldCount || schema->open)) return false;
        if (schema->open && (!plan->projectExtras || !schema->open->ops ||
            !viewValid(schema->open->storage) || !viewValid(schema->open->handle))) return false;
        for (size_t j=0;j<schema->fieldCount;++j) {
            const XL6_PersistenceFieldV1* field=&schema->fields[j];
            if (!viewValid(field->source) || !viewValid(field->target) ||
                (field->flags & ~(XL6_PERSIST_OMIT_NULL_V1|XL6_PERSIST_OMIT_EMPTY_V1))) return false;
        }
    }
    return true;
}

static int projectObject(uint64_t identity, xvaluegraphnextv1* next,
    xvalue** output, ptr user)
{
    const XL6_PersistencePlanV1* plan=user;
    const XL6_PersistenceSchemaV1* schema=NULL;
    for (size_t i=0;i<plan->schemaCount;++i)
        if (plan->schemas[i].identity==identity) { schema=&plan->schemas[i]; break; }
    if (!schema) {
        /* Dictionaries, including nominal generic dictionaries, retain their
         * ordinary full graph semantics. A language class requires an exact
         * format schema; the marker is never a schema-dispatch identity. */
        const xvalue* marker=xrtValueGraphObjectGetV1(next,XRT_STR_LITERAL("$xlir.class.runtime-type"));
        if (!marker && !xrtGetError()) return 0;
        if (!xrtGetError()) xrtSetErrorKind(XERR_UNSUPPORTED);
        return -1;
    }
    if (schema->flags & XL6_PERSIST_SCHEMA_DISABLED_V1) {
        xrtSetErrorKind(XERR_UNSUPPORTED); return -1;
    }
    xvalue* result=xrtValueObject();
    if (!result) return -1;
    if (!xrtValueReserve(result,schema->fieldCount) ||
        (schema->open && !plan->projectExtras(schema->open,next,result))) goto failure;
    for (size_t i=0;i<schema->fieldCount;++i) {
        const XL6_PersistenceFieldV1* field=&schema->fields[i];
        const xvalue* borrowed=xrtValueGraphObjectGetV1(next,field->source);
        if (!borrowed && xrtGetError()) goto failure;
        xvalue* item=xrtValueGraphNextV1(next,borrowed ? borrowed : xrtValueNull());
        if (!item) goto failure;
        bool omitted=(field->flags & XL6_PERSIST_OMIT_EMPTY_V1) ? !xrtValueTruthy(item) :
            (field->flags & XL6_PERSIST_OMIT_NULL_V1) && xrtValueType(item)==XVALUE_NULL;
        if (!omitted && !xrtValueObjectSetTake(result,field->target,&item)) {
            dropFailed(item); goto failure;
        }
        xrtValueRelease(item);
        if (xrtGetError()) goto failure;
    }
    *output=result; return 1;
failure:
    dropFailed(result); return -1;
}

xvalue* xl6PersistenceProjectV1(const xvalue* source, const XL6_PersistencePlanV1* plan)
{
    if (!planValid(plan)) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    return xl6ValueDataProjectV1(source,projectObject,(ptr)plan,plan->sequences,plan->sequenceCount);
}
