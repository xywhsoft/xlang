#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_native_class_result.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
static bool valid(XL6_NativeClassResultContextV1 c)
{
    const XL6_ClassIdentitySchemaV3* s=c.schema;const XL6_NativeHandlePolicyV1* p=c.field.policy;
    return s&&s->size==sizeof(*s)&&s->identity&&s->runtimeTypeTag&&
        s->hashMode==XL6_CLASS_IDENTITY_REFERENCE_V3&&s->equalMode==XL6_CLASS_IDENTITY_REFERENCE_V3&&
        !s->fieldCount&&!s->fields&&!s->dynamicFields.storageName&&!s->dynamicFields.storageSize&&
        !s->dynamicFields.handleName&&!s->dynamicFields.handleSize&&c.field.fieldName&&c.field.fieldSize&&
        c.field.fieldSize<=INT64_MAX&&(c.field.fieldSize<6||memcmp(c.field.fieldName,"$xlir.",6))&&
        p&&p->size==sizeof(*p)&&p->abiTag==XL6_NATIVE_HANDLE_POLICY_V1_TAG&&p->Ops&&p->Trace&&
        p->Ops->Drop&&!p->Ops->Clone&&!p->Ops->Hash&&!p->Ops->Equal;
}
static xvalue* retire(xvalue* object,void* raw,const XL6_NativeHandlePolicyV1* policy)
{
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
    xrtValueRelease(object);if(raw)policy->Ops->Drop(raw,NULL);
    xl6_error_result result=xl6ErrorScopeEnd(&cleanup);xrtSetErrorTake(result.error);return NULL;
}
bool xl6NativeClassResultContextValidV1(XL6_NativeClassResultContextV1 c)
{if(valid(c))return true;xrtSetErrorKind(XERR_ARGUMENT);return false;}
static xvalue* prepare(XL6_NativeClassResultContextV1 c,XL6_CodeLease* parent,bool derived)
{
    if(!valid(c)){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xvalue* object=derived?xl6ClassCreateFromLeaseV3(parent,c.schema):xl6ClassRuntimeV3Get()->Create(c.codeOwner,NULL,c.schema);if(!object)return NULL;
    if(!xrtValueTypeIdBind(object,c.schema->identity))return retire(object,NULL,c.field.policy);
    object=xl6ClassRuntimeV3Get()->BindIdentityTake(object);if(!object)return NULL;
    xvalue* tag=xrtValueInt(c.schema->runtimeTypeTag);if(!tag)return retire(object,NULL,c.field.policy);
    if(!xrtValueObjectSetNewPostCommit(object,(xstrview){"$xlir.class.runtime-type",24},tag))
        return retire(object,NULL,c.field.policy);
    return object;
}
xvalue* xl6NativeClassResultPrepareV1(XL6_NativeClassResultContextV1 c){return prepare(c,NULL,false);}
xvalue* xl6NativeClassResultPrepareFromLeaseV1(XL6_NativeClassResultContextV1 c,XL6_CodeLease* parent){return prepare(c,parent,true);}
xvalue* xl6NativeClassResultCommitTakeV1(XL6_NativeClassResultContextV1 c,xvalue** prepared,void** slot)
{
    if(!valid(c)||!prepared||!slot){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xvalue* object=*prepared;void* raw=*slot;*prepared=NULL;*slot=NULL;
    xstrview field={c.field.fieldName,c.field.fieldSize};
    xvalue* tag=object?xrtValueObjectGet(object,(xstrview){"$xlir.class.runtime-type",24}):NULL;int64 runtimeTag=0;
    if(!object||!raw||xrtValueTypeId(object)!=c.schema->identity||!tag||!xrtValueGetInt(tag,&runtimeTag)||
        runtimeTag!=c.schema->runtimeTypeTag||xrtValueObjectGet(object,field)){
        xrtSetErrorKind(XERR_STATE);return retire(object,raw,c.field.policy);
    }
    xvalue* handle=xl6NativeHandleTakeV1(&raw,c.field.policy);
    if(!handle)return retire(object,raw,c.field.policy);
    if(!xrtValueObjectSetNewPostCommit(object,field,handle))return retire(object,NULL,c.field.policy);
    return xl6ClassRuntimeV3Get()->CommitTake(object);
}
